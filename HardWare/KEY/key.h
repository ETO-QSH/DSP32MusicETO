/* key.h -- on-board 3x3 matrix keypad (debounce + press-edge detection)
   hardware: rows (input, active low) H1/H2/H3 = GPIO12/13/14
             cols (output, active low) L1/L2/L3 = GPIO48/49/50
   layout : col1 = K1/K4/K7, col2 = K2/K5/K8, col3 = K3/K6/K9 */
#ifndef KEY_H_
#define KEY_H_

#include "bsp.h"

#define KEY_NUM     9U

void key_init(void);
U16 key_scan(void);     /* raw read: 0 = none, 1..9 = K1..K9 */
U16 key_task(void);     /* debounced press edge: key (1..9) or 0; call every loop */

#endif
