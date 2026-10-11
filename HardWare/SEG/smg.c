/* ============================================================================
 *  smg.c —— 74HC164 + 位选 的 4 位数码管【非阻塞】动态扫描
 * ----------------------------------------------------------------------------
 *  设计要点（这是速度问题的关键修复）：
 *    · 旧版 smg_show() 一次把 4 位全扫完，每位 delay_ms(2) ⇒ 阻塞 8ms ✗
 *      ⇒ 主循环长达 ~20ms ⇒ 音频环形缓冲喂不饱 ⇒ 播放又慢又断 ✗
 *    · 新版 smg_show() 【每次调用只刷新 1 位】⇒ 每次只花 SMG_ON_US 微秒 ✓
 *      ⇒ 主循环降到几毫秒 ⇒ 缓冲始终满 ⇒ 速度与连续性恢复 ✓
 *    · 接口不变（app.c 里仍是每轮调用一次 smg_show(g_value)）✓
 *  硬件（普中 PZ-DSP28335-L，官方教程第 17 章）：
 *      共阴数码管，段选 A..DP 经 74HC164 串并转换：
 *          74HC164 数据 DSA = GPIO54，时钟 CLK = GPIO56（上升沿移入，高位先出）
 *      位选 = GPIO70/71/72/73（输出高 → 三极管导通 → 该位选通）
 *      段码: 0=0x3F 1=0x06 2=0x5B 3=0x4F 4=0x66 5=0x6D 6=0x7D 7=0x07 8=0x7F 9=0x6F
 * ========================================================================== */
#include "DSP2833x_Device.h"
#include "smg.h"
#include "delay.h"

#define HC164_DAT_PIN   54U     /* 74HC164 数据 */
#define HC164_CLK_PIN   56U     /* 74HC164 时钟 */
#define SEG_PIN_FIRST   70U     /* 位选 1（左起第 1 位）*/

#define HC164_MUX2      (B_MUX2(HC164_DAT_PIN) | B_MUX2(HC164_CLK_PIN))
#define HC164_DAT       (B_BIT(HC164_DAT_PIN) | B_BIT(HC164_CLK_PIN))
#define SEG_MUX1        (C_MUX1(70U) | C_MUX1(71U) | C_MUX1(72U) | C_MUX1(73U))
#define SEG_DAT         (C_BIT(70U) | C_BIT(71U) | C_BIT(72U) | C_BIT(73U))

/* 共阴段码 0..9 */
static const U16 seg_code[10] = {
    0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U, 0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
};

static U16 s_value = 0U;        /* 最近一次要显示的值 */
static U16 s_digit = 0U;        /* 下一次要点亮的位（0..3）*/

void smg_init(void)
{
    EALLOW;
    /* 74HC164 数据/时钟 → 普通 IO + 输出 */
    GpioCtrlRegs.GPBMUX2.all &= ~HC164_MUX2;
    GpioCtrlRegs.GPBDIR.all  |=  HC164_DAT;
    GpioCtrlRegs.GPBPUD.all  &= ~HC164_DAT;
    /* 位选 GPIO70-73 → 普通 IO + 输出 */
    GpioCtrlRegs.GPCMUX1.all &= ~SEG_MUX1;
    GpioCtrlRegs.GPCDIR.all  |=  SEG_DAT;
    GpioCtrlRegs.GPCPUD.all  &= ~SEG_DAT;
    EDIS;

    smg_off();
    GpioDataRegs.GPBCLEAR.all = B_BIT(HC164_CLK_PIN);
}

void smg_off(void)
{
    GpioDataRegs.GPCCLEAR.all = SEG_DAT;             /* 位选全部拉低 */
}

/* 串行发送一个段码（高位先出，时钟上升沿移入）*/
static void hc164_send(U16 dat)
{
    U16 i;
    for (i = 0U; i < 8U; i++) {
        GpioDataRegs.GPBCLEAR.all = B_BIT(HC164_CLK_PIN);
        if (dat & 0x0080U) {
            GpioDataRegs.GPBSET.all   = B_BIT(HC164_DAT_PIN);
        } else {
            GpioDataRegs.GPBCLEAR.all = B_BIT(HC164_DAT_PIN);
        }
        GpioDataRegs.GPBSET.all = B_BIT(HC164_CLK_PIN);
        dat = (U16)(dat << 1);
    }
}

/* ★ 非阻塞：每次调用只刷新 1 位，然后点亮 SMG_ON_US 微秒
 *   app.c 每轮主循环调用一次即可，4 轮刷新 4 位 ✓                    */
void smg_show(U16 value)
{
    U16 d;

    s_value = value;
    if (s_digit >= SMG_DIGITS) { s_digit = 0U; }

    switch (s_digit) {
        case 0U:  d = seg_code[(s_value / 1000U) % 10U]; break;
        case 1U:  d = seg_code[(s_value / 100U)  % 10U]; break;
        case 2U:  d = seg_code[(s_value / 10U)   % 10U]; break;
        default:  d = seg_code[s_value % 10U];           break;
    }

    smg_off();                                          /* 先消隐，防止串位 */
    hc164_send(d);
    GpioDataRegs.GPCSET.all = C_BIT((U16)(SEG_PIN_FIRST + s_digit));
    delay_us(SMG_ON_US);                                /* 该位点亮时长 */

    s_digit++;
}
