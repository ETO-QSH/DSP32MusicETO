/* oled.c -- 0.96" 128x64 I2C OLED (SSD1306) driver, software (bit-banged) I2C
 *
 *  SCL = GPIO15   SDA = GPIO16   (不与现有外设冲突)
 *  显存：8 页 x 128 字节，按 2 字节打包成 1 个 U16 => 8 x 64 = 512 字 RAM
 *  字模：font8x8.h（列主序，LSB = 顶行），正好是 SSD1306 显存的排布方式
 *
 *  自检画面在 oled_init() 末尾自动绘制（边框 + 棋盘格 + 文字），便于首次验证接线。
 */
#include "DSP2833x_Device.h"
#include "oled.h"
#include "delay.h"
#include "../MIDI/font_tables.h"
#include "../MIDI/lrc_table.h"

/* ---------------- 显存（按页打包，2 字节/字）---------------- */
static U16 s_oled[8][64];       /* [page][word] : word 低字节 = 偶数列, 高字节 = 奇数列 */

/* ---------------- GPIO 快捷操作 ---------------- */
#define SCL_H()   bsp_pin_set(OLED_SCL_PIN, 1U)
#define SCL_L()   bsp_pin_set(OLED_SCL_PIN, 0U)
#define SDA_H()   bsp_pin_set(OLED_SDA_PIN, 1U)
#define SDA_L()   bsp_pin_set(OLED_SDA_PIN, 0U)

#define OLED_ADDR 0x78U         /* SSD1306 从机地址 0x3C << 1 */

static void i2c_delay(void)
{
    delay_us(2U);               /* 约 100 kHz（含函数调用开销） */
}

static void i2c_start(void)
{
    SDA_H(); SCL_H(); i2c_delay();
    SDA_L(); i2c_delay();       /* SCL 高时 SDA 下降沿 = 起始 */
    SCL_L(); i2c_delay();
}

static void i2c_stop(void)
{
    SDA_L(); SCL_L(); i2c_delay();
    SCL_H(); i2c_delay();
    SDA_H(); i2c_delay();       /* SCL 高时 SDA 上升沿 = 停止 */
}

/* 写一个字节（第 9 个时钟的应答被忽略：OLED 一般不拉低也没关系） */
static void i2c_wr(U8 b)
{
    U8 i;
    for (i = 0U; i < 8U; i++) {
        if ((b & 0x80U) != 0U) { SDA_H(); } else { SDA_L(); }
        b = (U8)(b << 1);
        i2c_delay();
        SCL_H(); i2c_delay();
        SCL_L(); i2c_delay();
    }
    SDA_H(); i2c_delay();       /* ACK 时隙 */
    SCL_H(); i2c_delay();
    SCL_L(); i2c_delay();
}

static void oled_cmd(U8 c)
{
    i2c_start();
    i2c_wr(OLED_ADDR);
    i2c_wr(0x00U);              /* 控制字节：命令 */
    i2c_wr(c);
    i2c_stop();
}

/* ---------------- 初始化 ---------------- */
static void oled_pins_init(void)
{
    bsp_pin_config_output(OLED_SCL_PIN);
    bsp_pin_config_output(OLED_SDA_PIN);
    SCL_H();
    SDA_H();
}

static void oled_panel_init(void)
{
    oled_cmd(0xAEU);            /* display off */
    oled_cmd(0xD5U); oled_cmd(0x80U);   /* clock divide / osc freq */
    oled_cmd(0xA8U); oled_cmd(0x3FU);   /* multiplex = 64 */
    oled_cmd(0xD3U); oled_cmd(0x00U);   /* display offset = 0 */
    oled_cmd(0x40U);                    /* start line = 0 */
    oled_cmd(0x8DU); oled_cmd(0x14U);   /* charge pump on */
    oled_cmd(0x20U); oled_cmd(0x02U);   /* memory mode = page */
    oled_cmd(0xA1U);                    /* segment remap */
    oled_cmd(0xC8U);                    /* COM scan direction remapped */
    oled_cmd(0xDAU); oled_cmd(0x12U);   /* COM pins config */
    oled_cmd(0x81U); oled_cmd(0xCFU);   /* contrast */
    oled_cmd(0xD9U); oled_cmd(0xF1U);   /* pre-charge */
    oled_cmd(0xDBU); oled_cmd(0x40U);   /* VCOMH deselect */
    oled_cmd(0xA4U);                    /* resume RAM content */
    oled_cmd(0xA6U);                    /* normal (not inverted) */
    oled_cmd(0xAFU);                    /* display on */
}

/* ---------------- 显存操作 ---------------- */
void oled_clear(void)
{
    U8 p, c;
    for (p = 0U; p < 8U; p++) {
        for (c = 0U; c < 64U; c++) { s_oled[p][c] = 0U; }
    }
}

void oled_fill(U8 pattern)
{
    U8 p, c;
    U16 w = (U16)(((U16)pattern << 8) | (U16)pattern);
    for (p = 0U; p < 8U; p++) {
        for (c = 0U; c < 64U; c++) { s_oled[p][c] = w; }
    }
}

void oled_pixel(U8 x, U8 y, U8 on)
{
    U16 *w;
    U8  bit;

    if ((x >= OLED_W) || (y >= OLED_H)) { return; }
    w   = &s_oled[y >> 3][x >> 1];
    bit = (U8)(1U << (y & 7U));                 /* 页内竖直位 */
    if ((x & 1U) != 0U) { bit = (U8)(bit << 8); /* 奇数列 -> 高字节 */ }
    if (on != 0U) { *w = (U16)(*w | bit); }
    else          { *w = (U16)(*w & (U16)~bit); }
}

/* 8x8 字符：x = 0..15, y = 0..7（字符单元坐标） */
void oled_char(U8 x, U8 y, char c)
{
    U8 idx, col, page, bit;
    U8 code = (U8)c;

    if ((code < 0x20U) || (code > 0x7EU)) { code = (U8)'?'; }
    idx  = (U8)(code - 0x20U);
    page = y;

    for (col = 0U; col < 8U; col++) {
        U8 bits = font8x8[idx][col];            /* 该列的 8 个竖直像素（LSB = 顶行） */
        U8 px   = (U8)(x * 8U + col);
        for (bit = 0U; bit < 8U; bit++) {
            if ((bits & (U8)(1U << bit)) != 0U) {
                oled_pixel(px, (U8)(page * 8U + bit), 1U);
            } else {
                oled_pixel(px, (U8)(page * 8U + bit), 0U);
            }
        }
    }
}

void oled_str(U8 x, U8 y, const char *s)
{
    while ((*s != '\0') && (x < 16U)) {
        oled_char(x, y, *s);
        x++;
        s++;
    }
}

/* ---------------- 整屏刷新 ---------------- */
void oled_flush(void)
{
    U8 p, c;
    for (p = 0U; p < 8U; p++) {
        oled_cmd((U8)(0xB0U | p));  /* 页地址 */
        oled_cmd(0x00U);            /* 列地址低 4 位 = 0 */
        oled_cmd(0x10U);            /* 列地址高 4 位 = 0 */
        i2c_start();
        i2c_wr(OLED_ADDR);
        i2c_wr(0x40U);              /* 控制字节：数据 */
        for (c = 0U; c < 64U; c++) {
            U16 w = s_oled[p][c];
            i2c_wr((U8)(w & 0x00FFU));      /* 偶数列 */
            i2c_wr((U8)((w >> 8) & 0x00FFU)); /* 奇数列 */
        }
        i2c_stop();
    }
}

/* ---------------- 上电自检画面 ---------------- */
void oled_init(void)
{
    U8 i;

    oled_pins_init();
    delay_ms(20U);              /* OLED 上电稳定 */
    oled_panel_init();

    oled_clear();

    /* 四条边框 */
    for (i = 0U; i < 128U; i++) {
        oled_pixel(i, 0U, 1U);
        oled_pixel(i, 63U, 1U);
    }
    for (i = 0U; i < 64U; i++) {
        oled_pixel(0U, i, 1U);
        oled_pixel(127U, i, 1U);
    }
    /* 上排棋盘格（验证每个像素都能独立控制） */
    for (i = 0U; i < 128U; i += 2) {
        U8 j;
        for (j = 10U; j < 26U; j += 2) { oled_pixel(i, j, 1U); }
    }
    /* 文字 */
    oled_str(1U, 4U, "DSP32Music OLED");
    oled_str(1U, 6U, "SSD1306 128x64");
    oled_str(1U, 7U, "I2C SCL15 SDA16");

    oled_flush();
}


/* ---------------- 应用层主界面（改这里即可换显示内容）----------------
 *   第 0 行：曲名
 *   第 2 行：板名
 *   第 4 行：当前按键
 *   第 5 行：数码管计数
 *   第 7 行：音乐状态
 * 注意：不使用 sprintf（C2000 的 printf 会占几 KB 代码），用极简十进制转换。
 */
static void u16_str(char *dst, U16 v, U8 width)
{
    U8 i;
    for (i = 0U; i < width; i++) {
        dst[width - 1U - i] = (char)('0' + (char)(v % 10U));
        v = (U16)(v / 10U);
    }
    dst[width] = '\0';
}

void oled_show(U16 key, U16 value, U16 playing)
{
    char buf[17];

    oled_clear();

    oled_str(0U, 0U, "FUWA FUWA TIME");     /* ← 曲名 */
    oled_str(0U, 2U, "PZ-DSP28335-L");

    buf[0] = 'K'; buf[1] = 'E'; buf[2] = 'Y'; buf[3] = ':';
    buf[4] = ' '; buf[5] = 'K';
    buf[6] = (char)('0' + (char)key);
    buf[7] = '\0';
    oled_str(0U, 4U, buf);

    buf[0] = 'C'; buf[1] = 'N'; buf[2] = 'T'; buf[3] = ':';
    buf[4] = ' ';
    u16_str(&buf[5], value, 4U);
    oled_str(0U, 5U, buf);

    oled_str(0U, 7U, (playing != 0U) ? "MUSIC: PLAY" : "MUSIC: STOP");

    oled_flush();
}


/* ================= 歌词显示 =================
 *  字模：14x14（font16.h，按行存储，bit0 = 最左像素）
 *  排版：每行最多 9 字；<=9 字单行，否则两行（优先在空格处断开，保证上行不多于下行）
 *        水平居中；单行垂直居中(y=25)，两行 y=16 / y=34
 *  时钟：由调用者传入毫秒（真实时间，来自音频采样计数），只在换行时才刷屏
 */
#define FONT16_W      14U
#define FONT16_H      14U
#define LRC_SLOT      14U          /* 9*14 = 126 <= 128 */
#define LRC_9         9U           /* 每行 9 字 */

static const Glyph14 *glyph_find(U16 code)
{
    U16 lo = 0U, hi = (U16)(FONT16_COUNT - 1U);
    while (lo <= hi) {
        U16 mid = (U16)((lo + hi) >> 1);
        if (font16[mid].code == code) { return &font16[mid]; }
        if (font16[mid].code < code)  { lo = (U16)(mid + 1U); }
        else                          { if (mid == 0U) break; hi = (U16)(mid - 1U); }
    }
    return 0;
}

/* UTF-8 解码：返回码点，并把指针前移 */
static U16 utf8_next(const char **pp)
{
    const U8 *p = (const U8 *)(*pp);
    U16 c = *p;
    if (c < 0x80U) { *pp = (const char *)(p + 1); return c; }
    if ((c & 0xE0U) == 0xC0U) {
        U16 cp = (U16)(((c & 0x1FU) << 6) | (p[1] & 0x3FU));
        *pp = (const char *)(p + 2); return cp;
    }
    if ((c & 0xF0U) == 0xE0U) {
        U16 cp = (U16)(((c & 0x0FU) << 12) | ((p[1] & 0x3FU) << 6) | (p[2] & 0x3FU));
        *pp = (const char *)(p + 3); return cp;
    }
    *pp = (const char *)(p + 1); return (U16)'?';
}

/* 统计字符数（跳过 UTF-8 多字节 + 空格不计入可见宽度？这里空格计入一格） */
static U16 utf8_len(const char *s)
{
    U16 n = 0U;
    while (*s != '\0') { (void)utf8_next(&s); n++; }
    return n;
}

static void draw_glyph(U16 x, U16 y, const Glyph14 *g)
{
    U16 r, cc;
    if (g == 0) { return; }
    for (r = 0U; r < FONT16_H; r++) {
        U16 bits = g->rows[r];
        for (cc = 0U; cc < FONT16_W; cc++) {
            if ((bits & (U16)(1U << cc)) != 0U) {
                oled_pixel((U8)(x + cc), (U8)(y + r), 1U);
            }
        }
    }
}

/* 画 from 开始的 cnt 个字（空格留白） */
static void draw_run(U16 x, U16 y, const char *s, U16 cnt)
{
    U16 i;
    for (i = 0U; i < cnt; i++) {
        U16 cp = utf8_next(&s);
        if (cp != (U16)' ') { draw_glyph((U16)(x + i * LRC_SLOT), y, glyph_find(cp)); }
    }
}

/* 在空格处找最佳断点，保证 top <= bottom；优先选靠近中点且 top<=bottom 的 */
static U16 split_at_space(const char *s, U16 n)
{
    U16 i, best = 0U;
    const char *q = s;
    for (i = 0U; i < n; i++) {
        const char *save = q;
        if (utf8_next(&q) == (U16)' ') {
            U16 top = i;                 /* 空格前有 i 个字 */
            if ((top > 0U) && (top <= (U16)(n - top - 1U))) { best = top; }
        }
        (void)save;
    }
    if (best == 0U) { best = (U16)(n >> 1); }      /* 无空格或都不合适 ⇒ 取半 */
    if (best > (U16)(n - best)) { best = (U16)(n >> 1); }
    return best;
}

void oled_lyric(U32 t_ms)
{
    static U16 last = 0xFFFFU;
    U16 idx = 0U, i, n, top, x, y, off;
    const char *s;

    for (i = 0U; i < LRC_COUNT; i++) {
        if (lrc_table[i].t_ms <= t_ms) { idx = i; } else { break; }
    }
    if (t_ms < lrc_table[0].t_ms) {     /* 前奏：先留白 */
        if (last != 0xFFFEU) { last = 0xFFFEU; oled_clear(); oled_flush(); }
        return;
    }
    if (idx == last) { return; }        /* 未换行 ⇒ 不重画（省 90ms 的 I2C） */
    last = idx;

    s = lrc_table[idx].txt;
    n = utf8_len(s);
    oled_clear();

    if (n <= LRC_9) {                   /* 单行：水平 + 垂直居中 */
        x = (U16)((126U - n * LRC_SLOT) / 2U + 1U);
        y = (U16)((64U - FONT16_H) / 2U);
        draw_run(x, y, s, n);
    } else {                            /* 两行：上少下多 */
        top = split_at_space(s, n);
        if (top == 0U) { top = (U16)(n >> 1); }
        x = (U16)((126U - top * LRC_SLOT) / 2U + 1U);
        draw_run(x, 16U, s, top);

        /* 跳过断点处的空格 */
        off = 0U; { const char *q = s; while (off < top) { (void)utf8_next(&q); off++; }
                    if (*q == ' ') { q++; } else { } s = q; }
        n = (U16)(n - top - ((*s == '\0') ? 0U : 0U));
        /* 剩余字数 */
        { const char *q = s; n = utf8_len(q); }
        x = (U16)((126U - n * LRC_SLOT) / 2U + 1U);
        draw_run(x, 34U, s, n);
    }
    oled_flush();
}
