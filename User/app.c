/* ============================================================================
 *  app.c —— 应用层（把 HardWare 各模块组合成完整演示）
 * ----------------------------------------------------------------------------
 *  功能：
 *    * 数码管显示计数器 0..9999（约每 0.4 秒 +1）
 *    * K3 ：起停音乐（播放中再按 = 停止），并清零计数
 *    * 其它键 K1/K2/K4..K9 ：切换 LED 效果；【未播放时也清零计数】
 *    * 任意按键：LCD 第 2 行第 16 格显示按键号
 *    * 音乐：ePWM4A 150kHz 载波 + 周期中断喂 CMPA + 环形缓冲
 *            （打包曲表 3.4KB，播完自动从头循环）
 *    * 播放期间 GPIO6 归 ePWM，LCD 冻结；停止后不自动刷（只在按键时刷）
 *  说明：
 *    * LCD 采用"整行重写"：定位命令只用 goto(line,0)（=0xC0，bit1=0）
 *      ⇒ 即使 LCD 的 DB1(GPIO3) 接触不良，字符也不会跑位
 *    * 开机约 0.6s 后再补做一次 LCD 初始化并重画（自愈）：
 *      复位瞬间 LCD 仍带电，内部状态可能不确定 ⇒ 这一次补写把它纠正回来
 * ========================================================================== */
#include "app.h"
#include "delay.h"
#include "led.h"
#include "smg.h"
#include "key.h"
#include "lcd1602.h"
#include "audio.h"

#define APP_LOOP_MS         8U      /* 一轮主循环的名义时长(ms)，用于虚拟计时 */
#define APP_COUNT_MS        400U    /* 计数 +1 间隔(虚拟 ms) */
#define APP_LCD_HEAL_MS     500U    /* 开机后补写 LCD 的时刻（虚拟 ms）*/

volatile U16 g_value = 0U;
volatile U16 g_effect = 0U;
volatile U16 g_phase = 0U;
volatile U16 g_key = 1U;
volatile U16 g_key_log[16];
volatile U16 g_key_count = 0U;

static U16 s_ms = 0U;
static U16 s_last_count = 0U;
static U16 s_last_step = 0U;
static U16 s_lcd_dirty = 1U;
static U16 s_lcd_healed = 0U;

/* 画 LCD 两行固定内容（第 16 格 = 按键数字）*/
static void lcd_draw_all(U16 k)
{
    const char *p = "PZ-DSP28335-L K";
    char kb[17];
    U16 i;

    lcd1602_puts_at(LCD1602_LINE1, 0U, "Fuwa Fuwa Time!");
    lcd1602_puts(" ");                              /* 15+1 = 16 列 */

    for (i = 0U; i < 15U; i++) { kb[i] = p[i]; }
    kb[15] = (char)('0' + (char)k);                 /* 第 16 格 = 按键数字 */
    kb[16] = 0;
    lcd1602_puts_at(LCD1602_LINE2, 0U, kb);         /* 定位只用 0xC0 ⇒ 抗 DB1 抖动 */
}

void app_init(void)
{
    led_init();
    smg_init();
    key_init();
    lcd1602_init();

    led_effect(0U, 0U);
    smg_show(0U);

    lcd1602_clear();
    lcd_draw_all(1U);                               /* 第 16 格初始 K1 */

    delay_ms(800U);

    s_lcd_dirty = 0U;                               /* 开机已画好，无需再请求刷新 */
}

void app_task(void)
{
    U16 ev;
    U16 i;

    /* 1) 数码管：每次只刷 1 位（非阻塞），并推进虚拟时间 */
    smg_show(g_value);
    s_ms = (U16)(s_ms + APP_LOOP_MS);

    /* 2) 音乐：补充合成（追着中断的读指针填环形缓冲）*/
    audio_task();

    /* 3) 计数 0..9999（不在这里请求刷 LCD ⇒ 不播放时 LCD 保持静止）*/
    if ((U16)(s_ms - s_last_count) >= ((audio_is_playing() != 0U) ? APP_COUNT_MS : (U16)(APP_COUNT_MS * 16U))) {
        s_last_count = s_ms;
        g_value = (U16)((g_value + 1U) % 10000U);
    }

    /* 3b) 开机自愈：补做一次 LCD 初始化并重画（复位瞬间 LCD 仍带电可能花屏）*/
    if ((s_lcd_healed == 0U) && ((U16)(s_ms - APP_LCD_HEAL_MS) < 0x8000U)) {
        s_lcd_healed = 1U;
        if (audio_is_playing() == 0U) {
            lcd1602_init();
            lcd1602_clear();
            lcd_draw_all(g_key);
        }
    }

    /* 4) 按键 */
    ev = key_task();

    if (ev == 3U) {                                 /* K3：起停音乐 + 清零计数 */
        g_key = 3U;
        if (audio_is_playing() != 0U) {
            audio_stop();
            lcd_draw_all(3U);                       /* GPIO6 已交还，安全 */
        } else {
            lcd_draw_all(3U);                       /* 启动前写，GPIO6 还归 LCD */
            audio_start();
        }
        g_value = 0U;
        g_key_count++;
        ev = 0U;
    }

    if (ev != 0U) {                                 /* 其它键：切 LED 效果 */
        g_key = ev;
        if (audio_is_playing() == 0U) {             /* 未播放时才写 LCD（播放中会抢 GPIO6）*/
            lcd_draw_all(ev);
        }
        g_effect = (U16)(ev - 1U);
        if (ev == 9U) { g_effect = 2U; }            /* K9 使用与 K3 相同的效果编号 */
        g_key_count++;
        for (i = 15U; i > 0U; i--) {
            g_key_log[i] = g_key_log[i - 1U];
        }
        g_key_log[0] = ev;
        if (audio_is_playing() == 0U) { g_value = 0U; }   /* 未播放时任意键也清零 */
        g_phase = 0U;
        led_effect(g_effect, g_phase);
        s_last_step = s_ms;
    }

    /* 5) LED 效果步进 */
        {
        U16 lperiod = led_effect_period(g_effect);
        if (audio_is_playing() == 0U) {                 /* 未播放：灯效步进同样降为 1/16 速率 */
            lperiod = (lperiod < 0x1000U) ? (U16)(lperiod * 16U) : 0xFFFFU;
        }
        if ((U16)(s_ms - s_last_step) >= lperiod) {
            s_last_step = s_ms;
            g_phase++;
            led_effect(g_effect, g_phase);
        }
    }
}
