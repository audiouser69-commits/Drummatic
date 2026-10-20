/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the machine: eight voices played by their sequencers (Source/DSP/Sequencer.h) and by MIDI, the
 * send effects and the master (Source/DSP/DrumMachine.h). dm_machine_render is the audio ISR's: a half buffer at a
 * time it reads the kit (with the Performance page), runs the sequencers, the voices and the effects. */
#ifndef DM_MACHINE_H
#define DM_MACHINE_H
#include "dm_kit.h"
#include "dm_fx.h"

#define DM_HALF 128u                     /* samples per call (the FM-1's I2S half buffer) */

/* clock source */
enum { DM_CLK_INT, DM_CLK_EXT };
/* actions the audio side asks the main loop for (MIDI channel 11) */
enum { DA_NONE, DA_PREV_KIT, DA_NEXT_KIT, DA_RANDOM_KIT, DA_RANDOMIZE_ALL, DA_PERF_RANDOM, DA_PERF_RESET,
       DA_PERF_COMMIT, DA_RANDOMIZE_V0 = 16 };

typedef struct {
    /* read by the UI */
    volatile int16_t play_step[DM_NV];   /* the step playing (-1: none) */
    volatile uint32_t trig_count[DM_NV]; /* hits so far (the UI flashes on a change) */
    volatile float level[DM_NV];
    volatile uint8_t playing, clk_ext_seen;
    volatile float bpm_now;              /* the tempo playing (the kit's, or the clock's) */
    volatile uint32_t cpu_q8;
    /* set by the UI / MIDI */
    volatile uint8_t clk;                /* DM_CLK_INT / EXT */
    volatile uint8_t mom_mute[DM_NV], mom_solo[DM_NV];
    volatile int8_t audition;            /* a voice to play now (-1: none) */
    volatile uint8_t want_play;          /* the PLAY switch: 1 play, 0 stop */
    volatile uint8_t act_q[16], act_w, act_r;
} dm_machine_t;
extern dm_machine_t dm_m;

void dm_machine_init(void);
/* one half buffer: out gets DM_HALF stereo frames (L R interleaved), +-1.0 full scale */
void dm_machine_render(float *out);
/* MIDI from the rings (status, data 1, data 2); channel messages and the realtime ones */
void dm_machine_midi(uint32_t status, uint32_t d1, uint32_t d2);
void dm_machine_audition(uint32_t v, float vel);
int dm_machine_action(void);             /* main loop: the next DA_* (DA_NONE: none) */
uint32_t dm_machine_hits(uint32_t v, uint8_t *hits, uint32_t n);   /* the pattern of voice v (UI); its steps */
#endif
