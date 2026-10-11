/* smg.c -- 74HC164 + digit-select, 4-digit 7-seg, non-blocking scan
   smg_show() refreshes ONE digit per call and lights it SMG_ON_US.
   Call it once per main-loop pass (interface unchanged). */
#include "DSP2833x_Device.h"
#include "smg.h"
#include "delay.h"

#define HC164_DAT_PIN   54U
#define HC164_CLK_PIN   56U
#define SEG_PIN_FIRST   70U

#define HC164_MUX2      (B_MUX2(HC164_DAT_PIN) | B_MUX2(HC164_CLK_PIN))
#define HC164_DAT       (B_BIT(HC164_DAT_PIN) | B_BIT(HC164_CLK_PIN))
#define SEG_MUX1        (C_MUX1(70U) | C_MUX1(71U) | C_MUX1(72U) | C_MUX1(73U))
#define SEG_DAT         (C_BIT(70U) | C_BIT(71U) | C_BIT(72U) | C_BIT(73U))

static const U16 seg_code[10] = {
    0x3FU, 0x06U, 0x5BU, 0x4FU, 0x66U, 0x6DU, 0x7DU, 0x07U, 0x7FU, 0x6FU
};

static U16 s_value = 0U;
static U16 s_digit = 0U;

void smg_init(void)
{
    EALLOW;
    GpioCtrlRegs.GPBMUX2.all &= ~HC164_MUX2;
    GpioCtrlRegs.GPBDIR.all  |=  HC164_DAT;
    GpioCtrlRegs.GPBPUD.all  &= ~HC164_DAT;
    GpioCtrlRegs.GPCMUX1.all &= ~SEG_MUX1;
    GpioCtrlRegs.GPCDIR.all  |=  SEG_DAT;
    GpioCtrlRegs.GPCPUD.all  &= ~SEG_DAT;
    EDIS;

    smg_off();
    GpioDataRegs.GPBCLEAR.all = B_BIT(HC164_CLK_PIN);
}

void smg_off(void)
{
    GpioDataRegs.GPCCLEAR.all = SEG_DAT;
}

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

    smg_off();
    hc164_send(d);
    GpioDataRegs.GPCSET.all = C_BIT((U16)(SEG_PIN_FIRST + s_digit));
    delay_us(SMG_ON_US);

    s_digit++;
}