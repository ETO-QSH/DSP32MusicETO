/* smg.h -- 4-digit common-anode 7-seg (74HC164 segment + GPIO digit select)
   segment: 74HC164 DSA=GPIO54 CLK=GPIO56 (shift on rising edge, MSB first)
   digit  : GPIO70/71/72/73 (high -> transistor on -> digit selected)
   codes  : 0=0x3F 1=0x06 2=0x5B 3=0x4F 4=0x66 5=0x6D 6=0x7D 7=0x07 8=0x7F 9=0x6F */
#ifndef SMG_H_
#define SMG_H_

#include "bsp.h"

#define SMG_DIGITS          4U
#define SMG_ON_US           600U    /* on-time per digit, per call (non-blocking scan) */
#define SMG_REFRESH_MS      2U      /* nominal 4-digit period */

void smg_init(void);
void smg_off(void);
void smg_show(U16 value);           /* refresh ONE digit per call; shows 0..9999 */

#endif
