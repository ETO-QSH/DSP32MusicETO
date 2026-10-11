/* led.h -- the 7 on-board LEDs (9 display effects)
   hardware: D1..D5 = GPIO68,67,66,65,64 ; D6,D7 = GPIO10,11 ; low = lit */
#ifndef LED_H_
#define LED_H_

#include "bsp.h"

#define LED_NUM         7U
#define LED_EFFECT_NUM  9U          /* effects 0..8, mapped to keys K1..K9 */

void led_init(void);
void led_all_on(void);
void led_all_off(void);
void led_mask(U16 mask);                            /* bit0=D1 .. bit6=D7 ; 1 = lit */
void led_effect(U16 effect, U16 phase);             /* refresh "effect + step" */
U16 led_effect_period(U16 effect);                  /* step interval (ms) */
const char *led_effect_name(U16 effect);            /* ASCII name for LCD */

#endif