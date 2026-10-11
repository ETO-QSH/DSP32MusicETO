/* ============================================================================
 *  lcd1602.c —— LCD1602 驱动（8 位并口，只写）
 * ----------------------------------------------------------------------------
 *  HD44780 时序：RS 建立 → 数据建立 → E 上升沿 → E 高电平 ≥450ns →
 *  E 下降沿写入 → 等待 LCD 内部执行（普通指令约 37us，清屏约 1.52ms）
 *
 *  ★ 本驱动用"引脚表"逐位输出，所以任何一根线都能改接到别的 GPIO。
 *    这块板子上 GPIO6 同时是 LCD 的 DB4 和蜂鸣器的控制脚：
 *      - 不改线：靠 app.c 里"蜂鸣器响时不刷 LCD"互斥；
 *      - 改线（推荐）：把 LCD 第 11 脚(DB4) 掰出来接到空闲 GPIO，
 *        然后把下面 lcd_data_pin[4] 改成那个引脚号即可（如 15 = GPIO15），
 *        注意一定要先断开原来的 GPIO6，不能两根线直接并在一起。
 * ========================================================================== */
#include "lcd1602.h"
#include "delay.h"
#include "bsp.h"

/* ---------------- 引脚映射（要改线只改这两处）---------------- */
#define LCD_RS_PIN      0U      /* RS */
#define LCD_EN_PIN      1U      /* E  */
static const U16 lcd_data_pin[8] = { 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U };  /* DB0..DB7 */

static void lcd_pins_init(void)
{
    U16 i;

    bsp_pin_config_output(LCD_RS_PIN);
    bsp_pin_config_output(LCD_EN_PIN);
    for (i = 0U; i < 8U; i++) {
        bsp_pin_config_output(lcd_data_pin[i]);
    }
    bsp_pin_set(LCD_RS_PIN, 0U);
    bsp_pin_set(LCD_EN_PIN, 0U);
}

/* 写一个字节：rs = 1 写数据，rs = 0 写命令 */
static void lcd_write(U8 value, U16 rs)
{
    U16 i;

    bsp_pin_set(LCD_EN_PIN, 0U);                /* E = 0 */
    bsp_pin_set(LCD_RS_PIN, rs);                /* RS */

    for (i = 0U; i < 8U; i++) {                 /* DB0..DB7 逐位送出 */
        bsp_pin_set(lcd_data_pin[i], (U16)(((U16)value >> i) & 0x0001U));
    }
    delay_us(5U);                               /* 数据建立 */

    bsp_pin_set(LCD_EN_PIN, 1U);                /* E 上升沿 */
    delay_us(50U);                              /* E 高电平宽度（>>450ns） */
    bsp_pin_set(LCD_EN_PIN, 0U);                /* E 下降沿 → 写入 */
    delay_us(100U);                              /* 等 LCD 内部执行完 */
}

void lcd1602_cmd(U8 cmd)
{
    lcd_write(cmd, 0U);
    if ((cmd == 0x01U) || (cmd == 0x02U)) {
        delay_ms(5U);                           /* 清屏/回车需要 1.52ms */
    }
}

void lcd1602_data(U8 dat)
{
    lcd_write(dat, 1U);
}

void lcd1602_init(void)
{
    lcd_pins_init();

    delay_ms(50U);                              /* 上电等待 50ms（放宽） */
    lcd1602_cmd(0x38U);                         /* 8 位数据、2 行、5x7 点阵 */
    delay_ms(10U);
    lcd1602_cmd(0x38U);
    delay_ms(1U);
    lcd1602_cmd(0x38U);
    lcd1602_cmd(0x08U);                         /* 关显示 */
    lcd1602_cmd(0x01U);                         /* 清屏 */
    lcd1602_cmd(0x06U);                         /* 写入后光标右移，显示不移动 */
    lcd1602_cmd(0x0CU);                         /* 开显示、无光标、不闪烁 */
}

void lcd1602_clear(void)
{
    lcd1602_cmd(0x01U);
}

void lcd1602_goto(U8 line, U8 col)
{
    U8 addr;

    if (col >= LCD1602_COLS) {
        col = 0U;
    }
    if (line == LCD1602_LINE2) {
        addr = (U8)(0x40U + col);
    } else {
        addr = col;
    }
    lcd1602_cmd((U8)(0x80U | addr));
}

void lcd1602_puts(const char *s)
{
    while (*s != '\0') {
        lcd1602_data((U8)(*s));
        s++;
    }
}

void lcd1602_puts_at(U8 line, U8 col, const char *s)
{
    lcd1602_goto(line, col);
    lcd1602_puts(s);
}

void lcd1602_puts_pad(const char *s, U8 width)
{
    U8 n = 0U;

    while ((*s != '\0') && (n < width)) {
        lcd1602_data((U8)(*s));
        s++;
        n++;
    }
    while (n < width) {
        lcd1602_data((U8)' ');
        n++;
    }
}

void lcd1602_put_u16(U16 value, U8 width)
{
    U8 buf[5];
    U8 i;

    if (width > 5U) {
        width = 5U;
    }
    for (i = 0U; i < width; i++) {
        buf[width - 1U - i] = (U8)('0' + (value % 10U));
        value = (U16)(value / 10U);
    }
    for (i = 0U; i < width; i++) {
        lcd1602_data(buf[i]);
    }
}
