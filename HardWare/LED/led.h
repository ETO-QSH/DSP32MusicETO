/* ============================================================================
 *  led.h —— 板上那一排 7 个 LED（含 9 种显示效果）
 * ----------------------------------------------------------------------------
 *  硬件（普中 PZ-DSP28335-L，依据官方教程第 8 章 LED_Init 代码）：
 *      D1..D5 = GPIO68, 67, 66, 65, 64   （C 组）
 *      D6, D7 = GPIO10, 11               （A 组）
 *      输出低电平 = 点亮
 * ========================================================================== */
#ifndef LED_H_
#define LED_H_

#include "bsp.h"

#define LED_NUM         7U

/* 9 种效果（0..8），与矩阵按键 K1..K9 对应 */
#define LED_EFFECT_NUM  9U

void led_init(void);
void led_all_on(void);
void led_all_off(void);

/* mask: bit0=D1 ... bit6=D7，位为 1 表示点亮 */
void led_mask(U16 mask);

/* 按 "效果号 + 步进序号" 刷新 LED */
void led_effect(U16 effect, U16 phase);

/* 该效果的步进间隔(ms) */
U16 led_effect_period(U16 effect);

/* 效果名（ASCII，可显示在 LCD1602 上） */
const char *led_effect_name(U16 effect);

#endif /* LED_H_ */
