/* bsp.h -- board support: types + absolute-address registers + pin helpers
   chip: TMS320F28335 (PZ-DSP28335-L board)
   addresses cross-checked against TI GEL / CCS targetdb / bitfield data */

#ifndef BSP_H_
#define BSP_H_

typedef unsigned int  U16;      /* C28x: int = 16 bit */
typedef unsigned long U32;      /* C28x: long = 32 bit */
typedef unsigned char U8;       /* C28x: char = 16 bit; only low 8 used for peripherals */
typedef int           S16;
typedef long          S32;

/* The absolute-address defines below are only for modules that have NOT been
   migrated to the official structures. If a .c includes DSP2833x_Device.h
   (which defines DSP2833x_DEVICE_H), they vanish automatically, because their
   names collide with the official struct members (PLLCR/PCLKCR3/WDCR/GPADIR...)
   and the preprocessor would otherwise wreck the official header. */
#ifndef DSP2833x_DEVICE_H

/* ---- system control (base 0x7010, word offsets) ---- */
#define SYS_BASE    0x7010UL
#define PLLSTS      (*(volatile U16 *)(SYS_BASE +  1))   /* 0x7011 bit0 = PLLLOCKS */
#define PCLKCR3     (*(volatile U16 *)(SYS_BASE + 16))   /* 0x7020 peripheral clock 3 */
#define WDCNTR      (*(volatile U16 *)(SYS_BASE + 19))   /* 0x7023 */
#define WDKEY       (*(volatile U16 *)(SYS_BASE + 21))   /* 0x7025 */
#define WDCR        (*(volatile U16 *)(SYS_BASE + 25))   /* 0x7029 */
#define PLLCR       (*(volatile U16 *)(SYS_BASE + 17))   /* 0x7021 */
#define PCLKCR3_GPIOINENCLK   0x2000U    /* bit13 */

/* ---- flash registers (base 0x0A80) ---- */
#define FLASH_BASE  0x0A80UL
#define FOPT        (*(volatile U16 *)(FLASH_BASE + 0x0))
#define FPWR        (*(volatile U16 *)(FLASH_BASE + 0x2))
#define FSTATUS     (*(volatile U16 *)(FLASH_BASE + 0x3))
#define FSTDBYWAIT  (*(volatile U16 *)(FLASH_BASE + 0x4))
#define FACTIVEWAIT (*(volatile U16 *)(FLASH_BASE + 0x5))
#define FBANKWAIT   (*(volatile U16 *)(FLASH_BASE + 0x6))
#define FOTPWAIT    (*(volatile U16 *)(FLASH_BASE + 0x7))

/* ---- GPIO control (base 0x6F80, 32-bit, EALLOW-protected) ---- */
#define GPIO_BASE   0x6F80UL
#define GPAMUX1     (*(volatile U32 *)(GPIO_BASE +  6))  /* 0x6F86 GPIO0-15  2 bit/pin */
#define GPAMUX2     (*(volatile U32 *)(GPIO_BASE +  8))  /* 0x6F88 GPIO16-31 */
#define GPADIR      (*(volatile U32 *)(GPIO_BASE + 10))  /* 0x6F8A GPIO0-31  1 bit/pin */
#define GPAPUD      (*(volatile U32 *)(GPIO_BASE + 12))  /* 0x6F8C 0 = pull-up enabled */
#define GPBMUX1     (*(volatile U32 *)(GPIO_BASE + 22))  /* 0x6F96 GPIO32-47 */
#define GPBMUX2     (*(volatile U32 *)(GPIO_BASE + 24))  /* 0x6F98 GPIO48-63 */
#define GPBDIR      (*(volatile U32 *)(GPIO_BASE + 26))  /* 0x6F9A GPIO32-63 */
#define GPBPUD      (*(volatile U32 *)(GPIO_BASE + 28))  /* 0x6F9C */
#define GPCMUX1     (*(volatile U32 *)(GPIO_BASE + 38))  /* 0x6FA6 GPIO64-79 */
#define GPCMUX2     (*(volatile U32 *)(GPIO_BASE + 40))  /* 0x6FA8 GPIO80-87 */
#define GPCDIR      (*(volatile U32 *)(GPIO_BASE + 42))  /* 0x6FAA GPIO64-87 */
#define GPCPUD      (*(volatile U32 *)(GPIO_BASE + 44))  /* 0x6FAC */

/* ---- GPIO data (32-bit, NOT EALLOW-protected; writing 1 acts, writing 0 does not) ---- */
#define GPADAT      (*(volatile U32 *)(GPIO_BASE + 64))  /* 0x6FC0 read pin levels */
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

/* ---- CPU Timer0 (base 0x0C00) : TIM 0(32) PRD 2(32) TCR 4 TPR 6 ---- */
#define TIMER0_BASE   0x0C00UL
#define T0_TIM        (*(volatile U32 *)(TIMER0_BASE + 0x0U))
#define T0_PRD        (*(volatile U32 *)(TIMER0_BASE + 0x2U))
#define T0_TCR        (*(volatile U16 *)(TIMER0_BASE + 0x4U))
#define T0_TPR        (*(volatile U16 *)(TIMER0_BASE + 0x6U))
#define T0_TPRH       (*(volatile U16 *)(TIMER0_BASE + 0x7U))

/* TCR bits: note TIE is bit14, not bit0 */
#define TCR_TSS       0x0010U   /* bit4  1 = stop */
#define TCR_TRB       0x0020U   /* bit5  1 = reload */
#define TCR_TIE       0x4000U   /* bit14 timer int enable */
#define TCR_TIF       0x8000U   /* bit15 int flag (write 1 to clear) */
#define PCLKCR3_CPUTIMER0ENCLK   0x0100U   /* bit8 */

/* ---- PIE (base 0x0CE0) ; vector table at 0x0D00 ----
   TINT0 = INT1.7 -> PIEIER1 bit6, vector slot 6 */
#define PIE_BASE        0x0CE0UL
#define PIECTRL         (*(volatile U16 *)(PIE_BASE + 0x0U))   /* bit0 = ENPIE */
#define PIEACK          (*(volatile U16 *)(PIE_BASE + 0x1U))
#define PIEIER1         (*(volatile U16 *)(PIE_BASE + 0x2U))
#define PIEIFR1         (*(volatile U16 *)(PIE_BASE + 0x3U))

#define PIECTRL_ENPIE       0x0001U
#define PIEACK_GROUP1       0x0001U
#define PIEIER1_TINT0       0x0040U     /* INT1.7 = TINT0 */

/* PIE vector table (0x0D00): every vector is 32-bit (ONE word, since C28x
   addresses are 16-bit) => 12 groups x 8 channels = 96 entries.
   PITFALL: writing 16-bit values at 0x0D06 is wrong; TINT0 lives at
   0x0D00 + 6*2 = 0x0D0C. A wrong slot makes the CPU jump into leftover RAM. */
#define PIE_VECT_BASE       0x0D00UL
#define PIE_VECT_NUM        96U
#define PIE_CH_TINT0        6U

void bsp_pie_vect_init(void);                            /* point all to a safe trap ISR */
void bsp_pie_vect_set(U16 chan, U32 isr_addr);           /* call under EALLOW */

/* CPU core interrupt control.
   PITFALL: IER/IFR are CPU core registers with NO memory address; they must be
   accessed with the C28x OR/AND IER instructions. Writing *(volatile U16*)0x0001
   only touches M0 SARAM and interrupts never enable. */
#define ENABLE_INT1_ASM     asm(" OR IER, #0x0001")
#define DISABLE_INT1_ASM    asm(" AND IER, #0xFFFE")
#define EINT_ASM            asm(" CLRC INTM")
#define DINT_ASM            asm(" SETC INTM")
#define ERTM_ASM            asm(" CLRC DBGM")
#endif /* !DSP2833x_DEVICE_H */

/* ---- pin -> bit/mux helpers (A/B/C groups) ----
   A: data/dir bit = GPIOx      ; MUX1/2 = 2 bit/pin at x*2
   B: data/dir bit = GPIOx-32   ; MUX2   = 2 bit/pin at (x-48)*2
   C: data/dir bit = GPIOx-64   ; MUX1   = 2 bit/pin at (x-64)*2 */
#define A_BIT(g)    (1UL << (g))
#define A_MUX1(g)   (3UL << ((g) * 2))          /* GPIO0..15  */
#define A_MUX2(g)   (3UL << (((g) - 16) * 2))   /* GPIO16..31 */
#define B_BIT(g)    (1UL << ((g) - 32))
#define B_MUX1(g)   (3UL << (((g) - 32) * 2))   /* GPIO32..47 */
#define B_MUX2(g)   (3UL << (((g) - 48) * 2))   /* GPIO48..63 */
#define C_BIT(g)    (1UL << ((g) - 64))
#define C_MUX1(g)   (3UL << (((g) - 64) * 2))   /* GPIO64..79 */
#define C_MUX2(g)   (3UL << (((g) - 80) * 2))   /* GPIO80..87 */

void bsp_init(void);                            /* clock / watchdog / flash wait states */
void bsp_service_dog(void);

/* generic pin ops by absolute pin number 0..87 (A/B/C resolved automatically) */
void bsp_pin_config_output(U16 pin);
void bsp_pin_set(U16 pin, U16 level);

#endif
