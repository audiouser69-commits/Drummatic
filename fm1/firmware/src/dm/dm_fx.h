/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the send effects and the master (Source/DSP/Effects.h of Drummatic 0.2.3):
 * Shatter Delay, Headspace Reverb (8-line FDN), Master (MAXIMIZE, 3-band EQ, gain, clipping).
 * The FM-1 has 432 KB of RAM: the delay and the reverb run at half the sample rate (22.05 kHz, int16 lines; their
 * inputs averaged in pairs, their outputs interpolated), the delay holds 1.5 s, the predelay 0.5 s, the reverb's
 * lines 2.5 times their base length (the original's 4: SIZE above 2.5 is clamped, which only S.RATE below 0.64 on
 * HALL reaches). */
#ifndef DM_FX_H
#define DM_FX_H
#include "dm_math.h"

#define DM_FS2 22050.0f
#define DM_DLY_N 32768u                 /* 1.49 s at 22.05 kHz, per channel */
#define DM_PRE_N 11025u                 /* 0.5 s */
#define DM_REV_K 2.5f

typedef struct {                        /* Tilt EQ (-1..1) at 800 Hz: the gains of the block */
    dm_onepole_t lp;
    float glo, ghi;
} dm_tilt_t;
void dm_tilt_set(dm_tilt_t *t, float tilt);
static inline float dm_tilt_run(dm_tilt_t *t, float x) { float lo = dm_op_lp(&t->lp, x); return lo * t->glo + (x - lo) * t->ghi; }

typedef struct {
    dm_dline_t dl[2];
    float cur[2], prev[2], xf[2], gst[2], gvel[2], duck, sracc, held[2], yprev[2];
    dm_tilt_t tl[2];
    dm_rng_t rng;
    /* the block's settings */
    float time, fb, glitch, ducking, rate, gain;
    uint8_t stereo, pong;
    uint32_t quiet;                     /* samples of silence in and out (asleep past the line's length) */
} dm_delay_t;

typedef struct {
    dm_dline_t pre, line[8];
    dm_onepole_t damp[8];
    float lens[8], g[8], size, sracc, held, lfoph, yprev[2], predelay, rate, gain;
    dm_tilt_t in, outl, outr;
    uint32_t quiet;
} dm_reverb_t;

typedef struct {
    float env, att, rel, thr, mk, maximize;
    dm_svf_t eq[3][2];
    float eqg[3];                       /* gain - 1 of each band; 0: off */
    float gain;
    uint8_t clip;
} dm_master_t;

void dm_delay_init(dm_delay_t *d, int16_t *buf_l, int16_t *buf_r);
/* time in s, fb 0..1, glitch 0..1, fbtilt -1..1, ducking 0..1, srate 0..1, gain linear */
void dm_delay_set(dm_delay_t *d, float time, float fb, float glitch, float fbtilt, float ducking, float srate, float gain,
                  int stereo, int pong);
/* n (even) samples: in -> out (out is written, not added) */
void dm_delay_run(dm_delay_t *d, const float *inl, const float *inr, float *outl, float *outr, uint32_t n);

void dm_reverb_init(dm_reverb_t *r, int16_t *pre, int16_t *lines);
uint32_t dm_reverb_lines_len(void);     /* int16 samples the eight lines take */
/* mode 0 DARK, 1 BRIGHT, 2 HALL, 3 TIN */
void dm_reverb_set(dm_reverb_t *r, float predelay, float decay, float srate, float intilt, float outtilt, float gain, int mode);
void dm_reverb_run(dm_reverb_t *r, const float *inl, const float *inr, float *outl, float *outr, uint32_t n);

void dm_master_init(dm_master_t *m);
void dm_master_set(dm_master_t *m, float maximize, float low, float mid, float high, float gaindb, int clip);
void dm_master_run(dm_master_t *m, float *l, float *r, uint32_t n);
#endif
