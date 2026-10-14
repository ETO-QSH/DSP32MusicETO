/* oled.h -- 0.96" 128x64 I2C OLED (SSD1306), software I2C, bit-banged
 *
 * 接线（与现有外设无冲突）：
 *     OLED VCC -> 3.3V        OLED GND -> GND
 *     OLED SCL -> GPIO15      OLED SDA -> GPIO16
 * 说明：软件模拟 I2C，不使用硬件 I2C 模块（免去 GPIO32/33 复用与 FIFO 配置）。
 *       显存 128x64 = 1024 字节，按页组织（每字节 = 一列 8 个竖直像素，LSB = 顶行）。
 */
#ifndef OLED_H_
#define OLED_H_

#include "bsp.h"

#define OLED_SCL_PIN    15U
#define OLED_SDA_PIN    16U

#define OLED_W          128U
#define OLED_H          64U

void oled_init(void);                       /* 引脚 + SSD1306 初始化 + 上电自检画面 */
void oled_clear(void);                      /* 清显存（不自动刷新） */
void oled_fill(U8 pattern);                 /* 全屏填 0x00 / 0xFF 等图案 */
void oled_pixel(U8 x, U8 y, U8 on);         /* 画一个像素 */
void oled_char(U8 x, U8 y, char c);         /* x = 0..15，y = 0..7（8x8 字符单元） */
void oled_str(U8 x, U8 y, const char *s);   /* 从 (x,y) 开始写字符串 */
void oled_flush(void);
void oled_show(U16 key, U16 value, U16 playing);  /* 信息主界面 */
void oled_lyric(U32 t_ms);                       /* 按时间显示歌词（自动折行/居中）*/                      /* 把显存整屏写入 OLED */

#endif
