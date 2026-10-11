/* lcd1602.h -- LCD1602 character LCD (8-bit parallel, write-only)
   hardware: DB0..DB7 = GPIO2..GPIO9, RS = GPIO0, E = GPIO1, RW tied to GND
   contrast: adjusted by the on-board pot (V0)
   note: this driver only touches its own pins via GPASET/GPACLEAR */
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
void lcd1602_goto(U8 line, U8 col);             /* line = 1/2, col = 0..15 */
void lcd1602_puts(const char *s);               /* from current cursor */
void lcd1602_puts_at(U8 line, U8 col, const char *s);
void lcd1602_puts_pad(const char *s, U8 width); /* pad with spaces */
void lcd1602_put_u16(U16 value, U8 width);      /* e.g. put_u16(13,4) -> "0013" */

#endif
