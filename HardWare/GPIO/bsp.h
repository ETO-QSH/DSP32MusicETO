/* ============================================================================
 *  bsp.h —— 板级底层：MCU 寄存器地址映射 + 位掩码换算 + 类型定义
 * ----------------------------------------------------------------------------
 *  芯片: TMS320F28335 (普中 PZ-DSP28335-L)
 *
 *  寄存器地址来源（均已交叉验证）:
 *    - TI 官方 GEL      : ccs_base/emulation/gel/f28335.gel
 *    - CCS 官方寄存器库 : targetdb/Modules/C2000/28x3x_GPIO_Registers.xml
 *                         targetdb/Modules/C2000/28x3x_System_Control_Registers.xml
 *    - CCS 官方位域数据 : c2000-idea/.../f2833x_sysctrl_registers.js
 *      (PCLKCR3: bit8..10=CPUTIMER0/1/2ENCLK, bit11=DMAENCLK,
 *                bit12=XINTFENCLK, bit13=GPIOINENCLK)
 *    - GPIO 数据寄存器为 32 位: GPADAT=0x6FC0 GPASET=0x6FC2 GPACLEAR=0x6FC4
 *                              GPBDAT=0x6FC8 GPBSET=0x6FCA GPBCLEAR=0x6FCC
 *                              GPCDAT=0x6FD0 GPCSET=0x6FD2 GPCCLEAR=0x6FD4
 * ========================================================================== */
#ifndef BSP_H_
#define BSP_H_

typedef unsigned int  U16;      /* C28x: int = 16 位 */
typedef unsigned long U32;      /* C28x: long = 32 位 */
typedef unsigned char U8;       /* C28x: char = 16 位，但只用低 8 位给外设 */
typedef int           S16;      /* 16 位有符号 */
typedef long          S32;      /* 32 位有符号 */

/* ------------------------------------------------------------------ */
/* 系统控制寄存器（基址 0x7010，字节偏移）                              */
/* ------------------------------------------------------------------ */
/* ------------------------------------------------------------------ *
 *  ★ 迁移开关：以下"绝对地址寄存器宏"仅供**尚未迁移到官方 API** 的模块  *
 *  使用。若某个 .c 先 #include "DSP2833x_Device.h"（官方头定义           *
 *  DSP2833x_DEVICE_H），则下列宏自动失效，该模块必须改用官方结构体：     *
 *      GpioCtrlRegs / GpioDataRegs / SysCtrlRegs / CpuTimer0Regs / …     *
 *  原因：宏名与官方结构体成员同名（PLLCR/PCLKCR3/WDCR/GPADIR/…），同时   *
 *        存在时预处理器会把官方结构体定义替换成垃圾 → 语法错。            *
 * ------------------------------------------------------------------ */
#ifndef DSP2833x_DEVICE_H
#define SYS_BASE    0x7010UL
#define PLLSTS      (*(volatile U16 *)(SYS_BASE +  1))   /* 0x7011 bit0=PLLLOCKS */
#define PCLKCR3     (*(volatile U16 *)(SYS_BASE + 16))   /* 0x7020 外设时钟控制3 */
#define WDCNTR      (*(volatile U16 *)(SYS_BASE + 19))   /* 0x7023 */
#define WDKEY       (*(volatile U16 *)(SYS_BASE + 21))   /* 0x7025 */
#define WDCR        (*(volatile U16 *)(SYS_BASE + 25))   /* 0x7029 */
#define PLLCR       (*(volatile U16 *)(SYS_BASE + 17))   /* 0x7021 */

#define PCLKCR3_GPIOINENCLK   0x2000U    /* bit13 */

/* ------------------------------------------------------------------ */
/* FLASH 寄存器（基址 0x0A80）                                          */
/* ------------------------------------------------------------------ */
#define FLASH_BASE  0x0A80UL
#define FOPT        (*(volatile U16 *)(FLASH_BASE + 0x0))
#define FPWR        (*(volatile U16 *)(FLASH_BASE + 0x2))
#define FSTATUS     (*(volatile U16 *)(FLASH_BASE + 0x3))
#define FSTDBYWAIT  (*(volatile U16 *)(FLASH_BASE + 0x4))
#define FACTIVEWAIT (*(volatile U16 *)(FLASH_BASE + 0x5))
#define FBANKWAIT   (*(volatile U16 *)(FLASH_BASE + 0x6))
#define FOTPWAIT    (*(volatile U16 *)(FLASH_BASE + 0x7))

/* ------------------------------------------------------------------ */
/* GPIO 控制寄存器（基址 0x6F80，32 位，受 EALLOW 保护）                */
/* ------------------------------------------------------------------ */
#define GPIO_BASE   0x6F80UL
#define GPAMUX1     (*(volatile U32 *)(GPIO_BASE +  6))  /* 0x6F86 GPIO0-15  每脚2位 */
#define GPAMUX2     (*(volatile U32 *)(GPIO_BASE +  8))  /* 0x6F88 GPIO16-31 */
#define GPADIR      (*(volatile U32 *)(GPIO_BASE + 10))  /* 0x6F8A GPIO0-31  每脚1位 */
#define GPAPUD      (*(volatile U32 *)(GPIO_BASE + 12))  /* 0x6F8C 0=使能上拉 */
#define GPBMUX1     (*(volatile U32 *)(GPIO_BASE + 22))  /* 0x6F96 GPIO32-47 */
#define GPBMUX2     (*(volatile U32 *)(GPIO_BASE + 24))  /* 0x6F98 GPIO48-63 */
#define GPBDIR      (*(volatile U32 *)(GPIO_BASE + 26))  /* 0x6F9A GPIO32-63 */
#define GPBPUD      (*(volatile U32 *)(GPIO_BASE + 28))  /* 0x6F9C */
#define GPCMUX1     (*(volatile U32 *)(GPIO_BASE + 38))  /* 0x6FA6 GPIO64-79 */
#define GPCMUX2     (*(volatile U32 *)(GPIO_BASE + 40))  /* 0x6FA8 GPIO80-87 */
#define GPCDIR      (*(volatile U32 *)(GPIO_BASE + 42))  /* 0x6FAA GPIO64-87 */
#define GPCPUD      (*(volatile U32 *)(GPIO_BASE + 44))  /* 0x6FAC */

/* ------------------------------------------------------------------ */
/* GPIO 数据寄存器（32 位，不需 EALLOW）  写 1 有效、写 0 无效          */
/* ------------------------------------------------------------------ */
#define GPADAT      (*(volatile U32 *)(GPIO_BASE + 64))  /* 0x6FC0 读引脚电平 */
#define GPASET      (*(volatile U32 *)(GPIO_BASE + 66))  /* 0x6FC2 */
#define GPACLEAR    (*(volatile U32 *)(GPIO_BASE + 68))  /* 0x6FC4 */
#define GPATOGGLE   (*(volatile U32 *)(GPIO_BASE + 70))  /* 0x6FC6 */
#define GPBDAT      (*(volatile U32 *)(GPIO_BASE + 72))  /* 0x6FC8 */
#define GPBSET      (*(volatile U32 *)(GPIO_BASE + 74))  /* 0x6FCA */
#define GPBCLEAR    (*(volatile U32 *)(GPIO_BASE + 76))  /* 0x6FCC */
#define GPBTOGGLE   (*(volatile U32 *)(GPIO_BASE + 78))  /* 0x6FCE */
#define GPCDAT      (*(volatile U32 *)(GPIO_BASE + 80))  /* 0x6FD0 */
#define GPCSET      (*(volatile U32 *)(GPIO_BASE + 82))  /* 0x6FD2 */
#define GPCCLEAR    (*(volatile U32 *)(GPIO_BASE + 84))  /* 0x6FD4 */
#define GPCTOGGLE   (*(volatile U32 *)(GPIO_BASE + 86))  /* 0x6FD6 */

#define EALLOW  asm(" EALLOW")
#define EDIS    asm(" EDIS")

/* ------------------------------------------------------------------ */
/* CPU 定时器 CPU-Timer0（基址 0x0C00）                                 */
/*   地址来源: CCS targetdb/Modules/C2000/C2000_CPU_Timer_Registers.xml */
/*   TIM offset0(32bit) PRD offset2(32bit) TCR offset4 TPR offset6      */
/* ------------------------------------------------------------------ */
#define TIMER0_BASE   0x0C00UL
#define T0_TIM        (*(volatile U32 *)(TIMER0_BASE + 0x0U))
#define T0_PRD        (*(volatile U32 *)(TIMER0_BASE + 0x2U))
#define T0_TCR        (*(volatile U16 *)(TIMER0_BASE + 0x4U))
#define T0_TPR        (*(volatile U16 *)(TIMER0_BASE + 0x6U))
#define T0_TPRH       (*(volatile U16 *)(TIMER0_BASE + 0x7U))

/* CPU-Timer 控制位（依据 CCS 位域数据 f2833x_cputimers_registers.js：
   TSS shift4 / TIE shift14 / TIF shift15）—— TIE 是 bit14，不是 bit0！ */
#define TCR_TSS       0x0010U   /* bit4  1 = 停止计数 */
#define TCR_TRB       0x0020U   /* bit5  1 = 重装（自清） */
#define TCR_TIE       0x4000U   /* bit14 定时器中断使能 */
#define TCR_TIF       0x8000U   /* bit15 中断标志（写 1 清除） */

#define PCLKCR3_CPUTIMER0ENCLK   0x0100U   /* bit8 */

/* ------------------------------------------------------------------ */
/* PIE 中断扩展（基址 0x0CE0）+ PIE 向量表（0x0D00）                    */
/*   地址来源: CCS targetdb/Modules/C2000/C2000_PIE_Registers.xml       */
/*   CPU-Timer0 中断 = INT1.7 → PIEIER1 bit6、向量表 0x0D06             */
/* ------------------------------------------------------------------ */
#define PIE_BASE        0x0CE0UL
#define PIECTRL         (*(volatile U16 *)(PIE_BASE + 0x0U))   /* bit0 = ENPIE */
#define PIEACK          (*(volatile U16 *)(PIE_BASE + 0x1U))
#define PIEIER1         (*(volatile U16 *)(PIE_BASE + 0x2U))
#define PIEIFR1         (*(volatile U16 *)(PIE_BASE + 0x3U))

#define PIECTRL_ENPIE       0x0001U
#define PIEACK_GROUP1       0x0001U
#define PIEIER1_TINT0       0x0040U     /* INT1.7 = TINT0 */

/* ------------------------------------------------------------------ *
 * PIE 向量表（0x0D00）：F2833x 每个向量是 **32 位**（2 个字，低字在前），*
 * 共 12 组 × 8 通道 = 96 项。                                         *
 * ★ 曾经踩的坑：按 16 位写（0x0D06）是错的——TINT0 的正确位置是          *
 *   0x0D00 + 6*2 = 0x0D0C，写错位置会让 CPU 取到 RAM 里的残留向量      *
 *   （例如上一轮 TI 例程留下的地址）而跑飞。                           *
 * ------------------------------------------------------------------ */
#define PIE_VECT_BASE       0x0D00UL
#define PIE_VECT_NUM        96U         /* 12 组 × 8 通道 */
#define PIE_CH_TINT0        6U          /* INT1.7 的 0 基通道号 */

void bsp_pie_vect_init(void);                            /* 全部指向安全陷阱 ISR */
void bsp_pie_vect_set(U16 chan, U32 isr_addr);           /* 需在 EALLOW 下调用 */

/* CPU 内核中断控制
   ★ IER/IFR 是 CPU 内核寄存器，**没有内存地址**（CCS targetdb 里 IER/IFR
     只有 id 没有 address）。必须用 C28x 的 OR/AND IER 指令访问；
     早先写成 *(volatile U16*)0x0001 是错的——那只是 M0 SARAM，中断永远开不了。 */
#define ENABLE_INT1_ASM     asm(" OR IER, #0x0001")     /* 使能 CPU INT1 (TINT0) */
#define DISABLE_INT1_ASM    asm(" AND IER, #0xFFFE")
#define EINT_ASM            asm(" CLRC INTM")           /* 开全局中断 */
#define DINT_ASM            asm(" SETC INTM")           /* 关全局中断 */
#define ERTM_ASM            asm(" CLRC DBGM")           /* 允许调试时也响应中断 */
#endif /* !DSP2833x_DEVICE_H —— 已 include 官方头的模块，请改用官方结构体 */

/* ------------------------------------------------------------------ */
/* 引脚 → 位掩码换算                                                    */
/*   A 组 GPIO0..31 : 数据/方向 bit=GPIOx;  MUX1/2 每脚 2 位 = x*2       */
/*   B 组 GPIO32..63: 数据/方向 bit=GPIOx-32; MUX2 每脚 2 位=(x-48)*2    */
/*   C 组 GPIO64..87: 数据/方向 bit=GPIOx-64; MUX1 每脚 2 位=(x-64)*2    */
/* ------------------------------------------------------------------ */
#define A_BIT(g)    (1UL << (g))
#define A_MUX1(g)   (3UL << ((g) * 2))          /* GPIO0..15  */
#define A_MUX2(g)   (3UL << (((g) - 16) * 2))   /* GPIO16..31 */
#define B_BIT(g)    (1UL << ((g) - 32))
#define B_MUX1(g)   (3UL << (((g) - 32) * 2))   /* GPIO32..47 */
#define B_MUX2(g)   (3UL << (((g) - 48) * 2))   /* GPIO48..63 */
#define C_BIT(g)    (1UL << ((g) - 64))
#define C_MUX1(g)   (3UL << (((g) - 64) * 2))   /* GPIO64..79 */
#define C_MUX2(g)   (3UL << (((g) - 80) * 2))   /* GPIO80..87 */

/* ------------------------------------------------------------------ */
/* 板级初始化：时钟 / 看门狗 / (FLASH 版本) Flash 等待状态              */
/* ------------------------------------------------------------------ */
void bsp_init(void);
void bsp_service_dog(void);     /* 喂狗（已关狗时也无副作用） */

/* ------------------------------------------------------------------ */
/* 通用引脚操作（按“引脚号 0..87”操作，自动区分 A/B/C 组）              */
/*   有了它，LCD 等模块的数据脚可以随时改接到别的 GPIO（见 lcd1602.c）  */
/* ------------------------------------------------------------------ */
void bsp_pin_config_output(U16 pin);            /* 配成普通 IO + 输出 + 上拉 */
void bsp_pin_set(U16 pin, U16 level);           /* level 非 0 = 输出高 */

#endif /* BSP_H_ */
