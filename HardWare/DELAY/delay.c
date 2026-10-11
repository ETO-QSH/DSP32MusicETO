/* delay.c -- busy-wait delay; delay_ms also advances the heartbeat */
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
