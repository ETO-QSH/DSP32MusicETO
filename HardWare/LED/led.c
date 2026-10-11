/* ============================================================================
 *  led.c —— 7 个 LED 的驱动与 9 种效果
 * ========================================================================== */
/* 官方库：必须先于 led.h/bsp.h 包含，使 bsp.h 里的绝对地址寄存器宏自动失效
   → 本文件从此只使用官方 GpioCtrlRegs / GpioDataRegs 结构体 */
#include "DSP2833x_Device.h"
#include "led.h"

/* 引脚号（见 led.h 说明） */
#define LED_PIN_D1      68U     /* C 组 */
#define LED_PIN_D2      67U
#define LED_PIN_D3      66U
#define LED_PIN_D4      65U
#define LED_PIN_D5      64U
#define LED_PIN_D6      10U     /* A 组 */
#define LED_PIN_D7      11U

/* 聚合掩码 */
#define LED_MUX1_A      (A_MUX1(LED_PIN_D6) | A_MUX1(LED_PIN_D7))
#define LED_DAT_A       (A_BIT(LED_PIN_D6) | A_BIT(LED_PIN_D7))
#define LED_MUX1_C      (C_MUX1(LED_PIN_D5) | C_MUX1(LED_PIN_D4) | C_MUX1(LED_PIN_D3) | \
                         C_MUX1(LED_PIN_D2) | C_MUX1(LED_PIN_D1))
#define LED_DAT_C       (C_BIT(LED_PIN_D5) | C_BIT(LED_PIN_D4) | C_BIT(LED_PIN_D3) | \
                         C_BIT(LED_PIN_D2) | C_BIT(LED_PIN_D1))

void led_init(void)
{
    EALLOW;
    /* D6/D7（A 组）→ 普通 IO + 输出 + 上拉（官方 GPIO 结构体） */
    GpioCtrlRegs.GPAMUX1.all &= ~LED_MUX1_A;
    GpioCtrlRegs.GPADIR.all  |=  LED_DAT_A;
    GpioCtrlRegs.GPAPUD.all  &= ~LED_DAT_A;
    /* D1..D5（C 组）→ 普通 IO + 输出 + 上拉 */
    GpioCtrlRegs.GPCMUX1.all &= ~LED_MUX1_C;
    GpioCtrlRegs.GPCDIR.all  |=  LED_DAT_C;
    GpioCtrlRegs.GPCPUD.all  &= ~LED_DAT_C;
    EDIS;

    led_all_off();
}

void led_all_off(void)
{
    GpioDataRegs.GPASET.all   = LED_DAT_A;
    GpioDataRegs.GPCSET.all   = LED_DAT_C;
}

void led_all_on(void)
{
    GpioDataRegs.GPACLEAR.all = LED_DAT_A;
    GpioDataRegs.GPCCLEAR.all = LED_DAT_C;
}

void led_mask(U16 mask)
{
    U32 a = 0UL;
    U32 c = 0UL;

    if (mask & 0x01U) { c |= C_BIT(LED_PIN_D1); }
    if (mask & 0x02U) { c |= C_BIT(LED_PIN_D2); }
    if (mask & 0x04U) { c |= C_BIT(LED_PIN_D3); }
    if (mask & 0x08U) { c |= C_BIT(LED_PIN_D4); }
    if (mask & 0x10U) { c |= C_BIT(LED_PIN_D5); }
    if (mask & 0x20U) { a |= A_BIT(LED_PIN_D6); }
    if (mask & 0x40U) { a |= A_BIT(LED_PIN_D7); }

    led_all_off();
    if (a != 0UL) { GpioDataRegs.GPACLEAR.all = a; }
    if (c != 0UL) { GpioDataRegs.GPCCLEAR.all = c; }
}

/* 效果 0..8（与 K1..K9 对应）
   0 全亮     1 全灭     2 单灯流水快  3 单灯流水慢  4 双灯追逐
   5 整排快闪 6 奇偶交替 7 两端向中间  8 逐个累积满                       */
void led_effect(U16 effect, U16 phase)
{
    U16 p;
    U16 m;

    switch (effect) {
        case 0U:
            m = 0x7FU; break;
        case 1U:
            m = 0x00U; break;
        case 2U:
        case 3U:
            m = (U16)(1U << (phase % 7U)); break;
        case 4U:
            p = (U16)(phase % 7U);
            m = (U16)((1U << p) | (1U << ((p + 3U) % 7U))); break;
        case 5U:
            m = (phase & 1U) ? 0x7FU : 0x00U; break;
        case 6U:
            m = (phase & 1U) ? 0x55U : 0x2AU; break;
        case 7U:
            p = (U16)(phase % 4U);
            m = (U16)((1U << p) | (1U << (6U - p))); break;
        default:
            p = (U16)(phase % 8U);
            m = (p >= 7U) ? 0x7FU : (U16)((1U << p) - 1U); break;
    }
    led_mask(m);
}

U16 led_effect_period(U16 effect)
{
    static const U16 period[LED_EFFECT_NUM] = {
        1000U, 1000U, 80U, 300U, 120U, 250U, 250U, 150U, 120U
    };
    if (effect >= LED_EFFECT_NUM) {
        return 1000U;
    }
    return period[effect];
}

const char *led_effect_name(U16 effect)
{
    static const char *const name[LED_EFFECT_NUM] = {
        "ALL ON      ",
        "ALL OFF     ",
        "CHASE FAST  ",
        "CHASE SLOW  ",
        "DUAL CHASE  ",
        "FLASH ALL   ",
        "ALTERNATE   ",
        "MERGE       ",
        "FILL UP     "
    };
    if (effect >= LED_EFFECT_NUM) {
        return "UNKNOWN     ";
    }
    return name[effect];
}
