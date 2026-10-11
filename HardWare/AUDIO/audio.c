/* audio.c -- hardware-PWM audio engine (dual channel)
   hardware: ePWM3, one module + two outputs
             GPIO4 = EPWM3A = CMPA (true addr 0x6889) -> ch0 = right hand
             GPIO5 = EPWM3B = CMPB (true addr 0x688A) -> ch1 = left hand
             carrier 25 kHz (TBPRD 5999); ONE ISR feeds both -> never drift
   tables  : ch0 = s_song_right + s_pitch_hz   (native, 175..2637 Hz)
             ch1 = s_song_left  + s_pitch_hz_l (+12 semitones, 78..1245 Hz)
   PITFALLS: (1) GPIO MUX/DIR/PUD writes need EALLOW/EDIS, else silently ignored
                 (pin stays 0 V = completely silent)
             (2) EPWM2 base is 0x6840: 0x6869 hits EPWM2 CMPA, not EPWM3
             (3) DMA cannot write peripheral registers on this chip
             (4) GPIO4/GPIO5 are also LCD DB2/DB3 => no LCD writes while playing */
#include "DSP2833x_Device.h"
#include "DSP2833x_Examples.h"
#include "audio.h"
#include "key.h"
#include "song_tables.h"

#define AUDIO_TBPRD     5999U                       /* 150MHz/6000 = 25kHz */
#define AUDIO_SR        25000UL
#define RING_LEN        1024U                       /* 2^n；双声道各 1024 字 = 41ms，够主循环追 */
#define RING_MASK       (RING_LEN - 1U)
#define CMP_CENTER      3000U                       /* 50% = 静音（25kHz 载波） */
#define AUDIO_CMPA      (*(volatile Uint16 *)0x6889UL)   /* 真·CMPA（EPWM3 基址 0x6880 + 9）*/
#define AUDIO_CMPB      (*(volatile Uint16 *)0x688AUL)   /* 真·CMPB（+10）*/
#define AUDIO_CH        2U                          /* 0 = 右手(GPIO4) / 1 = 左手(GPIO5) */

static U16 s_ring[AUDIO_CH][RING_LEN];
static volatile U16 s_head[AUDIO_CH];               /* ISR 消费位置 */
static U16 s_tail[AUDIO_CH];                        /* 主循环生产位置 */
static volatile U16 s_playing = 0U;

/* ---------------- 正弦波表（64 点）---------------- */
static const S16 s_sine[64] = {
        0,  3212,  6393,  9512, 12539, 15446, 18204, 20787,
    23170, 25329, 27245, 28898, 30273, 31356, 32137, 32609,
    32767, 32609, 32137, 31356, 30273, 28898, 27245, 25329,
    23170, 20787, 18204, 15446, 12539,  9512,  6393,  3212,
        0, -3212, -6393, -9512,-12539,-15446,-18204,-20787,
   -23170,-25329,-27245,-28898,-30273,-31356,-32137,-32609,
   -32767,-32609,-32137,-31356,-30273,-28898,-27245,-25329,
   -23170,-20787,-18204,-15446,-12539, -9512, -6393, -3212
};

/* ---------------- 音高索引 → 频率(Hz) ----------------
   索引 1..N：声道0=右手（s_song_right，base 52 ⇒ MIDI 53..100，不移调）
              声道1=左手（s_song_left ，base 27 ⇒ MIDI 27..75 再 +24 ⇒ 51..99）*/
static const U16 s_pitch_hz[49] = {          /* 右手：0,175,185,...,2489,2637 */
    0U,
     175U, 185U, 196U, 208U, 220U, 233U, 247U, 262U, 277U, 294U, 311U, 330U,
     349U, 370U, 392U, 415U, 440U, 466U, 494U, 523U, 554U, 587U, 622U, 659U,
     698U, 740U, 784U, 831U, 880U, 932U, 988U,1047U,1109U,1175U,1245U,1319U,
    1397U,1480U,1568U,1661U,1760U,1865U,1976U,2093U,2218U,2349U,2489U,2637U
};

static const U16 s_pitch_hz_l[50] = {        /* 左手：+12 半音（+1 八度）⇒ MIDI 39..87 = 78..1245 Hz */
    0U,
      78U,  82U,  87U,  92U,  98U, 104U, 110U, 117U, 123U, 131U, 139U, 147U,
     156U, 165U, 175U, 185U, 196U, 208U, 220U, 233U, 247U, 262U, 277U, 294U,
     311U, 330U, 349U, 370U, 392U, 415U, 440U, 466U, 494U, 523U, 554U, 587U,
     622U, 659U, 698U, 740U, 784U, 831U, 880U, 932U, 988U,1047U,1109U,1175U,1245U
};
typedef struct { U32 phase; U32 inc; U16 amp; U16 decay; U16 active; } AUDIO_VOICE;
static AUDIO_VOICE s_v[AUDIO_CH][AUDIO_VOICES];
static U16 s_decim[AUDIO_CH];
static U16 s_gain[AUDIO_CH];        /* 1/n 定点（按活跃声部数归一化）*/
/* 声道增益：左手降了 1 个八度，小喇叭低频效率低（约 -6dB），这里补回来 */
static const U16 s_chgain[AUDIO_CH] = { 256U, 512U };   /* 0=右手 x1.0 ; 1=左手 x2.0 */

static U16 s_pk_i[AUDIO_CH];        /* 打包表字下标（每事件 +2）*/
static U32 s_left[AUDIO_CH];        /* 当前事件剩余采样数 */
static U16 s_fmax[AUDIO_CH];        /* 本声道最近事件内的最高频率(Hz) */

volatile U16 g_audio_events  = 0U;
volatile U16 g_audio_freq    = 0U;  /* 两声道合并的最高频率：LED 灯效跟随用 */
volatile U32 g_audio_samples = 0UL;

/* ---------------- ISR：每载波周期各搬一个采样 ---------------- */
interrupt void audio_isr(void)
{
    if (s_head[0] != s_tail[0]) {                   /* 声道0 → CMPA → GPIO4 */
        AUDIO_CMPA = s_ring[0][s_head[0]];
        s_head[0] = (U16)((s_head[0] + 1U) & RING_MASK);
        g_audio_samples++;
    }
    if (s_head[1] != s_tail[1]) {                   /* 声道1 → CMPB → GPIO5 */
        AUDIO_CMPB = s_ring[1][s_head[1]];
        s_head[1] = (U16)((s_head[1] + 1U) & RING_MASK);
    }
    EPwm3Regs.ETCLR.bit.INT = 1;
    PieCtrlRegs.PIEACK.bit.ACK3 = 1;                /* 组3（EPWM3_INT = INT3.3）*/
}

static U16 decay_coeff(U16 tc_ms)
{
    float c = 1.0f - 64.0f / ((float)tc_ms * 0.001f * (float)AUDIO_SR);
    if (c < 0.0f) { c = 0.0f; }
    return (U16)(c * 32767.0f);
}

static U32 freq_inc(U16 f)
{
    return (U32)((float)f * (4294967296.0f / (float)AUDIO_SR));
}

/* 合成一个采样（ch = 0/1）*/
static U16 synth_duty(U16 ch)
{
    S32 mix = 0;
    S32 duty;
    U16 v;

    for (v = 0U; v < AUDIO_VOICES; v++) {
        if (s_v[ch][v].active != 0U) {
            s_v[ch][v].phase += s_v[ch][v].inc;
            mix += ((S32)s_sine[(s_v[ch][v].phase >> 26) & 0x3FU] * (S32)s_v[ch][v].amp) >> 15;
        }
    }
    s_decim[ch]++;
    if (s_decim[ch] >= 64U) {
        U16 n = 0U;
        s_decim[ch] = 0U;
        for (v = 0U; v < AUDIO_VOICES; v++) { if (s_v[ch][v].active != 0U) { n++; } }
        if (n == 0U) { n = 1U; }
        s_gain[ch] = (U16)(256U / n);          /* n=1⇒256, 2⇒128, 3⇒85, 4⇒64 ... */
        for (v = 0U; v < AUDIO_VOICES; v++) {
            if (s_v[ch][v].active != 0U) {
                s_v[ch][v].amp = (U16)(((U32)s_v[ch][v].amp * (U32)s_v[ch][v].decay) >> 15);
                if (s_v[ch][v].amp < 8U) { s_v[ch][v].active = 0U; }
            }
        }
    }
    mix = (mix * (S32)s_gain[ch]) >> 8;         /* 按声部数归一化（统一音量）*/
    mix = (mix * (S32)s_chgain[ch]) >> 8;       /* ★ 声道增益（左手补偿低频损失）*/
    {
        S32 m = (mix >> 1) + (mix >> 2);        /* 3/4 满量程：±3000（中心 3000）*/
        S32 lim = (S32)(CMP_CENTER - 20U);      /* 线性区 ±2980 */
        if (m >  lim) { m =  lim + ((m - lim) >> 4); }   /* 软限幅 */
        if (m < -lim) { m = -lim + ((m + lim) >> 4); }
        duty = (S32)CMP_CENTER + m;
    }
    if (duty < 2) { duty = 2; }
    if (duty > (S32)(AUDIO_TBPRD - 2U)) { duty = (S32)(AUDIO_TBPRD - 2U); }
    return (U16)duty;
}

static void release_all(U16 ch)
{
    U16 v;
    for (v = 0U; v < AUDIO_VOICES; v++) {
        if (s_v[ch][v].active != 0U) { s_v[ch][v].decay = decay_coeff(260U); }
    }
}

static void start_chord(U16 ch, const U16 *freq)
{
    U16 i, v;

    release_all(ch);
    for (i = 0U; i < AUDIO_MAX_CHORD; i++) {
        if (freq[i] == 0U) { continue; }
        v = AUDIO_VOICES;
        {
            U16 k, best = 0U, best_amp = 0xFFFFU;
            for (k = 0U; k < AUDIO_VOICES; k++) {
                if (s_v[ch][k].active == 0U) { v = k; break; }
                if (s_v[ch][k].amp < best_amp) { best_amp = s_v[ch][k].amp; best = k; }
            }
            if (v == AUDIO_VOICES) { v = best; }
        }
        s_v[ch][v].phase = 0UL;
        s_v[ch][v].inc   = freq_inc(freq[i]);
        s_v[ch][v].amp   = AUDIO_AMP_PEAK;
        s_v[ch][v].decay = decay_coeff(900U);
        s_v[ch][v].active = 1U;
    }
}

/* 取下一个事件：ch=0 用右手表，ch=1 用左手表（含各自的频率表）*/
static U16 next_event(U16 ch)
{
    const U16 *tbl   = (ch == 0U) ? s_song_right : s_song_left;
    const U16 *pitch = (ch == 0U) ? s_pitch_hz   : s_pitch_hz_l;
    U16 w0 = tbl[s_pk_i[ch]];
    U16 w1 = tbl[s_pk_i[ch] + 1U];
    U16 f[4];
    U16 k, v;

    if ((w0 == 0U) && (w1 == 0U)) { return 0U; }        /* 表尾 (0,0) */
    s_pk_i[ch] = (U16)(s_pk_i[ch] + 2U);

    k = 0U;                                   /* 6bit 跨字打包：v0|v1|v2低位 / v2高位|v3|时值 */
    v = (U16)( w0        & 0x3FU); f[k] = ((v >= 1U) && (v <= 49U)) ? pitch[v] : 0U; k++;
    v = (U16)((w0 >> 6)  & 0x3FU); f[k] = ((v >= 1U) && (v <= 49U)) ? pitch[v] : 0U; k++;
    v = (U16)(((w0 >> 12) & 0x0FU) | ((w1 & 0x03U) << 4));
    f[k] = ((v >= 1U) && (v <= 49U)) ? pitch[v] : 0U; k++;
    v = (U16)((w1 >> 2)  & 0x3FU); f[k] = ((v >= 1U) && (v <= 49U)) ? pitch[v] : 0U;

    {   /* 本声道本事件的最高频率；两声道再取 max 供 LED 使用 */
        U16 k2;
        s_fmax[ch] = 0U;
        for (k2 = 0U; k2 < 4U; k2++) { if (f[k2] > s_fmax[ch]) { s_fmax[ch] = f[k2]; } }
        g_audio_freq = (s_fmax[0] > s_fmax[1]) ? s_fmax[0] : s_fmax[1];
    }

    start_chord(ch, f);
    s_left[ch] = (U32)((w1 >> 8) & 0xFFU) * (AUDIO_SR / 12UL);   /* 时值：250/3ms ⇒ u*SR/12 */
    g_audio_events++;
    return 1U;
}

void audio_init(void)
{
    U16 ch, i;

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    EPwm3Regs.TBCTL.bit.SYNCOSEL  = 3;
    EPwm3Regs.TBCTL.bit.PHSEN     = 0;
    EPwm3Regs.TBPHS.half.TBPHS    = 0;
    EPwm3Regs.TBCTR               = 0;
    EPwm3Regs.TBPRD               = AUDIO_TBPRD;
    EPwm3Regs.TBCTL.bit.CTRMODE   = 0;
    EPwm3Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm3Regs.TBCTL.bit.CLKDIV    = 0;
    EPwm3Regs.CMPCTL.bit.SHDWAMODE = 0;
    EPwm3Regs.CMPCTL.bit.LOADAMODE = 0;
    EPwm3Regs.CMPCTL.bit.SHDWBMODE = 0;
    EPwm3Regs.CMPCTL.bit.LOADBMODE = 0;
    AUDIO_CMPA                     = CMP_CENTER;
    AUDIO_CMPB                     = CMP_CENTER;
    EPwm3Regs.AQCTLA.bit.ZRO      = 2;            /* A: set   on zero     */
    EPwm3Regs.AQCTLA.bit.CAU      = 1;            /* A: clear on CMPA up  */
    EPwm3Regs.AQCTLB.bit.ZRO      = 2;            /* B: set   on zero     */
    EPwm3Regs.AQCTLB.bit.CBU      = 1;            /* B: clear on CMPB up  */
    EPwm3Regs.ETSEL.bit.INTSEL    = 1;            /* ET_CTR_ZERO */
    EPwm3Regs.ETSEL.bit.INTEN     = 1;
    EPwm3Regs.ETPS.bit.INTPRD     = 1;            /* ET_1ST */

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    PieVectTable.EPWM3_INT = &audio_isr;          /* EPWM3_INT = INT3.3 */
    EDIS;
    PieCtrlRegs.PIEIER3.bit.INTx3 = 1;
    PieCtrlRegs.PIEACK.bit.ACK3 = 1;
    IER |= M_INT3;
    EINT;
    ERTM;

    for (ch = 0U; ch < AUDIO_CH; ch++) {
        for (i = 0U; i < AUDIO_VOICES; i++) { s_v[ch][i].active = 0U; }
        s_fmax[ch] = 0U;
    }
}

void audio_stop(void)
{
    U16 ch, i;

    EPwm3Regs.TBCTL.bit.CTRMODE = 3;              /* freeze */
    AUDIO_CMPA = CMP_CENTER;
    AUDIO_CMPB = CMP_CENTER;
    PieCtrlRegs.PIEIER3.bit.INTx3 = 0;

    EALLOW;                                       /* GPIO4/5 交还 LCD(DB2/DB3) */
    GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 0;
    EDIS;
    GpioCtrlRegs.GPADIR.bit.GPIO4 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO5 = 1;
    GpioDataRegs.GPACLEAR.bit.GPIO4 = 1;
    GpioDataRegs.GPACLEAR.bit.GPIO5 = 1;

    for (ch = 0U; ch < AUDIO_CH; ch++) {
        for (i = 0U; i < AUDIO_VOICES; i++) { s_v[ch][i].active = 0U; }
        s_fmax[ch] = 0U;
        s_playing = 0U;
    }
    g_audio_freq = 0U;
    s_playing = 0U;
}

U16 audio_is_playing(void)
{
    return s_playing;
}

void audio_start(void)
{
    U16 ch, i;

    /* ★ 播放期间接管 GPIO4 = EPWM3A、GPIO5 = EPWM3B
       （GPIO 复用寄存器受 EALLOW 保护，必须包起来）*/
    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO4 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO4  = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO4  = 0;
    GpioCtrlRegs.GPAMUX1.bit.GPIO5 = 1;
    GpioCtrlRegs.GPADIR.bit.GPIO5  = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO5  = 0;
    EDIS;

    if (s_playing != 0U) { return; }

    for (ch = 0U; ch < AUDIO_CH; ch++) {
        s_pk_i[ch] = 0U;
        s_left[ch] = 0UL;
        s_head[ch] = 0U;
        s_tail[ch] = 0U;
        s_decim[ch] = 0U;
        s_gain[ch] = 256U;
        s_fmax[ch] = 0U;
        for (i = 0U; i < RING_LEN; i++) { s_ring[ch][i] = CMP_CENTER; }
    }
    g_audio_events = 0U;
    g_audio_samples = 0UL;
    g_audio_freq = 0U;

    /* 先各合成约 1/4 缓冲，再开载波，避免开头欠载 */
    for (ch = 0U; ch < AUDIO_CH; ch++) {
        for (i = 0U; i < (RING_LEN / 4U); i++) {
            if (s_left[ch] == 0UL) { if (next_event(ch) == 0U) { break; } }
            s_ring[ch][s_tail[ch]] = synth_duty(ch);
            s_tail[ch] = (U16)((s_tail[ch] + 1U) & RING_MASK);
            s_left[ch]--;
        }
    }

    audio_init();
    s_playing = 1U;
}

void audio_task(void)
{
    U16 ch;
    U16 guard = 0U;

    if (s_playing == 0U) { return; }

    for (ch = 0U; ch < AUDIO_CH; ch++) {
        guard = 0U;
        /* 追着 ISR 读指针填（留 1 格空以区分空/满）*/
        while ((((U16)(s_tail[ch] + 1U)) & RING_MASK) != s_head[ch] && guard < RING_LEN) {
            if (s_left[ch] == 0UL) {
                if (next_event(ch) == 0U) {        /* 曲表播完 */
                    s_pk_i[ch] = 0U;               /* ⇒ 从头循环，不停 */
                    if (next_event(ch) == 0U) {    /* 空表保护 */
                        audio_stop();
                        return;
                    }
                }
            }
            s_ring[ch][s_tail[ch]] = synth_duty(ch);
            s_tail[ch] = (U16)((s_tail[ch] + 1U) & RING_MASK);
            s_left[ch]--;
            guard++;
        }
    }
}
