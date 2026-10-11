/* ============================================================================
 *  lcd1602.h —— LCD1602 字符液晶（8 位并口）
 * ----------------------------------------------------------------------------
 *  硬件（普中 PZ-DSP28335-L，依据官方教程第 23 章）：
 *      DB0..DB7 = GPIO2..GPIO9
 *      RS       = GPIO0
 *      E        = GPIO1
 *      RW       = GND（只写，读不到忙标志，所以用延时等待）
 *      对比度由板上电位器 RJ1 调节
 *
 *  注意：本驱动只用 GPASET/GPACLEAR 操作自己的引脚，
 *        不会覆写整个 GPADAT（否则会把同组的 LED D6/D7 一起改掉）。
 * ========================================================================== */
#ifndef LCD1602_H_
#define LCD1602_H_

#include "bsp.h"

#define LCD1602_LINE1   1U
#define LCD1602_LINE2   2U
#define LCD1602_COLS    16U

void lcd1602_init(void);
void lcd1602_cmd(U8 cmd);
void lcd1602_data(U8 dat);
void lcd1602_clear(void);

/* 光标定位到第 line 行(1/2)、第 col 列(0..15) */
void lcd1602_goto(U8 line, U8 col);

/* 从当前光标处写字符串 */
void lcd1602_puts(const char *s);

/* 从 line 行 col 列写字符串 */
void lcd1602_puts_at(U8 line, U8 col, const char *s);

/* 定宽输出：不足补空格（避免旧字符残留） */
void lcd1602_puts_pad(const char *s, U8 width);

/* 定宽十进制输出，例如 put_u16(13, 4) → "0013" */
void lcd1602_put_u16(U16 value, U8 width);

#endif /* LCD1602_H_ */
