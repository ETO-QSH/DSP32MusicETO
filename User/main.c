/* main.c —— 入口：bsp_init + app_init + app_task 主循环
   各模块功能见 HardWare/（bsp/delay/led/smg/key/lcd1602/audio）与 app.c */
#include "bsp.h"
#include "app.h"

void main(void)
{
    bsp_init();
    app_init();

    for (;;) {
        app_task();
    }
}
