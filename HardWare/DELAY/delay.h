/* ============================================================================
 *  delay.h —— 软件延时 + 运行心跳（用于标定实际速度）
 * ----------------------------------------------------------------------------
 *  LOOPS_PER_MS：1 毫秒 ≈ 多少轮空循环（**实测标定值**）。
 *    RAM 版本与 FLASH 版本速度不同，分别标定；
 *  标定方法：用调试器读两次 g_heartbeat_ms（间隔 1 秒真实时间），
 *    若 1 秒涨了 2000，说明程序认为过了 2 秒 → 把 LOOPS_PER_MS 减半。
 * ========================================================================== */
#ifndef DELAY_H_
#define DELAY_H_

#include "bsp.h"

#ifdef FLASH_BUILD
#define LOOPS_PER_MS   4480UL       /* FLASH: 120MHz measured 5600 -> x0.8 for 150MHz (official InitSysCtrl PLLCR=10) */
#else
#define LOOPS_PER_MS   9200UL       /* RAM: 11500 x 0.8 for 150MHz */
#endif
#define LOOPS_PER_US   (LOOPS_PER_MS / 1000UL)

/* 心跳：累计"程序认为自己延时过的毫秒数" */
extern volatile U16 g_heartbeat_ms;

void delay_ms(U16 ms);
void delay_us(U16 us);

#endif /* DELAY_H_ */
