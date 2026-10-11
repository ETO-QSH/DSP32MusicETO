/* ============================================================================
 *  delay.c —— 软件延时（顺带产生蜂鸣器方波）
 * ----------------------------------------------------------------------------
 *  这样发声完全不占中断、不影响主循环，且音准与标定值无关
 *  （半周期 = LOOPS_PER_MS*500/freq 轮，而 1ms = LOOPS_PER_MS 轮，自动抵消）。
 * ========================================================================== */
#include "delay.h"

volatile U16 g_heartbeat_ms = 0U;

void delay_ms(U16 ms)
{
    volatile U32 n = (U32)ms * (U32)LOOPS_PER_MS;
    while (n > 0UL) {
        n = n - 1UL;
    }
    g_heartbeat_ms = (U16)(g_heartbeat_ms + ms);
}

void delay_us(U16 us)
{
    volatile U32 n = (U32)us * (U32)LOOPS_PER_US;
    while (n > 0UL) {
        n = n - 1UL;
    }
}
