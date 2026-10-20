/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the kit (see dm_kit.h): defaults, DADO, Performance, the compact form for the flash. */
#include "dm_kit.h"
#ifndef DM_KIT_POOL
#define DM_KIT_POOL
#endif

dm_kit_t dm_kit;

const dm_lane_info_t DM_LANE[DM_NLANE] = {
    {"VEL", 0, 127, 100}, {"SHIFT", -100, 100, 0}, {"PITCH", -24, 24, 0}, {"RATCH", 1, 8, 1}, {"ENV", -100, 100, 0},
    {"PAN", -100, 100, 0}, {"VAR", 0, 100, 0}, {"PROB", -100, 100, 0}, {"SENDS", -100, 100, 0}, {"MACRO 2", 0, 100, 0},
};
const char *const DM_LSYNC_NAME[LS_COUNT] = {"SYNC", "FREE", "TRIG", "RETRIG"};
const char *const DM_SPEED_NAME[DM_NSPEED] = {"1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T"};
const float DM_SPEED_BEATS[DM_NSPEED] = {4, 2, 1, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f, 1.0f / 6.0f, 1.0f / 12.0f};
const char *const DM_MULT_NAME[DM_NMULT] = {"/4", "/3", "/2", "1X", "2X", "3X", "4X"};
const float DM_MULT_F[DM_NMULT] = {0.25f, 1.0f / 3.0f, 0.5f, 1, 2, 3, 4};
const char *const DM_PF_NAME[PF_N] = {"ATTACK", "HOLD", "DECAY", "CURVE", "PITCH", "SYNTH 1", "SYNTH 2", "SYNTH 3",
                                      "SYNTH 4", "MACRO 1", "MACRO 2", "VARIAT", "MOD AMP", "DISTORT", "CUTOFF", "RES",
                                      "STEPS", "FILL", "PATTERN", "DENSITY", "PROB", "RATCHET"};

static void zero(void *p, uint32_t n)
{
    uint8_t *b = p;
    while (n--)
        *b++ = 0;
}

/* Drummatic's power-on kit (Params.h voiceDefaults): engine, ENV 1 decay, FILL, ROTATE, gain, pan */
static const struct { uint8_t eng; float decay; int8_t fill, rotate; float gain, pan; } VDEF[DM_NV] = {
    {ENG_KICK, 0.35f, 4, 0, -2, 0}, {ENG_SNARE, 0.22f, 2, 4, -4, 0}, {ENG_HAT, 0.05f, 8, 2, -9, 0.15f},
    {ENG_HAT, 0.35f, 0, 0, -10, -0.15f}, {ENG_CLAP, 0.3f, 0, 0, -6, 0}, {ENG_FM, 0.25f, 0, 0, -8, 0},
    {ENG_MODAL, 0.6f, 0, 0, -10, 0.2f}, {ENG_COWBELL, 0.2f, 0, 0, -12, -0.2f},
};

void dm_lane_clear(dm_vkit_t *v, uint32_t l)
{
    uint32_t i;
    for (i = 0; i < DM_MAXSTEPS; i++)
        v->lane[l][i] = DM_LANE[l].def;
}

void dm_voice_reset_sound(dm_vkit_t *v, uint32_t index)
{
    uint32_t t;
    index %= DM_NV;
    for (t = 0; t < DT_N; t++)
        v->base[t] = dm_norm(t, DM_MAP[t].def);
    v->base[T_E1D] = dm_norm(T_E1D, VDEF[index].decay);
    v->base[T_GAIN] = dm_norm(T_GAIN, VDEF[index].gain);
    v->base[T_PAN] = dm_norm(T_PAN, VDEF[index].pan);
    v->base[T_CUTOFF] = 1.0f;
    zero(v->depth, sizeof v->depth);
    v->locks = 0;
    v->engine = VDEF[index].eng;
    v->env2 = v->stereo = v->post = 0;
    v->fmode = FM_SCREAM_LP;
    v->dmode = DM_TAPE;
    v->e1curve = v->e2curve = 1.0f;
    v->e1vel = v->e2vel = 0.5f;
    v->l1sync = v->l2sync = 1;
    v->l1bip = v->l2bip = 1;
    v->l1wave = v->l2wave = LW_WAVETABLE;
    v->l1reset = v->l2reset = LR_FREE;
    v->l1shape = v->l2shape = 0.5f;
    v->macro1 = v->macro2 = v->variation = 0.0f;
    v->choke = v->link = 0;
}

void dm_voice_defaults(dm_vkit_t *v, uint32_t index)
{
    uint32_t l;
    zero(v, sizeof *v);
    dm_voice_reset_sound(v, index);
    v->pf_enable = 1;
    v->steps = 16;
    v->fill = VDEF[index % DM_NV].fill;
    v->pattern = 1;
    v->rotate = VDEF[index % DM_NV].rotate;
    v->speed = 4;
    v->mult = 3;
    v->length = 16;
    v->snap = 1;
    v->reset_len = 64;
    for (l = 0; l < DM_NLANE; l++) {
        dm_lane_clear(v, l);
        v->lane_len[l] = 16;
        v->lane_sync[l] = LS_SYNC;
    }
}

void dm_kit_defaults(dm_kit_t *k)
{
    uint32_t v;
    zero(k, sizeof *k);
    for (v = 0; v < DM_NV; v++)
        dm_voice_defaults(&k->v[v], v);
    k->g.d_sync = 1;
    k->g.d_div = 6;
    k->g.d_stereo = 1;
    k->g.d_time_ms = 250.0f;
    k->g.d_fb = 0.31f;
    k->g.d_srate = 1.0f;
    k->g.r_div = 8;
    k->g.r_pre_ms = 1.0f;
    k->g.r_decay = 4.0f;
    k->g.r_srate = 1.0f;
    k->g.m_clip = 1;
    k->g.bpm = 120.0f;
    k->g.rnd_depth = 0.5f;
    k->g.pf_depth = 1.0f;
    k->g.pf_enabled = 1;
    k->g.pf_rnd = 0.1f;
    k->name[0] = 'I';
    k->name[1] = 'N';
    k->name[2] = 'I';
    k->name[3] = 'T';
}

uint32_t dm_seq_length(int steps, int length, int snap)
{
    int s = steps < 1 ? 1 : steps, m;
    if (!snap)
        return (uint32_t)dm_clampi(length, 1, DM_MAXSTEPS);
    m = (length + s - 1) / s;
    if (m < 1)
        m = 1;
    return (uint32_t)dm_clampi(m * s, 1, DM_MAXSTEPS);
}

/* a voice's sound from src into dst (voice presets, PASTE): the sequencer, the sequence, the mix and the routing stay;
 * a locked control keeps its value (Drummatic: a lock holds through a preset change). Field by field, never a
 * passing copy of the whole voice: the audio reads dst at any moment */
void dm_voice_copy_sound(dm_vkit_t *dst, const dm_vkit_t *s)
{
    uint32_t i, t;
    for (t = 0; t < DT_N; t++)
        if (!((dst->locks >> t) & 1u) && t != T_GAIN && t != T_PAN && t != T_DSEND && t != T_RSEND)
            dst->base[t] = s->base[t];
    for (i = 0; i < DS_N; i++)
        for (t = 0; t < DT_N; t++)
            dst->depth[i][t] = s->depth[i][t];
    dst->locks |= s->locks;
    dst->e1curve = s->e1curve; dst->e1vel = s->e1vel; dst->e2curve = s->e2curve; dst->e2vel = s->e2vel;
    dst->l1shape = s->l1shape; dst->l2shape = s->l2shape;
    dst->macro1 = s->macro1; dst->macro2 = s->macro2; dst->variation = s->variation;
    dst->engine = s->engine; dst->env2 = s->env2; dst->stereo = s->stereo; dst->post = s->post;
    dst->fmode = s->fmode; dst->dmode = s->dmode;
    dst->l1sync = s->l1sync; dst->l1bip = s->l1bip; dst->l1wave = s->l1wave; dst->l1reset = s->l1reset;
    dst->l2sync = s->l2sync; dst->l2bip = s->l2bip; dst->l2wave = s->l2wave; dst->l2reset = s->l2reset;
}

/* ---------------------------------------------------------------- DADO --- */
/* PluginProcessor::randomizeVoice: every control of the voice but the mix, the sequencer, the routing and the
 * Performance switches moves by up to +-DEPTH of its range (a choice or a switch: a new one in DEPTH of the cases);
 * the locked targets stay; the cutoff stays above 45 %, the decays between 20 and 75 %, the attacks under 35 % */
static float rnd_float(float cur, float depth, dm_rng_t *r) { return dm_clamp01(cur + dm_rng_bi(r) * depth); }
static uint8_t rnd_choice(uint8_t cur, uint32_t n, float depth, dm_rng_t *r)
{
    if (dm_rng_uni(r) < depth)
        return (uint8_t)dm_clampi((int)dm_roundf(dm_rng_uni(r) * (float)(n - 1u)), 0, (int)n - 1);
    return cur;
}
void dm_randomize_voice(dm_kit_t *k, uint32_t vi, dm_rng_t *r)
{
    dm_vkit_t *v = &k->v[vi % DM_NV];
    float d = k->g.rnd_depth;
    uint32_t t;
    for (t = 0; t < DT_N; t++) {
        float nv;
        if (t == T_GAIN || t == T_PAN || t == T_DSEND || t == T_RSEND || ((v->locks >> t) & 1u))
            continue;
        nv = rnd_float(v->base[t], d, r);
        if (t == T_CUTOFF)
            nv = dm_maxf(nv, 0.45f);
        if (t == T_E1D || t == T_E2D)
            nv = dm_clampf(nv, 0.2f, 0.75f);
        if (t == T_E1A || t == T_E2A)
            nv = dm_minf(nv, 0.35f);
        if (t == T_PITCH)
            nv = dm_norm(T_PITCH, dm_roundf(dm_denorm(T_PITCH, nv)));
        v->base[t] = nv;
    }
    v->engine = rnd_choice(v->engine, ENG_COUNT, d, r);
    v->env2 = rnd_choice(v->env2, 2, d, r);
    v->stereo = rnd_choice(v->stereo, 2, d, r);
    v->fmode = rnd_choice(v->fmode, FM_COUNT, d, r);
    v->post = rnd_choice(v->post, 2, d, r);
    v->dmode = rnd_choice(v->dmode, DM_COUNT, d, r);
    v->e1curve = rnd_float(v->e1curve, d, r);
    v->e1vel = rnd_float(v->e1vel, d, r);
    v->e2curve = rnd_float(v->e2curve, d, r);
    v->e2vel = rnd_float(v->e2vel, d, r);
    v->l1sync = rnd_choice(v->l1sync, 2, d, r);
    v->l1bip = rnd_choice(v->l1bip, 2, d, r);
    v->l1wave = rnd_choice(v->l1wave, LW_COUNT, d, r);
    v->l1reset = rnd_choice(v->l1reset, LR_COUNT, d, r);
    v->l1shape = rnd_float(v->l1shape, d, r);
    v->l2sync = rnd_choice(v->l2sync, 2, d, r);
    v->l2bip = rnd_choice(v->l2bip, 2, d, r);
    v->l2wave = rnd_choice(v->l2wave, LW_COUNT, d, r);
    v->l2reset = rnd_choice(v->l2reset, LR_COUNT, d, r);
    v->l2shape = rnd_float(v->l2shape, d, r);
    v->macro1 = rnd_float(v->macro1, d, r);
    v->macro2 = rnd_float(v->macro2, d, r);
    v->variation = rnd_float(v->variation, d, r);
}
void dm_randomize_all(dm_kit_t *k, dm_rng_t *r)
{
    uint32_t v;
    for (v = 0; v < DM_NV; v++)
        dm_randomize_voice(k, v, r);
}
void dm_lane_randomize(dm_vkit_t *v, uint32_t l, dm_rng_t *r)    /* SeqPage::randomLane over the steps shown */
{
    uint32_t i, n = dm_seq_length(v->steps, v->length, v->snap);
    float depth = dm_kit.g.rnd_depth, span = (float)(DM_LANE[l].hi - DM_LANE[l].lo);
    if (n < 16u)
        n = 16u;
    for (i = 0; i < n && i < DM_MAXSTEPS; i++) {
        float x = (float)v->lane[l][i] + dm_rng_bi(r) * depth * span;
        v->lane[l][i] = (int8_t)dm_clampi((int)dm_roundf(x), DM_LANE[l].lo, DM_LANE[l].hi);
    }
}

/* --------------------------------------------------------- Performance --- */
static int perf_target(uint32_t i)
{
    switch (i) {
    case PF_SYNPITCH: return T_PITCH;
    case PF_SYN1: return T_P1;
    case PF_SYN2: return T_P2;
    case PF_SYN3: return T_P3;
    case PF_SYN4: return T_P4;
    case PF_DISTORT: return T_DIST;
    case PF_CUTOFF: return T_CUTOFF;
    case PF_RES: return T_RES;
    default: return -1;
    }
}
static float perf_scale(uint32_t i) { return i == PF_SYNPITCH ? 0.25f : 1.0f; }

void dm_perf_randomize(dm_kit_t *k, dm_rng_t *r)
{
    uint32_t i;
    for (i = 0; i < PF_N; i++)
        if (!((k->g.perf_lock >> i) & 1u))
            k->g.perf[i] = dm_clampf(2.0f * dm_rng_bi(r) * k->g.pf_rnd, -1.0f, 1.0f);
}
void dm_perf_reset(dm_kit_t *k)
{
    uint32_t i;
    for (i = 0; i < PF_N; i++)
        k->g.perf[i] = 0.0f;
}
/* COMMIT: the sound playing written into the voices, the Performance page back to 0 */
void dm_perf_commit(dm_kit_t *k)
{
    uint32_t v, i, st;
    if (!k->g.pf_enabled) {
        dm_perf_reset(k);
        return;
    }
    for (v = 0; v < DM_NV; v++) {
        dm_vkit_t *x = &k->v[v];
        float kk = k->g.pf_depth * (x->pf_flip ? -1.0f : 1.0f), po;
        int ro;
#define PV(n) (k->g.perf[n] * kk)
        if (!x->pf_enable)
            continue;
        for (i = 0; i < PF_N; i++) {
            int t = perf_target(i);
            float d = PV(i) * perf_scale(i);
            if (t >= 0 && d != 0.0f && !((x->locks >> t) & 1u))
                x->base[t] = dm_clamp01(x->base[t] + d);
        }
#define ADDT(t, d) do { if ((d) != 0.0f && !((x->locks >> (t)) & 1u)) x->base[t] = dm_clamp01(x->base[t] + (d)); } while (0)
        ADDT(T_E1A, PV(PF_ENVA));
        ADDT(T_E2A, PV(PF_ENVA));
        ADDT(T_E1H, PV(PF_ENVH));
        ADDT(T_E2H, PV(PF_ENVH));
        ADDT(T_E1D, PV(PF_ENVD));
        ADDT(T_E2D, PV(PF_ENVD));
        x->e1curve = dm_clamp01(x->e1curve + PV(PF_ENVC));
        x->e2curve = dm_clamp01(x->e2curve + PV(PF_ENVC));
        x->macro1 = dm_clamp01(x->macro1 + PV(PF_MACRO1));
        x->macro2 = dm_clamp01(x->macro2 + PV(PF_MACRO2));
        x->variation = dm_clamp01(x->variation + PV(PF_VAR));
        x->density = dm_clampf(x->density + PV(PF_DENSITY) * 0.5f, -1.0f, 1.0f);
        x->steps = (int8_t)dm_clampi(x->steps + (int)dm_roundf(PV(PF_STEPS) * 32.0f), 1, 64);
        x->fill = (int8_t)dm_clampi(x->fill + (int)dm_roundf(PV(PF_FILL) * 32.0f), 0, 64);
        x->pattern = (int8_t)dm_clampi(x->pattern + (int)dm_roundf(PV(PF_PATTERN) * 32.0f), 1, 64);
        {   /* MOD AMP scales the modulation depths */
            float amp = dm_clampf(1.0f + PV(PF_MODAMP), 0.0f, 2.0f);
            uint32_t s, t;
            if (amp != 1.0f)
                for (s = 0; s < DS_N; s++)
                    for (t = 0; t < DT_N; t++)
                        x->depth[s][t] = dm_clampf(x->depth[s][t] * amp, -1.0f, 1.0f);
        }
        po = PV(PF_PROB) * 100.0f;
        ro = (int)dm_roundf(PV(PF_RATCHET) * 4.0f);
        for (st = 0; st < DM_MAXSTEPS; st++) {
            if (po != 0.0f)
                x->lane[L_PROB][st] = (int8_t)dm_clampi((int)dm_roundf((float)x->lane[L_PROB][st] + po), -100, 100);
            if (ro != 0)
                x->lane[L_RATCH][st] = (int8_t)dm_clampi(x->lane[L_RATCH][st] + ro, 1, 8);
        }
#undef ADDT
#undef PV
    }
    dm_perf_reset(k);
}

/* the audio's view of voice vi: the kit plus the Performance page (PluginProcessor::pullSettings + applyPerformance) */
void dm_effective(const dm_kit_t *k, uint32_t vi, dm_vset_t *s, dm_seqset_t *q)
{
    const dm_vkit_t *v = &k->v[vi % DM_NV];
    uint32_t t, i;
    for (t = 0; t < DT_N; t++)
        s->base[t] = v->base[t];
    s->depth = (const float (*)[DT_N])v->depth;
    s->e1curve = v->e1curve;
    s->e1vel = v->e1vel;
    s->e2curve = v->e2curve;
    s->e2vel = v->e2vel;
    s->l1shape = v->l1shape;
    s->l2shape = v->l2shape;
    s->macro1 = v->macro1;
    s->macro2 = v->macro2;
    s->variation = v->variation;
    s->modamp = 1.0f;
    s->locks = v->locks;
    s->engine = v->engine < ENG_COUNT ? v->engine : 0;
    s->env2 = v->env2;
    s->stereo = v->stereo;
    s->post = v->post;
    s->fmode = v->fmode % FM_COUNT;
    s->dmode = v->dmode % DM_COUNT;
    s->l1sync = v->l1sync;
    s->l1bip = v->l1bip;
    s->l1wave = v->l1wave % LW_COUNT;
    s->l1reset = v->l1reset % LR_COUNT;
    s->l2sync = v->l2sync;
    s->l2bip = v->l2bip;
    s->l2wave = v->l2wave % LW_COUNT;
    s->l2reset = v->l2reset % LR_COUNT;
    q->steps = dm_clampi(v->steps, 1, 64);
    q->fill = dm_clampi(v->fill, 0, 64);
    q->pattern = dm_clampi(v->pattern, 1, 64);
    q->rotate = v->rotate;
    q->invert = v->invert;
    q->speed = v->speed % DM_NSPEED;
    q->mult = v->mult % DM_NMULT;
    q->length = dm_clampi(v->length, 1, DM_MAXSTEPS);
    q->snap = v->snap;
    q->reset_on = v->reset_on;
    q->reset_len = dm_clampi(v->reset_len, 1, DM_MAXSTEPS);
    q->shift = v->shift;
    q->swing = v->swing;
    q->shape = v->shape;
    q->density = v->density;
    q->prob_off = 0.0f;
    q->ratch_off = 0;
    if (k->g.pf_enabled && v->pf_enable) {
        float kk = k->g.pf_depth * (v->pf_flip ? -1.0f : 1.0f);
#define PV(n) (k->g.perf[n] * kk)
#define ADDT(t, d) do { float dd_ = (d); if (dd_ != 0.0f && !((v->locks >> (t)) & 1u)) s->base[t] = dm_clamp01(s->base[t] + dd_); } while (0)
        for (i = 0; i < PF_N; i++) {
            int t2 = perf_target(i);
            if (t2 >= 0)
                ADDT((uint32_t)t2, PV(i) * perf_scale(i));
        }
        ADDT(T_E1A, PV(PF_ENVA));
        ADDT(T_E2A, PV(PF_ENVA));
        ADDT(T_E1H, PV(PF_ENVH));
        ADDT(T_E2H, PV(PF_ENVH));
        ADDT(T_E1D, PV(PF_ENVD));
        ADDT(T_E2D, PV(PF_ENVD));
        s->e1curve = dm_clamp01(s->e1curve + PV(PF_ENVC));
        s->e2curve = dm_clamp01(s->e2curve + PV(PF_ENVC));
        s->macro1 = dm_clamp01(s->macro1 + PV(PF_MACRO1));
        s->macro2 = dm_clamp01(s->macro2 + PV(PF_MACRO2));
        s->variation = dm_clamp01(s->variation + PV(PF_VAR));
        s->modamp = dm_clampf(1.0f + PV(PF_MODAMP), 0.0f, 2.0f);
        q->steps = dm_clampi(q->steps + (int)dm_roundf(PV(PF_STEPS) * 32.0f), 1, 64);
        q->fill = dm_clampi(q->fill + (int)dm_roundf(PV(PF_FILL) * 32.0f), 0, 64);
        q->pattern = dm_clampi(q->pattern + (int)dm_roundf(PV(PF_PATTERN) * 32.0f), 1, 64);
        q->density = dm_clampf(q->density + PV(PF_DENSITY), -1.0f, 1.0f);
        q->prob_off = PV(PF_PROB);
        q->ratch_off = (int)dm_roundf(PV(PF_RATCHET) * 4.0f);
#undef ADDT
#undef PV
    }
}

/* ------------------------------------------------------- flash format --- */
/* "DMK1": the name; per voice the sound (floats as 16-bit fractions; the modulation as (source, target, depth)
 * triples, only the non-zero ones), the sequencer settings, the manual trigs, the lanes (each as its length of values
 * that differ from the default, run-length coded); then the globals. Little endian. */
#define KMAGIC 0x314B4D44u
typedef struct { uint8_t *p; uint32_t n, max; int bad; } wr_t;
static void w8(wr_t *w, uint32_t v)
{
    if (w->n < w->max)
        w->p[w->n] = (uint8_t)v;
    else
        w->bad = 1;
    w->n++;
}
static void w16(wr_t *w, uint32_t v) { w8(w, v); w8(w, v >> 8); }
static void w32(wr_t *w, uint32_t v) { w16(w, v); w16(w, v >> 16); }
static void wf(wr_t *w, float x)          /* float as its bits (the globals: any range) */
{
    union { float f; uint32_t u; } u;
    u.f = x;
    w32(w, u.u);
}
static void wq(wr_t *w, float x, float lo, float hi) { w16(w, (uint32_t)dm_roundf(dm_clamp01((x - lo) / (hi - lo)) * 65535.0f)); }
typedef struct { const uint8_t *p; uint32_t n, i; int bad; } rd_t;
static uint32_t r8(rd_t *r)
{
    if (r->i < r->n)
        return r->p[r->i++];
    r->bad = 1;
    return 0;
}
static uint32_t r16(rd_t *r) { uint32_t a = r8(r); return a | r8(r) << 8; }
static uint32_t r32(rd_t *r) { uint32_t a = r16(r); return a | r16(r) << 16; }
static float rf(rd_t *r)
{
    union { float f; uint32_t u; } u;
    u.u = r32(r);
    return dm_finite(u.f) ? u.f : 0.0f;
}
static float rq(rd_t *r, float lo, float hi) { return lo + (hi - lo) * (float)r16(r) * (1.0f / 65535.0f); }

static void pack_voice(wr_t *w, const dm_vkit_t *x)
{
    uint32_t s, t, i, l, nmod = 0;
    for (t = 0; t < DT_N; t++)
        wq(w, x->base[t], 0.0f, 1.0f);
    for (s = 0; s < DS_N; s++)
        for (t = 0; t < DT_N; t++)
            nmod += x->depth[s][t] != 0.0f;
    w8(w, nmod);
    for (s = 0; s < DS_N; s++)
        for (t = 0; t < DT_N; t++)
            if (x->depth[s][t] != 0.0f) {
                w8(w, s);
                w8(w, t);
                wq(w, x->depth[s][t], -1.0f, 1.0f);
            }
    w32(w, x->locks);
    wq(w, x->e1curve, 0, 1);
    wq(w, x->e1vel, 0, 1);
    wq(w, x->e2curve, 0, 1);
    wq(w, x->e2vel, 0, 1);
    wq(w, x->l1shape, 0, 1);
    wq(w, x->l2shape, 0, 1);
    wq(w, x->macro1, 0, 1);
    wq(w, x->macro2, 0, 1);
    wq(w, x->variation, 0, 1);
    w8(w, x->engine); w8(w, x->env2); w8(w, x->stereo); w8(w, x->post); w8(w, x->fmode); w8(w, x->dmode);
    w8(w, x->l1sync); w8(w, x->l1bip); w8(w, x->l1wave); w8(w, x->l1reset);
    w8(w, x->l2sync); w8(w, x->l2bip); w8(w, x->l2wave); w8(w, x->l2reset);
    w8(w, x->choke); w8(w, x->link); w8(w, x->pf_enable); w8(w, x->pf_flip); w8(w, x->mute); w8(w, x->solo);
    w8(w, (uint8_t)x->steps); w8(w, (uint8_t)x->fill); w8(w, (uint8_t)x->pattern); w8(w, (uint8_t)x->rotate);
    w8(w, x->invert); w8(w, x->speed); w8(w, x->mult); w8(w, x->length); w8(w, x->snap);
    w8(w, x->reset_on); w8(w, x->reset_len);
    wq(w, x->shift, -1, 1);
    wq(w, x->swing, -1, 1);
    wq(w, x->shape, -1, 1);
    wq(w, x->density, -1, 1);
    for (i = 0; i < DM_MAXSTEPS / 32u; i++)
        w32(w, x->manual[i]);
    for (l = 0; l < DM_NLANE; l++) {               /* the values up to the last one off the default, raw */
        uint32_t last = 0;
        w8(w, x->lane_len[l]);
        w8(w, x->lane_sync[l]);
        for (i = 0; i < DM_MAXSTEPS; i++)
            if (x->lane[l][i] != DM_LANE[l].def)
                last = i + 1u;
        w8(w, last);
        for (i = 0; i < last; i++)
            w8(w, (uint8_t)x->lane[l][i]);
    }
}

static void unpack_voice(rd_t *r, dm_vkit_t *x)
{
    uint32_t i, t, l, nm;
    for (t = 0; t < DT_N; t++)
        x->base[t] = rq(r, 0, 1);
    for (i = 0; i < DS_N; i++)
        for (t = 0; t < DT_N; t++)
            x->depth[i][t] = 0.0f;
    nm = r8(r);
    for (i = 0; i < nm; i++) {
        uint32_t s = r8(r), tt = r8(r);
        float d = rq(r, -1, 1);
        if (s < DS_N && tt < DT_N)
            x->depth[s][tt] = dm_absf(d) < 1.0f / 65535.0f ? 0.0f : d;
    }
    x->locks = r32(r);
    x->e1curve = rq(r, 0, 1);
    x->e1vel = rq(r, 0, 1);
    x->e2curve = rq(r, 0, 1);
    x->e2vel = rq(r, 0, 1);
    x->l1shape = rq(r, 0, 1);
    x->l2shape = rq(r, 0, 1);
    x->macro1 = rq(r, 0, 1);
    x->macro2 = rq(r, 0, 1);
    x->variation = rq(r, 0, 1);
    x->engine = (uint8_t)(r8(r) % ENG_COUNT); x->env2 = (uint8_t)r8(r); x->stereo = (uint8_t)r8(r);
    x->post = (uint8_t)r8(r); x->fmode = (uint8_t)(r8(r) % FM_COUNT); x->dmode = (uint8_t)(r8(r) % DM_COUNT);
    x->l1sync = (uint8_t)r8(r); x->l1bip = (uint8_t)r8(r); x->l1wave = (uint8_t)(r8(r) % LW_COUNT);
    x->l1reset = (uint8_t)(r8(r) % LR_COUNT);
    x->l2sync = (uint8_t)r8(r); x->l2bip = (uint8_t)r8(r); x->l2wave = (uint8_t)(r8(r) % LW_COUNT);
    x->l2reset = (uint8_t)(r8(r) % LR_COUNT);
    x->choke = (uint8_t)(r8(r) % 9u); x->link = (uint8_t)(r8(r) % 9u); x->pf_enable = (uint8_t)r8(r);
    x->pf_flip = (uint8_t)r8(r); x->mute = (uint8_t)r8(r); x->solo = (uint8_t)r8(r);
    x->steps = (int8_t)dm_clampi((int8_t)r8(r), 1, 64); x->fill = (int8_t)dm_clampi((int8_t)r8(r), 0, 64);
    x->pattern = (int8_t)dm_clampi((int8_t)r8(r), 1, 64); x->rotate = (int8_t)dm_clampi((int8_t)r8(r), -32, 32);
    x->invert = (uint8_t)r8(r); x->speed = (uint8_t)(r8(r) % DM_NSPEED); x->mult = (uint8_t)(r8(r) % DM_NMULT);
    x->length = (uint8_t)dm_clampi((int)r8(r), 1, DM_MAXSTEPS); x->snap = (uint8_t)r8(r);
    x->reset_on = (uint8_t)r8(r); x->reset_len = (uint8_t)dm_clampi((int)r8(r), 1, DM_MAXSTEPS);
    x->shift = rq(r, -1, 1);
    x->swing = rq(r, -1, 1);
    x->shape = rq(r, -1, 1);
    x->density = rq(r, -1, 1);
    for (i = 0; i < DM_MAXSTEPS / 32u; i++)
        x->manual[i] = r32(r);
    for (l = 0; l < DM_NLANE; l++) {
        uint32_t last;
        x->lane_len[l] = (uint8_t)dm_clampi((int)r8(r), 1, DM_MAXSTEPS);
        x->lane_sync[l] = (uint8_t)(r8(r) % LS_COUNT);
        last = r8(r);
        dm_lane_clear(x, l);
        for (i = 0; i < last; i++) {
            int8_t val = (int8_t)r8(r);
            if (i < DM_MAXSTEPS)
                x->lane[l][i] = (int8_t)dm_clampi(val, DM_LANE[l].lo, DM_LANE[l].hi);
        }
    }
}

static void pack_globals(wr_t *w, const dm_gkit_t *g)
{
    uint32_t i;
    w8(w, g->d_sync); w8(w, g->d_div); w8(w, g->d_stereo); w8(w, g->d_pong);
    wf(w, g->d_time_ms); wf(w, g->d_fb); wf(w, g->d_glitch); wf(w, g->d_fbtilt); wf(w, g->d_duck);
    wf(w, g->d_srate); wf(w, g->d_gain_db); wf(w, g->d_torev);
    w8(w, g->r_sync); w8(w, g->r_div); w8(w, g->r_mode); w8(w, g->m_clip);
    wf(w, g->r_pre_ms); wf(w, g->r_decay); wf(w, g->r_srate); wf(w, g->r_intilt); wf(w, g->r_outtilt);
    wf(w, g->r_gain_db);
    wf(w, g->m_max); wf(w, g->m_low); wf(w, g->m_mid); wf(w, g->m_high); wf(w, g->m_gain);
    wf(w, g->humanize); wf(w, g->bpm); wf(w, g->rnd_depth);
    w8(w, PF_N);
    for (i = 0; i < PF_N; i++)
        wf(w, g->perf[i]);
    wf(w, g->pf_depth); wf(w, g->pf_rnd); w8(w, g->pf_enabled); w32(w, g->perf_lock);
}
static void unpack_globals(rd_t *r, dm_gkit_t *g)
{
    uint32_t i, np;
    g->d_sync = (uint8_t)r8(r); g->d_div = (uint8_t)(r8(r) % DM_NLFODIV); g->d_stereo = (uint8_t)r8(r);
    g->d_pong = (uint8_t)r8(r);
    g->d_time_ms = dm_clampf(rf(r), 1.0f, 2000.0f); g->d_fb = rf(r); g->d_glitch = rf(r); g->d_fbtilt = rf(r);
    g->d_duck = rf(r); g->d_srate = rf(r); g->d_gain_db = rf(r); g->d_torev = rf(r);
    g->r_sync = (uint8_t)r8(r); g->r_div = (uint8_t)(r8(r) % DM_NLFODIV); g->r_mode = (uint8_t)(r8(r) & 3u);
    g->m_clip = (uint8_t)(r8(r) & 3u);
    g->r_pre_ms = rf(r); g->r_decay = rf(r); g->r_srate = rf(r); g->r_intilt = rf(r); g->r_outtilt = rf(r);
    g->r_gain_db = rf(r);
    g->m_max = rf(r); g->m_low = rf(r); g->m_mid = rf(r); g->m_high = rf(r); g->m_gain = rf(r);
    g->humanize = rf(r); g->bpm = dm_clampf(rf(r), 40.0f, 300.0f); g->rnd_depth = rf(r);
    /* (the UI's ranges, dm_params.c: a blob from elsewhere cannot push the audio outside them) */
    g->d_fb = dm_clamp01(g->d_fb); g->d_glitch = dm_clamp01(g->d_glitch); g->d_fbtilt = dm_clampf(g->d_fbtilt, -1, 1);
    g->d_duck = dm_clamp01(g->d_duck); g->d_srate = dm_clamp01(g->d_srate); g->d_gain_db = dm_clampf(g->d_gain_db, -24, 12);
    g->d_torev = dm_clamp01(g->d_torev);
    g->r_pre_ms = dm_clampf(g->r_pre_ms, 0.1f, 499.0f); g->r_decay = dm_clampf(g->r_decay, 0.1f, 20.0f);
    g->r_srate = dm_clamp01(g->r_srate); g->r_intilt = dm_clampf(g->r_intilt, -1, 1); g->r_outtilt = dm_clampf(g->r_outtilt, -1, 1);
    g->r_gain_db = dm_clampf(g->r_gain_db, -24, 12);
    g->m_max = dm_clamp01(g->m_max); g->m_low = dm_clampf(g->m_low, -18, 18); g->m_mid = dm_clampf(g->m_mid, -18, 18);
    g->m_high = dm_clampf(g->m_high, -18, 18); g->m_gain = dm_clampf(g->m_gain, -36, 12);
    g->humanize = dm_clamp01(g->humanize); g->rnd_depth = dm_clamp01(g->rnd_depth);
    np = r8(r);
    for (i = 0; i < np; i++) {
        float x = rf(r);
        if (i < PF_N)
            g->perf[i] = dm_clampf(x, -1.0f, 1.0f);
    }
    g->pf_depth = dm_clampf(rf(r), -1, 1); g->pf_rnd = dm_clamp01(rf(r)); g->pf_enabled = (uint8_t)r8(r);
    g->perf_lock = r32(r);
}

uint32_t dm_kit_pack(const dm_kit_t *k, uint8_t *out, uint32_t max)
{
    wr_t w = {out, 0, max, 0};
    uint32_t v, i;
    w32(&w, KMAGIC);
    for (i = 0; i < 12u; i++)
        w8(&w, (uint8_t)k->name[i]);
    for (v = 0; v < DM_NV; v++)
        pack_voice(&w, &k->v[v]);
    pack_globals(&w, &k->g);
    return w.bad ? 0u : w.n;
}

/* checked first into scratch (a voice at a time), then for real: a bad blob leaves k as it was */
dm_vkit_t dm_scratch_voice;              /* one voice of scratch: the checks of the unpackers, the voice presets */
int dm_kit_unpack(dm_kit_t *k, const uint8_t *in, uint32_t n)
{
    static dm_gkit_t sg;
    dm_vkit_t *const sv = &dm_scratch_voice;
    uint32_t pass, v, i;
    for (pass = 0; pass < 2u; pass++) {
        rd_t r = {in, n, 0, 0};
        if (r32(&r) != KMAGIC)
            return -1;
        for (i = 0; i < 12u; i++) {
            char c = (char)r8(&r);
            if (pass)
                k->name[i] = c;
        }
        if (pass)
            k->name[11] = 0;
        for (v = 0; v < DM_NV; v++)
            unpack_voice(&r, pass ? &k->v[v] : sv);
        unpack_globals(&r, pass ? &k->g : &sg);
        if (r.bad)
            return -1;
    }
    return 0;
}

/* one voice's whole data (voice presets, the voice clipboard) */
uint32_t dm_voice_pack(const dm_vkit_t *v, uint8_t *out, uint32_t max)
{
    wr_t w = {out, 0, max, 0};
    w32(&w, KMAGIC ^ 0x56u);
    pack_voice(&w, v);
    return w.bad ? 0u : w.n;
}
int dm_voice_unpack(dm_vkit_t *v, const uint8_t *in, uint32_t n)   /* (v may be dm_scratch_voice) */
{
    rd_t r = {in, n, 0, 0};
    if (r32(&r) != (KMAGIC ^ 0x56u))
        return -1;
    unpack_voice(&r, &dm_scratch_voice);
    if (r.bad)
        return -1;
    if (v != &dm_scratch_voice)
        *v = dm_scratch_voice;
    return 0;
}

/* --------------------------------------------------- the reference kit --- */
/* "DAEL REF": the intro of Autechre's "Dael" as the owner rebuilt it on the FM-1 (his reference for every project:
 * 90 BPM, -20 cents, one bar of 16ths): step 1 a short FM hit and a falling buzz, 4 a buzz swelling for two seconds
 * into the next bar, 5 a noise hit, 6 a zap, 8 a metallic stack held for half a bar, 9 and 13 closed hats; dark (most
 * of the energy under 150 Hz), the percussion very short, little reverb. The levels are set so that each voice alone
 * sits where it sits in the reference (RMS over the bar: buzz swell -23, buzz fall -26, hit + hats -32, noise -33,
 * stack -37, zap -39 dBFS: tests/dm_ref.c measures them). Voice 8: the kick of the owner's kick / bass reference
 * (pitch ~110 Hz at the attack, -11 dB at 20 ms), silent in the pattern. */
static void ref_trig(dm_vkit_t *v, uint32_t step) { dm_manual_set(v, step, 1); }
static void ref_set(dm_vkit_t *v, uint32_t t, float val) { v->base[t] = dm_norm(t, val); }
void dm_kit_reference(dm_kit_t *k)
{
    uint32_t i;
    dm_vkit_t *v;
    dm_kit_defaults(k);
    for (i = 0; i < DM_NV; i++) {
        v = &k->v[i];
        v->fill = 0;
        v->rotate = 0;
        ref_set(v, T_PITCH, -0.2f);
    }
    /* 1 HIT: a short two-operator FM hit near 190 Hz, a noisy edge */
    v = &k->v[0];
    v->engine = ENG_FM;
    ref_set(v, T_PITCH, 6.6f);                     /* 130.8 Hz -> ~190 Hz */
    v->base[T_P1] = 0.52f; v->base[T_P2] = 0.85f; v->base[T_P3] = 0.35f; v->base[T_P4] = 0.9f;
    ref_set(v, T_E1A, 0.0005f); ref_set(v, T_E1H, 0.0005f); ref_set(v, T_E1D, 0.07f);
    v->e1curve = 1.0f;
    ref_set(v, T_GAIN, 0.5f);
    ref_trig(v, 0);
    /* 2 NOISE: the clap, a single burst, short */
    v = &k->v[1];
    v->engine = ENG_CLAP;
    v->base[T_P1] = 0.15f; v->base[T_P2] = 0.2f; v->base[T_P3] = 0.45f; v->base[T_P4] = 0.25f;
    ref_set(v, T_E1D, 0.12f);
    ref_set(v, T_GAIN, 0.9f);
    ref_set(v, T_RSEND, 0.35f);
    ref_trig(v, 4);
    /* 3 ZAP: from ~2 kHz down to ~100 Hz in ~40 ms, noisy */
    v = &k->v[2];
    v->engine = ENG_ZAP;
    ref_set(v, T_PITCH, 4.8f);
    v->base[T_P1] = 0.15f; v->base[T_P2] = 0.9f; v->base[T_P3] = 0.15f; v->base[T_P4] = 0.0f;
    ref_set(v, T_E1D, 0.09f);
    ref_set(v, T_GAIN, -8.2f);
    ref_set(v, T_RSEND, 0.45f);
    ref_trig(v, 5);
    /* 4 METAL STACK: inharmonic plate partials (436..2105 Hz), held half a bar */
    v = &k->v[3];
    v->engine = ENG_SCRAPYARD;
    ref_set(v, T_PITCH, 5.0f);
    v->base[T_P1] = 0.3f; v->base[T_P2] = 0.15f; v->base[T_P3] = 0.55f; v->base[T_P4] = 0.1f;
    ref_set(v, T_E1A, 0.002f); ref_set(v, T_E1D, 1.2f);
    v->e1curve = 0.7f;
    ref_set(v, T_GAIN, -15.3f);
    ref_set(v, T_RSEND, 0.4f);
    ref_trig(v, 7);
    /* 5 BUZZ SWELL: a ~27 Hz saw, dark (1 kHz), swelling over two seconds into the next bar */
    v = &k->v[4];
    v->engine = ENG_BUILDER;
    ref_set(v, T_PITCH, -27.4f);                   /* 130.8 Hz -> 27.2 Hz */
    v->base[T_P1] = 0.667f; v->base[T_P2] = 1.0f; v->base[T_P3] = 0.0f; v->base[T_P4] = 0.0f;
    v->fmode = FM_DARK_LP;
    ref_set(v, T_CUTOFF, 1000.0f);
    ref_set(v, T_E1A, 2.0f); ref_set(v, T_E1H, 0.17f); ref_set(v, T_E1D, 0.15f);
    v->e1curve = 0.0f;
    v->e1vel = 0.0f;
    ref_set(v, T_GAIN, -11.7f);
    ref_trig(v, 3);
    /* 6 BUZZ FALL: a saw pulse train falling from ~28 to ~16 Hz over four steps, brighter (2.8 kHz) */
    v = &k->v[5];
    v->engine = ENG_BUILDER;
    ref_set(v, T_PITCH, -36.4f);                   /* 16 Hz at rest */
    v->base[T_P1] = 0.667f; v->base[T_P2] = 1.0f; v->base[T_P3] = 0.0f; v->base[T_P4] = 0.0f;
    v->fmode = FM_DARK_LP;
    ref_set(v, T_CUTOFF, 2800.0f);
    ref_set(v, T_E1A, 0.0005f); ref_set(v, T_E1H, 0.25f); ref_set(v, T_E1D, 0.55f);
    v->e1curve = 0.4f;
    v->e1vel = 0.0f;
    ref_set(v, T_E2A, 0.0005f); ref_set(v, T_E2H, 0.0005f); ref_set(v, T_E2D, 0.66f);
    v->e2curve = 0.0f;
    v->e2vel = 0.0f;
    v->depth[S_ENV2][T_PITCH] = 9.7f / 96.0f;      /* +9.7 st at the hit: ~28 Hz, falling to 16 Hz */
    ref_set(v, T_GAIN, -10.4f);
    ref_trig(v, 0);
    /* 7 HATS: very short metallic FM ticks, steps 9 and 13 */
    v = &k->v[6];
    v->engine = ENG_HAT;
    v->base[T_P1] = 0.35f; v->base[T_P2] = 1.0f; v->base[T_P3] = 1.0f; v->base[T_P4] = 0.7f;
    ref_set(v, T_E1A, 0.0005f); ref_set(v, T_E1H, 0.0005f); ref_set(v, T_E1D, 0.012f);
    ref_set(v, T_GAIN, 4.5f);
    ref_trig(v, 8);
    ref_trig(v, 12);
    /* 8 KICK (the kick / bass reference): ~110 Hz at the attack, a dry hit of ~30 ms; not in the pattern */
    v = &k->v[7];
    v->engine = ENG_KICK;
    ref_set(v, T_PITCH, 5.0f);                     /* 65.4 Hz base (an octave down) -> ~87 Hz, the sweep above */
    v->base[T_P1] = 0.12f; v->base[T_P2] = 0.15f; v->base[T_P3] = 0.5f; v->base[T_P4] = 0.0f;
    ref_set(v, T_E1A, 0.0005f); ref_set(v, T_E1H, 0.004f); ref_set(v, T_E1D, 0.05f);
    v->dmode = DM_TAPE;
    ref_set(v, T_DIST, 0.25f);
    ref_set(v, T_GAIN, -2.0f);
    k->g.bpm = 90.0f;
    k->g.r_mode = 0;
    k->g.r_decay = 1.6f;
    k->g.d_fb = 0.31f;
    k->name[0] = 'D'; k->name[1] = 'A'; k->name[2] = 'E'; k->name[3] = 'L'; k->name[4] = ' ';
    k->name[5] = 'R'; k->name[6] = 'E'; k->name[7] = 'F'; k->name[8] = 0;
}
