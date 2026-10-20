/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the 25 synth engines of Drummatic 0.2.3 (Source/DSP/SynthEngines*.h), in C.
 * Each engine is split in three: trigger (a hit), ctrl (its coefficients from the frequency and the four knobs, once
 * a control block of 32 samples and right after a hit) and run (one sample). The sounds follow the C++ originals
 * operation by operation; what moved to the control rate are the coefficient computations the originals repeated on
 * every sample with the same inputs (pow, exp, tan of the knobs), which the knobs only change at the control rate. */
#ifndef DM_ENGINES_H
#define DM_ENGINES_H
#include "dm_math.h"

enum {
    ENG_KICK, ENG_SNARE, ENG_HAT, ENG_CLAP, ENG_FM, ENG_MODAL, ENG_COWBELL, ENG_ZAP, ENG_PILLOW,
    ENG_TRIPWIRE, ENG_RADIATOR, ENG_KOMBUCHA, ENG_LPG, ENG_BUILDER, ENG_ADDITIVE,
    ENG_CHAOS, ENG_VOSIM, ENG_SUMSYN, ENG_RESON, ENG_COMBINATORIC, ENG_TERRAINING, ENG_SHAME, ENG_ENTANGLED,
    ENG_HAYWIRE, ENG_SCRAPYARD,
    ENG_COUNT
};

typedef struct { const char *name; const char *k[4]; uint8_t octave_down; } dm_eng_info_t;
extern const dm_eng_info_t DM_ENG_INFO[ENG_COUNT];

typedef struct { float v, c; } dm_denv_t;          /* DecayEnv: next() returns v, then v *= c */
static inline float dm_denv_next(dm_denv_t *e) { float o = e->v; e->v *= e->c; return o; }

#define DM_ADD_N 24
#define DM_MODAL_N 8

typedef struct {
    uint8_t id, vary;            /* engine; MODAL: draw the partials' levels at the next ctrl */
    dm_rng_t rng;
    float f, p[4], vel;          /* the frequency and the knobs of the last ctrl */
    float noise_k;               /* (see each engine) */
    dm_dline_t dl;               /* KOMBUCHA, COMBINATORIC: the arena's line (dl.b 0: none) */
    union {
        struct { uint32_t ph, sub; float inc; dm_denv_t pe; float k1; } kick;
        struct { uint32_t ph; float inc, k, rate, acc, held; dm_denv_t pe, ne; dm_svf_t hp, lp; } snare;
        struct { uint32_t ph[6], inc[6]; float rate, acc, held; dm_svf_t bp, hp; } hat;
        struct { int32_t left, cnt, claps, spacing; dm_denv_t burst, tail; dm_svf_t bp; dm_onepole_t lp; } clap;
        struct { uint32_t car, mod, cinc, minc; float last, depth0, fb0, envk; dm_denv_t env; } fm;
        struct { uint32_t ph[DM_MODAL_N], inc[DM_MODAL_N]; float amp[DM_MODAL_N], c[DM_MODAL_N], g[DM_MODAL_N];
                 uint8_t on[DM_MODAL_N]; } modal;
        struct { uint32_t p1, p2, i1, i2; float namt, camt; dm_denv_t click; dm_svf_t hp, bp; } cow;
        struct { uint32_t ph, ov, sub; float inc, k, camt, mix, held; int32_t cnt; dm_denv_t pe, click; } zap;
        struct { uint32_t ph, sub; float inc, camt; dm_denv_t pe, click; dm_pink_t pink; dm_svf_t lp1, lp2; } pillow;
        struct { uint32_t ph; float inc, drive, norm, tamt; dm_denv_t e1, e2, tr; } trip;
        struct { uint32_t ph, ov, sub; float inc, k, tamt; dm_denv_t pe, tick; dm_onepole_t lp; } rad;
        struct { float rate, acc, held, fb, k, f2; dm_denv_t burst, pe; dm_onepole_t damp; } kom;
        struct { uint32_t ph; float inc, fold, veloK, dampK; dm_denv_t env; dm_svf_t lp; } lpg;
        struct { uint32_t ph; float inc; int32_t hp; dm_svf_t nf; } bld;
        struct { uint32_t ph[DM_ADD_N], inc[DM_ADD_N]; float a[DM_ADD_N], norm; int32_t n; dm_denv_t env; } add;
        struct { uint32_t a, b; float lastA, lastB, c0, ck, r; dm_denv_t env; dm_svf_t filt; } chaos;
        struct { uint32_t win, s1, s2, iw, i1, i2; dm_denv_t env; } vosim;
        struct { uint32_t o1, o2, o3, i1, i2, i3; float a, pm0, pmk, c1, aa, fold; dm_denv_t env; } sum;
        struct { dm_svf_t bank[6]; float amp[6]; uint8_t on[6]; dm_denv_t burst; } reson;
        struct { uint32_t osc, inc; float ratio, fb, k, fm; dm_denv_t pe; } comb;
        struct { uint32_t a, b, ia, ib; } terr;
        struct { uint32_t a, b, ib; float last, held, r2; } shame;
        struct { uint32_t a, b, ia, ib; float last; } ent;
        struct { uint32_t o1, o2; float i1, i2, k, ta, tb, ramt; dm_denv_t pe, re, body; dm_svf_t bp1, bp2; } hay;
        struct { uint32_t ph[8], mod, minc; float base[8], amp[8], zk, mamt; dm_denv_t zap; } scrap;
    } u;
} dm_eng_t;

void dm_eng_reset(dm_eng_t *e, uint32_t id, uint32_t seed, int16_t *line, uint32_t line_n);
void dm_eng_trigger(dm_eng_t *e, float vel);
void dm_eng_ctrl(dm_eng_t *e, float f, const float *p);
float dm_eng_run(dm_eng_t *e);
uint32_t dm_eng_needs_line(uint32_t id);      /* samples of delay line the engine wants (0: none) */
float dm_eng_base_freq(uint32_t id);
#endif
