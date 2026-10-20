/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — a voice: the synth engine (two instances in STEREO), TONE (filter + distortion), the two AHD
 * envelopes, the two LFOs, the modulation matrix and VARIATION, as Source/DSP/Voice.h, ModSources.h, Mappings.h and
 * Tone.h of Drummatic 0.2.3. The sampler is not part of this port: BLEND, the sampler's knobs and START / END are gone
 * with it (the synth always plays at full level, as Drummatic with BLEND at 0). */
#ifndef DM_VOICE_H
#define DM_VOICE_H
#include "dm_engines.h"

/* modulation targets (normalised 0..1 in the kit) */
enum {
    T_PITCH, T_P1, T_P2, T_P3, T_P4, T_CUTOFF, T_RES, T_DIST,
    T_E1A, T_E1H, T_E1D, T_E2A, T_E2H, T_E2D,
    T_L1SPD, T_L1AMP, T_L2SPD, T_L2AMP,
    T_PAN, T_GAIN, T_DSEND, T_RSEND,
    DT_N
};
enum { S_RANDOM, S_VELOCITY, S_MACRO1, S_MACRO2, S_ENV1, S_ENV2, S_LFO1, S_LFO2, DS_N };
enum { FM_SCREAM_LP, FM_DARK_LP, FM_SCREAM_HP, FM_DARK_HP, FM_SCREAM_BP, FM_DARK_BP,
       FM_COMB_FBP, FM_COMB_FBM, FM_COMB_FFP, FM_COMB_FFM, FM_COMB_AP, FM_COUNT };
enum { DM_TAPE, DM_TUBE, DM_BITCRUSH, DM_RATECRUSH, DM_WAVEFOLD, DM_PHASE, DM_SQUARE, DM_DROPOUT, DM_MINIGLITCH,
       DM_MEGAGLITCH, DM_CONTRAST, DM_COUNT };
enum { LW_WAVETABLE, LW_TRITILT, LW_PULSE, LW_SMOOTHRND, LW_COUNT };
enum { LR_FREE, LR_ALWAYS, LR_SOFT, LR_SH, LR_ONESHOT, LR_COUNT };

typedef struct { float lo, hi; uint8_t exp; float def; } dm_map_t;
extern const dm_map_t DM_MAP[DT_N];
extern const char *const DM_TNAME[DT_N];
extern const char *const DM_SNAME[DS_N];
extern const char *const DM_FNAME[FM_COUNT];
extern const char *const DM_DNAME[DM_COUNT];
float dm_denorm(uint32_t t, float n);
float dm_norm(uint32_t t, float v);
float dm_lfo_hz(float n);
#define DM_NLFODIV 13
extern const char *const DM_LFODIV_NAME[DM_NLFODIV];
extern const float DM_LFODIV_BEATS[DM_NLFODIV];
static inline uint32_t dm_lfo_div(float n) { uint32_t i = (uint32_t)(dm_clamp01(n) * DM_NLFODIV); return i < DM_NLFODIV ? i : DM_NLFODIV - 1u; }

/* the settings of a voice the audio reads (the kit plus the Performance page) */
typedef struct {
    float base[DT_N];
    const float (*depth)[DT_N];          /* the kit's matrix [DS_N][DT_N] (not copied) */
    float e1curve, e1vel, e2curve, e2vel, l1shape, l2shape, macro1, macro2, variation, modamp;
    uint32_t locks;                      /* bit t: target t is locked (no VARIATION) */
    uint8_t engine, env2, stereo, post, fmode, dmode;
    uint8_t l1sync, l1bip, l1wave, l1reset, l2sync, l2bip, l2wave, l2reset;
} dm_vset_t;

/* what a hit carries (sequencer lanes, MIDI) */
typedef struct {
    float vel, pitch, envscale, pan, var, sdly, srev, macro2;
    uint8_t accent;
} dm_trig_t;

typedef struct {
    uint8_t stage;
    float value, peak, start, pos;
    float attack, hold, decay, curve, tscale;
    float inv_a, hold_end, inv_d, r, ex;     /* the stage's constants (ahd_prepare) */
} dm_ahd_t;

typedef struct {
    float phase, shape, amp, held, rnda, rndb;
    uint8_t bip, wave, reset, done;
    dm_rng_t rng;
} dm_lfo_t;

typedef struct {
    dm_svf_t svf;
    dm_onepole_t dark;
    dm_dline_t comb;                     /* comb.b 0: no line (the comb modes pass the input) */
} dm_filt_t;

typedef struct {
    float held, acc, last, gpos, gvel, gtime, gprev, gxf, gmax;
    uint8_t dropping;
    dm_rng_t rng, grng, drng;            /* MINIGLITCH, MEGAGLITCH (per channel), DROPOUT */
    dm_dline_t micro, gbuf;              /* PHASE / MINI-MEGA GLITCH: the arena's lines */
    /* coefficients from the amount (ctrl) */
    float k1, k2, k3;
} dm_dist_t;

#ifndef DM_ARENA
#define DM_ARENA 7168u                   /* int16 samples of delay line per voice (KOMBUCHA / COMBINATORIC, combs, glitch) */
#endif

typedef struct {
    dm_eng_t eng[2];
    dm_filt_t filt[2];
    dm_dist_t dist[2];
    dm_onepole_t dc[2];
    dm_ahd_t env1, env2;
    dm_lfo_t lfo[2];
    dm_trig_t trig;
    dm_rng_t rng;
    float rand_val, vel_val, lfo_val[2];
    float var_off[DT_N], cur[DT_N], cur_dist;
    float g_l, g_r, s_d, s_r;            /* gains of the block (pan law x voice gain) and the sends */
    float comb_d, comb_g, bpk;           /* the filter's constants of the block */
    float last_n[DT_N];                  /* the targets' normalised values last computed (denorm only on a change) */
    float k_cut, k_res, k_dist, k_gain, k_pan;   /* what the block's constants were computed from */
    uint8_t k_fm, k_dm;
    float choke_fade, level;
    uint8_t choking, nch, lay_eng, lay_stereo, lay_fmode, lay_dmode, laid;
    int16_t *arena;
} dm_voice_t;

void dm_voice_init(dm_voice_t *v, uint32_t index, int16_t *arena);
void dm_voice_trigger(dm_voice_t *v, const dm_vset_t *s, const dm_trig_t *t, float bpm);
void dm_voice_choke(dm_voice_t *v);
void dm_voice_transport_start(dm_voice_t *v);
int dm_voice_active(const dm_voice_t *v);
/* the control tick (once a block of DM_CTL samples, before render) */
void dm_voice_ctrl(dm_voice_t *v, const dm_vset_t *s, float bpm);
/* n samples added into out (L R), the delay and reverb sends */
void dm_voice_render(dm_voice_t *v, const dm_vset_t *s, float *outl, float *outr, float *dl, float *dr, float *rl,
                     float *rr, uint32_t n);
#define DM_CTL 32u
#endif
