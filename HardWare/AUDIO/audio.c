/* ============================================================================
 *  HardWare/AUDIO/audio.c —— 硬件 PWM 音频引擎（ePWM4 周期中断 + 环形缓冲）
 * ----------------------------------------------------------------------------
 *  为什么不用 DMA：
 *      F2833x 的 DMA 只能访问 RAM/XINTF（官方 3 个 DMA 例程目标全是 RAM），
 *      写外设寄存器无效 ⇒ "DMA→CMPA" 在这颗芯片上不可行。
 *  本实现（全部已在硬件上验证）：
 *      · ePWM4A (GPIO6) 出 150kHz 载波，TBPRD = 999（150MHz/1000）
 *      · 每周期产生中断（EPWM4_INT = INT3.4），ISR 从环形缓冲取一个采样写 CMPA
 *      · CMPA 必须用真地址 0x68C9：vendor 的 &EPwm4Regs.CMPA 会算成 0x68C8 ✗
 *      · ISR 约 40~60 周期 / 800 周期 ⇒ 约 7% CPU
 *      · 主循环 audio_task() 追着 ISR 的读指针合成（正弦表+包络+和弦），约 12%
 *  参考：Example14_DSP2833x_Epwm_Time_Interrupt（官方能跑的中断例程）
 * ========================================================================== */
#include "DSP2833x_Device.h"
#include "DSP2833x_GlobalPrototypes.h"
#include "audio.h"
#include "delay.h"
#include "key.h"
#include "song_tables.h"     /* 打包曲表：每事件 2 字（4 字节）= 3.4KB ✓ 原经典表 7.3KB */

/* 音高索引 1..24 = C6..B7（值 = MIDI-83），与生成脚本 note_hz 公式一致 */
/* 音高索引 1..49 = MIDI 27..75（v = MIDI - SONG_L_BASE(27) + 1，6bit 无折叠）*/
/* 播放用表：音高索引 1..49 + 整体移调 +24 半音（+2 八度）
   原始 MIDI 27..75 → 实际发声 MIDI 51..99 = 156..2489 Hz
   移调只为适配压电片的可闻频段（不改音程 ⇒ 旋律精确）*/
/* 播放用表：右手曲表 s_song_right，v = MIDI - SONG_R_BASE(52) + 1 (1..48)
   音域 MIDI 53..100 = 175..2637 Hz（未移调：本身就是压电片的最佳频段）*/
static const U16 s_pitch_hz[49] = {
    0U,
     175U, 185U, 196U, 208U, 220U, 233U, 247U, 262U, 277U, 294U, 311U, 330U,
     349U, 370U, 392U, 415U, 440U, 466U, 494U, 523U, 554U, 587U, 622U, 659U,
     698U, 740U, 784U, 831U, 880U, 932U, 988U,1047U,1109U,1175U,1245U,1319U,
    1397U,1480U,1568U,1661U,1760U,1865U,1976U,2093U,2218U,2349U,2489U,2637U
};


#define AUDIO_TBPRD     5999U                       /* 150MHz/6000 = 25kHz */
#define AUDIO_SR        25000UL
#define RING_LEN        2048U                     /* 2^n；13.6ms，够主循环追 */
#define RING_MASK       (RING_LEN - 1U)
#define CMP_CENTER      3000U                      /* 50% = 静音（25kHz 载波） */
#define AUDIO_CMPA      (*(volatile Uint16 *)0x68C9UL)   /* 真·CMPA */

static U16 s_ring[RING_LEN];
static volatile U16 s_head = 0U;                  /* ISR 消费位置 */
static U16 s_tail = 0U;                           /* 主循环生产位置 */
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

typedef struct { U32 phase; U32 inc; U16 amp; U16 decay; U16 active; } AUDIO_VOICE;
static AUDIO_VOICE s_v[AUDIO_VOICES];
static U16 s_decim = 0U;
static U16 s_gain  = 256U;   /* 1/n 的定点表示（按活跃声部数归一化，单音与和弦同响度）*/

static U16 s_pk_i = 0U;   /* 打包表字下标（每事件 +2）*/
static U32 s_left = 0UL;

volatile U16 g_audio_events  = 0U;
volatile U16 g_audio_freq    = 0U;       /* 当前所有声部中的最高频率(Hz)：LED 灯效跟随用 */
volatile U32 g_audio_samples = 0UL;

/* ---------------- ISR：每载波周期搬一个采样到 CMPA ---------------- */
interrupt void audio_isr(void)
{
    if (s_head != s_tail) {
        AUDIO_CMPA = s_ring[s_head];
        s_head = (U16)((s_head + 1U) & RING_MASK);
        g_audio_samples++;
    }
    EPwm4Regs.ETCLR.bit.INT = 1;
    PieCtrlRegs.PIEACK.bit.ACK3 = 1;              /* 组3（EPWM4_INT = INT3.4）*/
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

static U16 synth_duty(void)
{
    S32 mix = 0;
    S32 duty;
    U16 v;

    for (v = 0U; v < AUDIO_VOICES; v++) {
        if (s_v[v].active != 0U) {
            s_v[v].phase += s_v[v].inc;
            mix += ((S32)s_sine[(s_v[v].phase >> 26) & 0x3FU] * (S32)s_v[v].amp) >> 15;
        }
    }
    s_decim++;
    if (s_decim >= 64U) {
        U16 n = 0U;
        s_decim = 0U;
        for (v = 0U; v < AUDIO_VOICES; v++) { if (s_v[v].active != 0U) { n++; } }
        if (n == 0U) { n = 1U; }
        s_gain = (U16)(256U / n);          /* n=1⇒256, 2⇒128, 3⇒85, 4⇒64 ... */
        for (v = 0U; v < AUDIO_VOICES; v++) {
            if (s_v[v].active != 0U) {
                s_v[v].amp = (U16)(((U32)s_v[v].amp * (U32)s_v[v].decay) >> 15);
                if (s_v[v].amp < 8U) { s_v[v].active = 0U; }
            }
        }
    }
    {
        mix = (mix * (S32)s_gain) >> 8;      /* 按声部数归一化（统一音量）*/
        S32 m = (mix >> 1) + (mix >> 2);   /* 3/4 满量程：±3000（中心3000，限幅 ±2980）*/
        S32 lim = (S32)(CMP_CENTER - 20U);   /* 线性区 ±2980 */
        if (m >  lim) { m =  lim + ((m - lim) >> 4); }   /* 软限幅：超出部分压缩 16 倍 */
        if (m < -lim) { m = -lim + ((m + lim) >> 4); }
        duty = (S32)CMP_CENTER + m;
    }
    if (duty < 2) { duty = 2; }
    if (duty > (S32)(AUDIO_TBPRD - 2U)) { duty = (S32)(AUDIO_TBPRD - 2U); }
    return (U16)duty;
}

static void release_all(void)
{
    U16 v;
    for (v = 0U; v < AUDIO_VOICES; v++) {
        if (s_v[v].active != 0U) { s_v[v].decay = decay_coeff(260U); }
    }
}

static void start_chord(const U16 *freq)
{
    U16 i, v;

    release_all();
    for (i = 0U; i < AUDIO_MAX_CHORD; i++) {
        if (freq[i] == 0U) { continue; }
        v = AUDIO_VOICES;
        {
            U16 k, best = 0U, best_amp = 0xFFFFU;
            for (k = 0U; k < AUDIO_VOICES; k++) {
                if (s_v[k].active == 0U) { v = k; break; }
                if (s_v[k].amp < best_amp) { best_amp = s_v[k].amp; best = k; }
            }
            if (v == AUDIO_VOICES) { v = best; }
        }
        s_v[v].phase = 0UL;
        s_v[v].inc   = freq_inc(freq[i]);
        s_v[v].amp   = AUDIO_AMP_PEAK;
        s_v[v].decay = decay_coeff(900U);
        s_v[v].active = 1U;
    }
}

static U16 next_event(void)
{
    U16 w0 = s_song_right[s_pk_i];
    U16 w1 = s_song_right[s_pk_i + 1U];
    U16 f[4];
    U16 k, v;

    if ((w0 == 0U) && (w1 == 0U)) { return 0U; }        /* 表尾 (0,0) */
    s_pk_i = (U16)(s_pk_i + 2U);

    k = 0U;
    v = (U16)( w0        & 0x3FU); f[k] = ((v >= 1U) && (v <= 48U)) ? s_pitch_hz[v] : 0U; k++;
    v = (U16)((w0 >> 6)  & 0x3FU); f[k] = ((v >= 1U) && (v <= 48U)) ? s_pitch_hz[v] : 0U; k++;
    v = (U16)((w0 >> 12) & 0x3FU); f[k] = ((v >= 1U) && (v <= 48U)) ? s_pitch_hz[v] : 0U; k++;
    v = (U16)( w1        & 0x3FU); f[k] = ((v >= 1U) && (v <= 48U)) ? s_pitch_hz[v] : 0U;
    {   /* LED 跟随：记录本事件 4 个声部中的最高频率 */
        U16 k2; g_audio_freq = 0U;
        for (k2 = 0U; k2 < 4U; k2++) { if (f[k2] > g_audio_freq) { g_audio_freq = f[k2]; } }
    }

    start_chord(f);
    g_audio_events++;
    s_left = (U32)((w1 >> 6) & 0xFFU) * (AUDIO_SR / 12UL);      /* 时值单位 = 250/3ms ⇒ 采样 = u*SR/12 ✓ */
    return 1U;
}

void audio_init(void)
{
    U16 i;

    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 1;           /* GPIO6 = EPWM4A */
    GpioCtrlRegs.GPADIR.bit.GPIO6  = 1;
    GpioCtrlRegs.GPAPUD.bit.GPIO6  = 0;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 0;
    EDIS;

    EPwm4Regs.TBCTL.bit.SYNCOSEL  = 3;
    EPwm4Regs.TBCTL.bit.PHSEN     = 0;
    EPwm4Regs.TBPHS.half.TBPHS    = 0;
    EPwm4Regs.TBCTR               = 0;
    EPwm4Regs.TBPRD               = AUDIO_TBPRD;
    EPwm4Regs.TBCTL.bit.CTRMODE   = 0;
    EPwm4Regs.TBCTL.bit.HSPCLKDIV = 0;
    EPwm4Regs.TBCTL.bit.CLKDIV    = 0;
    EPwm4Regs.CMPCTL.bit.SHDWAMODE = 0;
    EPwm4Regs.CMPCTL.bit.LOADAMODE = 0;
    AUDIO_CMPA                     = CMP_CENTER;
    EPwm4Regs.AQCTLA.bit.ZRO      = 2;            /* AQ_SET   on zero    */
    EPwm4Regs.AQCTLA.bit.CAU      = 1;            /* AQ_CLEAR on CMPA up */
    EPwm4Regs.ETSEL.bit.INTSEL    = 1;            /* ET_CTR_ZERO */
    EPwm4Regs.ETSEL.bit.INTEN     = 1;
    EPwm4Regs.ETPS.bit.INTPRD     = 1;            /* ET_1ST */

    EALLOW;
    SysCtrlRegs.PCLKCR0.bit.TBCLKSYNC = 1;
    PieVectTable.EPWM4_INT = &audio_isr;          /* EPWM4_INT = INT3.4 */
    EDIS;
    PieCtrlRegs.PIEIER3.bit.INTx4 = 1;
    PieCtrlRegs.PIEACK.bit.ACK3 = 1;
    IER |= M_INT3;
    EINT;
    ERTM;

    for (i = 0U; i < AUDIO_VOICES; i++) { s_v[i].active = 0U; }
}

void audio_stop(void)
{
    U16 i;

    EPwm4Regs.TBCTL.bit.CTRMODE = 3;              /* freeze */
    AUDIO_CMPA = CMP_CENTER;
    PieCtrlRegs.PIEIER3.bit.INTx4 = 0;

    EALLOW;
    GpioCtrlRegs.GPAMUX1.bit.GPIO6 = 0;           /* GPIO6 交还 LCD(DB4) */
    EDIS;
    GpioDataRegs.GPACLEAR.bit.GPIO6 = 1;

    for (i = 0U; i < AUDIO_VOICES; i++) { s_v[i].active = 0U; }
    s_playing = 0U;
}

U16 audio_is_playing(void)
{
    return s_playing;
}

void audio_start(void)
{
    U16 i;

    if (s_playing != 0U) { return; }

    s_pk_i = 0U;
    s_left = 0UL;
    s_head = 0U;
    s_tail = 0U;
    for (i = 0U; i < RING_LEN; i++) { s_ring[i] = CMP_CENTER; }
    g_audio_events = 0U;
    g_audio_samples = 0UL;

    /* 先合成约 1/4 缓冲，再开载波，避免开头欠载 */
    for (i = 0U; i < (RING_LEN / 4U); i++) {
        if (s_left == 0UL) { if (next_event() == 0U) { break; } }
        s_ring[s_tail] = synth_duty();
        s_tail = (U16)((s_tail + 1U) & RING_MASK);
        s_left--;
    }

    audio_init();
    s_playing = 1U;
}

void audio_task(void)
{
    U16 guard = 0U;

    if (s_playing == 0U) { return; }

    /* 追着 ISR 读指针填（留 1 格空以区分空/满）*/
    while ((((U16)(s_tail + 1U)) & RING_MASK) != s_head && guard < RING_LEN) {
        if (s_left == 0UL) {
            if (next_event() == 0U) {          /* 曲表播完 */
                s_pk_i = 0U;                     /* ⇒ 从头循环，不停 */
                if (next_event() == 0U) {      /* 空表保护 */
                    audio_stop();
                    return;
                }
            }
        }
        s_ring[s_tail] = synth_duty();
        s_tail = (U16)((s_tail + 1U) & RING_MASK);
        s_left--;
        guard++;
    }
}

U16 audio_key_check(void)
{
    if ((s_playing != 0U) && (key_scan() != 0U)) {
        audio_stop();
        return 1U;
    }
    return 0U;
}

