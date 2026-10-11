/* ============================================================================
 *  HardWare/GPIO/bsp.c —— 板级底层（已迁移到 TI 官方 DSP2833x 库）
 * ----------------------------------------------------------------------------
 *  全部改用官方 API：
 *    InitSysCtrl()        PLL / 外设时钟 / 关看门狗      (DSP2833x_SysCtrl.c)
 *    MemCopy()            ramfuncs 段 FLASH→RAM          (DSP2833x_MemCopy.c)
 *    InitFlash()          设置 Flash 等待状态/流水线（必须在 RAM 中执行）
 *    InitPieCtrl()        PIE 控制寄存器                 (DSP2833x_PieCtrl.c)
 *    InitPieVectTable()   中断向量表（指向官方默认 ISR）  (DSP2833x_PieVect.c)
 *    ServiceDog()         喂狗
 *    GpioCtrlRegs / GpioDataRegs   引脚操作（官方 DSP2833x_Gpio.h）
 *
 *  注意包含顺序：官方头必须先于 bsp.h，这样 bsp.h 中那些与官方结构体成员
 *  同名的"绝对地址宏"会被 #ifndef DSP2833x_DEVICE_H 自动跳过。
 * ========================================================================== */
#include "DSP2833x_Device.h"
#include "DSP2833x_GlobalPrototypes.h"
#include "bsp.h"

#ifdef FLASH_BUILD
/* 官方链接脚本 (tools\F28335.cmd) 在 ramfuncs 段上提供的三个符号 */
extern Uint16 RamfuncsLoadStart;
extern Uint16 RamfuncsLoadEnd;
extern Uint16 RamfuncsRunStart;
#endif

volatile U16 g_pclkcr1_seen = 0U;
volatile U16 g_pclkcr3_seen = 0U;

void bsp_init(void)
{
    InitSysCtrl();                          /* 官方：PLL、外设时钟、关看门狗 */

#ifdef FLASH_BUILD
    /* 官方做法：先把要在 RAM 中运行的代码拷过去，再设置 Flash 等待状态 */
    MemCopy(&RamfuncsLoadStart, &RamfuncsLoadEnd, &RamfuncsRunStart);
    InitFlash();
#endif

    /* 外设时钟：ePWM4 由官方 InitSysCtrl 打开；这里按需补 DMA/CPU-Timer0（实测地址写法）*/
    EALLOW;
    (*(volatile Uint16 *)0x701EUL) |= 0x0008U;   /* PCLKCR1 EPWM4ENCLK (hw-verified addr) */  /* EPWM4ENCLK */
    (*(volatile Uint16 *)0x7020UL) |= 0x0B00U;   /* PCLKCR3 DMAENCLK+CPUTIMER0ENCLK (hw-verified) */  /* DMAENCLK + CPUTIMER0ENCLK */
    EDIS;
    g_pclkcr1_seen = *(volatile Uint16 *)0x701EUL;
    g_pclkcr3_seen = *(volatile Uint16 *)0x7020UL;

    DINT;                                   /* 关全局中断 */
    InitPieCtrl();                          /* 官方 PIE 初始化 */
    IER = 0x0000;
    IFR = 0x0000;
    InitPieVectTable();                     /* 官方中断向量表（默认 ISR） */
}

void bsp_service_dog(void)
{
    ServiceDog();                           /* 官方喂狗 */
}

/* ------------------------------------------------------------------ */
/* 通用引脚操作（按引脚号 0..87，全部走官方 GPIO 结构体）              */
/* ------------------------------------------------------------------ */
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
