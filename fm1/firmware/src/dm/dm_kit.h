/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the kit: everything a Drummatic kit holds (Source/Params.cpp, the EXTRA tree of
 * PluginProcessor.cpp) minus the sampler and the multi-outputs: eight voices (sound, modulation, locks, sequencer
 * settings, the sequence and its ten lanes), the FX and master, the Performance page. The UI edits it; the audio
 * reads it (dm_machine.c: the kit plus the Performance page make the voices' settings, every half buffer). */
#ifndef DM_KIT_H
#define DM_KIT_H
#include "dm_voice.h"

#define DM_NV 8
#define DM_MAXSTEPS 128
enum { L_VEL, L_SHIFT, L_PITCH, L_RATCH, L_ENV, L_PAN, L_VAR, L_PROB, L_SENDS, L_MACRO2, DM_NLANE };
enum { LS_SYNC, LS_FREE, LS_TRIG, LS_RETRIG, LS_COUNT };
typedef struct { const char *name; int8_t lo, hi, def; } dm_lane_info_t;
extern const dm_lane_info_t DM_LANE[DM_NLANE];
extern const char *const DM_LSYNC_NAME[LS_COUNT];

#define DM_NSPEED 10
#define DM_NMULT 7
extern const char *const DM_SPEED_NAME[DM_NSPEED];
extern const float DM_SPEED_BEATS[DM_NSPEED];
extern const char *const DM_MULT_NAME[DM_NMULT];
extern const float DM_MULT_F[DM_NMULT];

/* the Performance page (without the sampler's five) */
enum { PF_ENVA, PF_ENVH, PF_ENVD, PF_ENVC, PF_SYNPITCH, PF_SYN1, PF_SYN2, PF_SYN3, PF_SYN4,
       PF_MACRO1, PF_MACRO2, PF_VAR, PF_MODAMP, PF_DISTORT, PF_CUTOFF, PF_RES,
       PF_STEPS, PF_FILL, PF_PATTERN, PF_DENSITY, PF_PROB, PF_RATCHET, PF_N };
extern const char *const DM_PF_NAME[PF_N];

typedef struct {
    /* sound */
    float base[DT_N];                    /* normalised */
    float depth[DS_N][DT_N];             /* the modulation matrix, -1..1 */
    uint32_t locks;                      /* bit per target */
    float e1curve, e1vel, e2curve, e2vel, l1shape, l2shape, macro1, macro2, variation;
    uint8_t engine, env2, stereo, post, fmode, dmode;
    uint8_t l1sync, l1bip, l1wave, l1reset, l2sync, l2bip, l2wave, l2reset;
    uint8_t choke, link;                 /* 0 none, 1..8 the voice */
    uint8_t pf_enable, pf_flip, mute, solo;
    /* sequencer */
    int8_t steps, fill, pattern, rotate; /* 1..64, 0..64, 1..64, -32..32 */
    uint8_t invert, speed, mult, length, snap, reset_on, reset_len;   /* length, reset_len 1..128 */
    float shift, swing, shape, density;  /* -1..1 */
    uint32_t manual[DM_MAXSTEPS / 32];   /* manual trigs */
    int8_t lane[DM_NLANE][DM_MAXSTEPS];
    uint8_t lane_len[DM_NLANE], lane_sync[DM_NLANE];
} dm_vkit_t;

typedef struct {
    uint8_t d_sync, d_div, d_stereo, d_pong;
    float d_time_ms, d_fb, d_glitch, d_fbtilt, d_duck, d_srate, d_gain_db, d_torev;
    uint8_t r_sync, r_div, r_mode, m_clip;
    float r_pre_ms, r_decay, r_srate, r_intilt, r_outtilt, r_gain_db;
    float m_max, m_low, m_mid, m_high, m_gain;
    float humanize, bpm, rnd_depth;
    float perf[PF_N], pf_depth, pf_rnd;
    uint8_t pf_enabled, swing_dummy[3];
    uint32_t perf_lock;                  /* bit per Performance control: left out of its DADO */
} dm_gkit_t;

typedef struct {
    char name[12];
    dm_vkit_t v[DM_NV];
    dm_gkit_t g;
} dm_kit_t;

extern dm_kit_t dm_kit;                  /* the kit playing */
extern dm_vkit_t dm_scratch_voice;

void dm_kit_defaults(dm_kit_t *k);       /* Drummatic's power-on kit */
void dm_kit_reference(dm_kit_t *k);      /* the reference kit ("Dael" intro, see dm_kit.c) */
void dm_voice_defaults(dm_vkit_t *v, uint32_t index);   /* the voice's sound as at power-on (sequence kept) */
void dm_voice_reset_sound(dm_vkit_t *v, uint32_t index);/* RESET: the sound to its defaults, the modulation and
                                                          * locks cleared, the sequencer settings kept */
void dm_lane_clear(dm_vkit_t *v, uint32_t lane);
void dm_voice_copy_sound(dm_vkit_t *dst, const dm_vkit_t *src);
uint32_t dm_seq_length(int steps, int length, int snap);
static inline int dm_manual(const dm_vkit_t *v, uint32_t s) { return (int)((v->manual[(s >> 5) & 3u] >> (s & 31u)) & 1u); }
static inline void dm_manual_set(dm_vkit_t *v, uint32_t s, int on)
{
    uint32_t b = 1u << (s & 31u);
    if (on)
        v->manual[(s >> 5) & 3u] |= b;
    else
        v->manual[(s >> 5) & 3u] &= ~b;
}

/* randomisation (DADO), Performance (dm_kit.c); r: the caller's generator */
void dm_randomize_voice(dm_kit_t *k, uint32_t v, dm_rng_t *r);
void dm_randomize_all(dm_kit_t *k, dm_rng_t *r);
void dm_perf_randomize(dm_kit_t *k, dm_rng_t *r);
void dm_perf_reset(dm_kit_t *k);
void dm_perf_commit(dm_kit_t *k);
void dm_lane_randomize(dm_vkit_t *v, uint32_t lane, dm_rng_t *r);
/* the voice's settings for the audio: the kit plus the Performance page (k: DEPTH x FLIP) */
typedef struct {
    int steps, fill, pattern, rotate, invert, speed, mult, length, snap, reset_on, reset_len, ratch_off;
    float shift, swing, shape, density, prob_off;
} dm_seqset_t;
void dm_effective(const dm_kit_t *k, uint32_t v, dm_vset_t *s, dm_seqset_t *q);

/* kits in flash: a compact form (dm_kit.c) */
#define DM_KIT_BLOB_MAX 7680u
uint32_t dm_kit_pack(const dm_kit_t *k, uint8_t *out, uint32_t max);
int dm_kit_unpack(dm_kit_t *k, const uint8_t *in, uint32_t n);
uint32_t dm_voice_pack(const dm_vkit_t *v, uint8_t *out, uint32_t max);
int dm_voice_unpack(dm_vkit_t *v, const uint8_t *in, uint32_t n);
#endif
