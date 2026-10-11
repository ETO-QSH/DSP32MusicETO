/* audio.h -- hardware-PWM audio engine (dual channel)
   hardware: ePWM3, carrier 25 kHz (TBPRD 5999)
             channel 0: CMPA (true addr 0x6889) -> GPIO4 = right hand
             channel 1: CMPB (true addr 0x688A) -> GPIO5 = left hand
   note: GPIO4/GPIO5 are also LCD DB2/DB3, so no LCD writes while playing */
#ifndef AUDIO_H_
#define AUDIO_H_

#include "bsp.h"

#define AUDIO_VOICES        6U          /* voice slots (4 chord + 2 release tails) */
#define AUDIO_MAX_CHORD     4U
#define AUDIO_AMP_PEAK      4000U

/* legacy chord type (kept for reference; the packed tables are used instead) */
typedef struct {
    U16 freq[AUDIO_MAX_CHORD];
    U16 ms;
} CHORD_NOTE;

void audio_init(void);                  /* configure ePWM3 + PIE (no pin mux here) */
void audio_stop(void);                  /* silence, hand GPIO4/GPIO5 back to the LCD */
void audio_start(void);                 /* non-blocking start (muxes the pins) */
void audio_task(void);                  /* call every main-loop pass: fill both rings */
U16  audio_is_playing(void);

extern volatile U16 g_audio_events;
extern volatile U16 g_audio_freq;       /* highest active voice (Hz): for the LED effect */
extern volatile U32 g_audio_samples;

#endif