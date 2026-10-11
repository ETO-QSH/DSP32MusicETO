/* delay.h -- busy-wait delay + heartbeat counter */
#ifndef DELAY_H_
#define DELAY_H_

#include "bsp.h"

#ifdef FLASH_BUILD
#define LOOPS_PER_MS   4480UL       /* calibrated; re-calibrate if clock/loop changes */
#else
#define LOOPS_PER_MS   9200UL
#endif
#define LOOPS_PER_US   (LOOPS_PER_MS / 1000UL)

extern volatile U16 g_heartbeat_ms;

void delay_ms(U16 ms);
void delay_us(U16 us);

#endif
