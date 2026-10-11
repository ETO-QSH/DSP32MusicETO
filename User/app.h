/* app.h -- application layer: combines the modules into the demo
   4-digit counter 0..9999; K1..K9 switch LED effects; LCD shows status */
#ifndef APP_H_
#define APP_H_

#include "bsp.h"

void app_init(void);
void app_task(void);

/* observable variables (readable by debugger / DSS scripts) */
extern volatile U16 g_value;
extern volatile U16 g_effect;
extern volatile U16 g_phase;
extern volatile U16 g_key;
extern volatile U16 g_key_log[16];
extern volatile U16 g_key_count;

#endif
