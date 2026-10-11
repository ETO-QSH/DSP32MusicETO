/* ============================================================================
 *  HardWare/AUDIO/audio.h —— 硬件 PWM 音频引擎接口（官方库版）
 *  寄存器访问：EPwm4Regs / SysCtrlRegs / GpioCtrlRegs；CMPA 用实测真地址 0x68C9
 * ========================================================================== */
#ifndef AUDIO_H_
#define AUDIO_H_

#include "bsp.h"

#define AUDIO_VOICES        6U          /* 声部数（4 当前和弦 + 2 延音尾巴） */
#define AUDIO_MAX_CHORD     4U
#define AUDIO_AMP_PEAK      4000U

/* 和弦事件：最多 4 个声部（Hz，0 = 该声部无音）+ 时值(ms) */
typedef struct {
    U16 freq[AUDIO_MAX_CHORD];
    U16 ms;
} CHORD_NOTE;

void audio_init(void);                          /* GPIO6→EPWM4A + ePWM4 + Timer0 */
void audio_stop(void);                          /* 停声并把 GPIO6 交还普通 IO */
void audio_start(void);       /* 非阻塞启动播放 */
void audio_task(void);                          /* 主循环周期调用：合成填缓冲 */
U16  audio_is_playing(void);
U16  audio_key_check(void);                     /* 按任意键停止，返回 1 */

extern volatile U16 g_audio_events;
extern volatile U16 g_audio_freq;                     /* 当前最高声部频率(Hz) */
extern volatile U32 g_audio_samples;

#endif /* AUDIO_H_ */
