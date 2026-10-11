/* bsp.c -- board support, built on the official TI DSP2833x library
   InitSysCtrl()       PLL / peripheral clocks / watchdog off
   MemCopy()           copy the ramfuncs section FLASH->RAM
   InitFlash()         flash wait states (must run from RAM)
   InitPieCtrl()       PIE control registers
   InitPieVectTable()  vector table -> official default ISRs
   ServiceDog()        feed the watchdog
   GpioCtrlRegs / GpioDataRegs  pin access
   include order: official headers FIRST, then bsp.h, so that bsp.h's
   absolute-address macros are skipped by #ifndef DSP2833x_DEVICE_H */
#include "DSP2833x_Device.h"
#include "DSP2833x_GlobalPrototypes.h"
#include "bsp.h"

#ifdef FLASH_BUILD
/* three symbols provided by the official linker command file (tools\F28335.cmd) */
extern Uint16 RamfuncsLoadStart;
extern Uint16 RamfuncsLoadEnd;
extern Uint16 RamfuncsRunStart;
#endif

volatile U16 g_pclkcr1_seen = 0U;
volatile U16 g_pclkcr3_seen = 0U;

void bsp_init(void)
{
    InitSysCtrl();

#ifdef FLASH_BUILD
    MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);
    InitFlash();
#endif

    /* peripheral clocks: InitSysCtrl already enables EPWM1..6; the two lines
       below are belt-and-braces for EPWM4 (bit3) and CPU-Timer0 (bit8).
       Addresses are hardware-verified: the vendor SysCtrlRegs offsets are wrong. */
    EALLOW;
    (*(volatile Uint16 *)0x701EUL) |= 0x0008U;   /* PCLKCR1 EPWM4ENCLK */
    (*(volatile Uint16 *)0x7020UL) |= 0x0B00U;   /* PCLKCR3 DMAENCLK + CPUTIMER0ENCLK */
    EDIS;
    g_pclkcr1_seen = *(volatile Uint16 *)0x701EUL;
    g_pclkcr3_seen = *(volatile Uint16 *)0x7020UL;

    DINT;
    InitPieCtrl();
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();
}

void bsp_service_dog(void)
{
    ServiceDog();
}

/* generic pin ops by absolute pin number 0..87, via the official structures */
void bsp_pin_config_output(U16 pin)
{
    EALLOW;
    if (pin < 32U) {
        GpioCtrlRegs.GPAMUX1.all &= ~((pin < 16U) ? A_MUX1(pin) : A_MUX2(pin));
        GpioCtrlRegs.GPADIR.all  |= A_BIT(pin);
        GpioCtrlRegs.GPAPUD.all  &= ~A_BIT(pin);
    } else if (pin < 64U) {
        GpioCtrlRegs.GPBMUX1.all &= ~((pin < 48U) ? B_MUX1(pin) : B_MUX2(pin));
        GpioCtrlRegs.GPBDIR.all  |= B_BIT(pin);
        GpioCtrlRegs.GPBPUD.all  &= ~B_BIT(pin);
    } else {
        GpioCtrlRegs.GPCMUX1.all &= ~((pin < 80U) ? C_MUX1(pin) : C_MUX2(pin));
        GpioCtrlRegs.GPCDIR.all  |= C_BIT(pin);
        GpioCtrlRegs.GPCPUD.all  &= ~C_BIT(pin);
    }
    EDIS;
}

void bsp_pin_set(U16 pin, U16 level)
{
    if (pin < 32U) {
        if (level != 0U) { GpioDataRegs.GPASET.all   = A_BIT(pin); }
        else             { GpioDataRegs.GPACLEAR.all = A_BIT(pin); }
    } else if (pin < 64U) {
        if (level != 0U) { GpioDataRegs.GPBSET.all   = B_BIT(pin); }
        else             { GpioDataRegs.GPBCLEAR.all = B_BIT(pin); }
    } else {
        if (level != 0U) { GpioDataRegs.GPCSET.all   = C_BIT(pin); }
        else             { GpioDataRegs.GPCCLEAR.all = C_BIT(pin); }
    }
}
