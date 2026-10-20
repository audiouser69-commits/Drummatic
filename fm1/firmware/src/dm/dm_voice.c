/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the voice (see dm_voice.h). */
#include "dm_voice.h"

const dm_map_t DM_MAP[DT_N] = {
    {-48.0f, 48.0f, 0, 0.0f},                                             /* pitch (semitones) */
    {0, 1, 0, 0.5f}, {0, 1, 0, 0.5f}, {0, 1, 0, 0.5f}, {0, 1, 0, 0.5f},     /* the engine's four knobs */
    {20.0f, 20000.0f, 1, 20000.0f},                                       /* cutoff (Hz) */
    {0, 1, 0, 0.0f}, {0, 1, 0, 0.0f},                                     /* resonance, distortion */
    {0.0005f, 5.0f, 1, 0.001f}, {0.0005f, 5.0f, 1, 0.001f}, {0.002f, 10.0f, 1, 0.3f},    /* ENV 1 A H D (s) */
    {0.0005f, 5.0f, 1, 0.001f}, {0.0005f, 20.0f, 1, 0.001f}, {0.002f, 20.0f, 1, 0.8f},   /* ENV 2 */
    {0, 1, 0, 0.35f}, {0, 1, 0, 1.0f}, {0, 1, 0, 0.35f}, {0, 1, 0, 1.0f},  /* LFO speed (normalised) / amp */
    {-1.0f, 1.0f, 0, 0.0f},                                               /* pan */
    {-60.0f, 12.0f, 0, 0.0f},                                             /* gain (dB) */
    {0, 1, 0, 0.0f}, {0, 1, 0, 0.0f},                                     /* delay / reverb send */
};
const char *const DM_TNAME[DT_N] = {"PITCH", "P1", "P2", "P3", "P4", "CUTOFF", "RES", "DISTORT",
                                    "ATTACK", "HOLD", "DECAY", "ATTACK", "HOLD", "DECAY",
                                    "SPEED", "AMP", "SPEED", "AMP", "PAN", "GAIN", "DELAY", "REVERB"};
const char *const DM_SNAME[DS_N] = {"RANDOM", "VELOCITY", "MACRO 1", "MACRO 2", "ENV 1", "ENV 2", "LFO 1", "LFO 2"};
const char *const DM_FNAME[FM_COUNT] = {"SCRM LP", "DARK LP", "SCRM HP", "DARK HP", "SCRM BP", "DARK BP",
                                        "COMB FB+", "COMB FB-", "COMB FF+", "COMB FF-", "COMB AP"};
const char *const DM_DNAME[DM_COUNT] = {"TAPE", "TUBE", "BITCRUSH", "RATECRSH", "WAVEFOLD", "PHASE", "SQUARE",
                                        "DROPOUT", "MINIGLCH", "MEGAGLCH", "CONTRAST"};
const char *const DM_LFODIV_NAME[DM_NLFODIV] = {"8 BAR", "4 BAR", "2 BAR", "1 BAR", "1/2", "1/4", "1/8", "1/16",
                                                "1/32", "1/4T", "1/8T", "1/16T", "1/64"};
const float DM_LFODIV_BEATS[DM_NLFODIV] = {32, 16, 8, 4, 2, 1, 0.5f, 0.25f, 0.125f, 2.0f / 3.0f, 1.0f / 3.0f,
                                           1.0f / 6.0f, 0.0625f};

float dm_denorm(uint32_t t, float n)
{
    const dm_map_t *m = &DM_MAP[t];
    n = dm_clamp01(n);
    return m->exp ? m->lo * dm_exp2(n * dm_log2(m->hi / m->lo)) : m->lo + (m->hi - m->lo) * n;
}
float dm_norm(uint32_t t, float v)
{
    const dm_map_t *m = &DM_MAP[t];
    if (!m->exp)
        return dm_clamp01((v - m->lo) / (m->hi - m->lo));
    return dm_clamp01(dm_log2(dm_maxf(v, m->lo) / m->lo) / dm_log2(m->hi / m->lo));
}
float dm_lfo_hz(float n)              /* 0..0.5 -> 0.01..20 Hz, 0.5..1 -> 20..440 Hz */
{
    n = dm_clamp01(n);
    return n < 0.5f ? 0.01f * dm_powf(2000.0f, n * 2.0f) : 20.0f * dm_powf(22.0f, (n - 0.5f) * 2.0f);
}

/* ------------------------------------------------------------------ AHD --- */
#define ST_IDLE 0
#define ST_ATK 1
#define ST_HOLD 2
#define ST_DEC 3
static void ahd_trigger(dm_ahd_t *e, float vel, float vamt)
{
    e->peak = 1.0f - dm_clamp01(vamt) * (1.0f - dm_clamp01(vel));
    e->start = e->value;                         /* soft reset: from the current value, no click */
    e->stage = ST_ATK;
    e->pos = 0.0f;
}
static void ahd_choke(dm_ahd_t *e)
{
    if (e->stage != ST_IDLE) {
        e->stage = ST_DEC;
        e->pos = 0.0f;
        e->decay = 0.015f;
        e->peak = e->value;
        e->curve = 0.0f;
        e->tscale = 1.0f;
    }
}
/* per sample; the decay's exponential iterated from its exact value at the stage's start */
static inline float ahd_run(dm_ahd_t *e)
{
    dm_ahd_t *k = e;
    switch (e->stage) {
    case ST_ATK: {
        float a = dm_maxf(1.0e-4f, e->attack * e->tscale);
        e->pos += DM_ISR;
        if (e->pos >= a) {
            e->stage = ST_HOLD;
            e->pos = 0.0f;
            e->value = e->peak;
        } else
            e->value = e->start + (e->peak - e->start) * (e->pos * k->inv_a);
        break;
    }
    case ST_HOLD:
        e->pos += DM_ISR;
        e->value = e->peak;
        if (e->pos >= k->hold_end) {
            float d = dm_maxf(1.0e-4f, e->decay * e->tscale);
            e->stage = ST_DEC;
            e->pos = 0.0f;
            k->inv_d = 1.0f / d;
            k->r = dm_expf(-6.9f * DM_ISR * k->inv_d);
            k->ex = 1.0f;
        }
        break;
    case ST_DEC: {
        float t, lin, ex;
        e->pos += DM_ISR;
        k->ex *= k->r;
        t = e->pos * k->inv_d;
        if (t >= 1.0f) {
            e->stage = ST_IDLE;
            e->value = 0.0f;
            break;
        }
        lin = 1.0f - t;
        ex = (k->ex - 0.001f) * (1.0f / 0.999f);
        e->value = e->peak * dm_maxf(0.0f, dm_lerp(lin, ex, dm_clamp01(e->curve)));
        break;
    }
    default:
        e->value = 0.0f;
        break;
    }
    return e->value;
}
static void ahd_prepare(dm_ahd_t *e)                      /* after a trigger or a choke */
{
    dm_ahd_t *k = e;
    k->inv_a = 1.0f / dm_maxf(1.0e-4f, e->attack * e->tscale);
    k->hold_end = e->hold * e->tscale;
    if (e->stage == ST_DEC) {
        float d = dm_maxf(1.0e-4f, e->decay * e->tscale);
        k->inv_d = 1.0f / d;
        k->r = dm_expf(-6.9f * DM_ISR * k->inv_d);
        k->ex = dm_expf(-6.9f * e->pos * k->inv_d);
    }
}

/* ------------------------------------------------------------------ LFO --- */
static float lfo_raw(const dm_lfo_t *l)
{
    float p = l->phase;
    switch (l->wave) {
    case LW_WAVETABLE: {
        float s = dm_clamp01(l->shape) * 4.0f, sine, tri2, saw, sq;
        tri2 = p < 0.25f ? p * 4.0f : p < 0.75f ? 2.0f - p * 4.0f : p * 4.0f - 4.0f;
        if (s < 1.0f) {
            sine = dm_sin_cyc(p);
            return dm_lerp(sine, tri2, s);
        }
        saw = 2.0f * p - 1.0f;
        if (s < 2.0f)
            return dm_lerp(tri2, saw, s - 1.0f);
        sq = p < 0.5f ? 1.0f : -1.0f;
        if (s < 3.0f)
            return dm_lerp(saw, sq, s - 2.0f);
        return dm_lerp(sq, l->rnda, s - 3.0f);
    }
    case LW_TRITILT: {
        float k = dm_clampf(l->shape, 0.001f, 0.999f);
        return p < k ? (2.0f * p / k - 1.0f) : (1.0f - 2.0f * (p - k) / (1.0f - k));
    }
    case LW_PULSE: return p < dm_clampf(l->shape, 0.02f, 0.98f) ? 1.0f : -1.0f;
    case LW_SMOOTHRND: {
        float sm = dm_clamp01(l->shape), t = sm <= 0.001f ? 1.0f : dm_clamp01(p / sm), tt = t * t * (3.0f - 2.0f * t);
        return dm_lerp(l->rndb, l->rnda, tt);
    }
    default: return 0.0f;
    }
}
static void lfo_trigger(dm_lfo_t *l, int was_silent)
{
    switch (l->reset) {
    case LR_ALWAYS: l->phase = 0.0f; break;
    case LR_SOFT: if (was_silent) l->phase = 0.0f; break;
    case LR_SH: l->held = lfo_raw(l); break;
    case LR_ONESHOT: l->phase = 0.0f; l->done = 0; break;
    default: break;
    }
}
static float lfo_advance(dm_lfo_t *l, float dt, float hz)
{
    float v;
    if (!(l->reset == LR_ONESHOT && l->done)) {
        l->phase += hz * dt;
        if (l->phase >= 1.0f) {
            l->phase -= dm_floorf(l->phase);
            l->rndb = l->rnda;
            l->rnda = dm_rng_bi(&l->rng);
            if (l->reset == LR_ONESHOT) {
                l->done = 1;
                l->phase = 0.9999f;
            }
        }
    }
    v = l->reset == LR_SH ? l->held : lfo_raw(l);
    if (!l->bip)
        v = 0.5f * (v + 1.0f);
    return v * l->amp;
}

/* ---------------------------------------------------------------- TONE --- */
static void filt_reset(dm_filt_t *f)
{
    uint32_t i;
    f->svf.ic1 = f->svf.ic2 = 0.0f;
    f->dark.z = 0.0f;
    if (f->comb.b)
        for (i = 0; i < f->comb.n; i++)
            f->comb.b[i] = 0;
}
static void dist_reset(dm_dist_t *d, uint32_t mode)
{
    uint32_t i;
    d->held = d->acc = d->last = 0.0f;
    d->dropping = 0;
    d->gpos = d->gvel = 0.0f;
    if (mode == DM_PHASE && d->micro.b)
        for (i = 0; i < d->micro.n; i++)
            d->micro.b[i] = 0;
    if ((mode == DM_MINIGLITCH || mode == DM_MEGAGLITCH) && d->gbuf.b)
        for (i = 0; i < d->gbuf.n; i++)
            d->gbuf.b[i] = 0;
}

/* per block: the filter's coefficients (both channels) */
static void filt_ctrl(dm_voice_t *v, uint32_t mode, float cutoff, float res)
{
    uint32_t c;
    res = dm_clamp01(res);
    if (mode <= FM_DARK_BP) {
        int scream = (mode % 2u) == 0u;
        float q = scream ? 0.5f + res * res * 40.0f : 0.5f + res * 4.0f;
        dm_svf_set(&v->filt[0].svf, cutoff, q);
        v->filt[0].dark.a = dm_onepole_a(cutoff * 1.3f + 200.0f);
        for (c = 1; c < 2u; c++) {
            v->filt[c].svf.k = v->filt[0].svf.k;
            v->filt[c].svf.a1 = v->filt[0].svf.a1;
            v->filt[c].svf.a2 = v->filt[0].svf.a2;
            v->filt[c].svf.a3 = v->filt[0].svf.a3;
            v->filt[c].dark.a = v->filt[0].dark.a;
        }
        v->bpk = mode == FM_DARK_BP ? 1.0f + res * 1.5f : 1.0f;
    } else {
        float n = v->filt[0].comb.b ? (float)v->filt[0].comb.n : 64.0f;
        v->comb_d = dm_clampf(DM_FS / dm_maxf(15.0f, cutoff), 2.0f, n - 4.0f);
        v->comb_g = res * 0.985f;
    }
}
static inline float filt_run(dm_filt_t *f, const dm_voice_t *k, float x, uint32_t mode)
{
    float y, del;
    if (mode <= FM_DARK_BP) {
        int scream = (mode % 2u) == 0u;
        float in = scream ? dm_tanh(x * 1.2f) : x;
        dm_svf_run(&f->svf, in);
        y = mode <= FM_DARK_LP ? f->svf.lp : mode <= FM_DARK_HP ? f->svf.hp : f->svf.bp;
        if (scream)
            return dm_tanh(y * 0.9f) * 1.05f;
        return dm_op_lp(&f->dark, y) * k->bpk;
    }
    if (!f->comb.b)
        return x;
    switch (mode) {
    case FM_COMB_FBP: del = dm_dl_read(&f->comb, k->comb_d); y = x + k->comb_g * del; dm_dl_push(&f->comb, dm_tanh(y)); return y;
    case FM_COMB_FBM: del = dm_dl_read(&f->comb, k->comb_d); y = x - k->comb_g * del; dm_dl_push(&f->comb, dm_tanh(y)); return y;
    case FM_COMB_FFP: dm_dl_push(&f->comb, x); return (x + k->comb_g * dm_dl_read(&f->comb, k->comb_d)) * 0.7f;
    case FM_COMB_FFM: dm_dl_push(&f->comb, x); return (x - k->comb_g * dm_dl_read(&f->comb, k->comb_d)) * 0.7f;
    case FM_COMB_AP: {
        float vv;
        del = dm_dl_read(&f->comb, k->comb_d);
        vv = x + k->comb_g * del;
        y = del - k->comb_g * vv;
        dm_dl_push(&f->comb, dm_tanh(vv));
        return y;
    }
    default: return x;
    }
}

/* the distortion's coefficients from the amount (once a block) */
static void dist_ctrl(dm_dist_t *d, uint32_t mode, float amt)
{
    amt = dm_clamp01(amt);
    d->k3 = amt;
    switch (mode) {
    case DM_TAPE: d->k1 = 1.0f + amt * 12.0f; d->k2 = 1.0f / (0.8f + amt * 0.6f); break;
    case DM_TUBE: d->k1 = 1.0f + amt * 15.0f; d->k2 = 1.0f / (1.0f + amt * 0.5f); break;
    case DM_BITCRUSH: {
        float bits = dm_lerp(16.0f, 1.0f, dm_powf(amt, 0.6f));
        d->k1 = dm_exp2(bits - 1.0f);
        d->k2 = 1.0f / d->k1;
        break;
    }
    case DM_RATECRUSH: d->k1 = 1.0f - dm_powf(amt, 1.5f) * 0.995f; break;
    case DM_WAVEFOLD: d->k1 = 1.0f + amt * 8.0f; break;
    case DM_PHASE: d->k1 = amt * DM_FS * 0.004f; d->k2 = dm_minf(1.0f, amt * 2.0f); break;
    case DM_SQUARE: d->k1 = 1.0f - amt * 0.98f; d->k2 = 0.6f + 0.4f * (1.0f - amt); break;
    case DM_DROPOUT: d->k1 = amt * 0.85f; break;
    case DM_CONTRAST: d->k1 = amt * 3.0f * 0.25f * (1.0f / DM_TWOPI); d->k2 = dm_minf(1.0f, amt * 2.0f); break;
    default: break;
    }
}
static inline float fold_f(float v)
{
    int i;
    for (i = 0; i < 6 && (v > 1.0f || v < -1.0f); i++)
        v = v > 1.0f ? 2.0f - v : -2.0f - v;
    return v;
}
static inline float glitch(dm_dist_t *d, float x, float amt, int stereo)
{
    dm_rng_t *r = stereo ? &d->grng : &d->rng;
    float target, y;
    d->gvel = d->gvel * 0.999f + dm_rng_bi(r) * 0.0004f * (0.2f + amt);
    d->gpos = dm_clampf(d->gpos + d->gvel, -1.0f, 1.0f);
    target = dm_clampf(amt * 0.2f * (1.0f + d->gpos * amt * 0.9f), 0.001f, d->gmax);
    if (dm_absf(target - d->gtime) > 0.004f && d->gxf >= 1.0f) {
        d->gprev = d->gtime;
        d->gtime = target;
        d->gxf = 0.0f;
    }
    if (d->gxf < 1.0f) {
        d->gxf = dm_minf(1.0f, d->gxf + 1.0f / (0.01f * DM_FS));
        y = dm_dl_read(&d->gbuf, d->gprev * DM_FS) * (1.0f - d->gxf) + dm_dl_read(&d->gbuf, d->gtime * DM_FS) * d->gxf;
    } else
        y = dm_dl_read(&d->gbuf, d->gtime * DM_FS);
    dm_dl_push(&d->gbuf, dm_tanh(x + y * amt * 0.75f));
    return dm_lerp(x, x * 0.6f + y, dm_minf(1.0f, amt * 1.5f));
}
static inline float dist_run(dm_dist_t *d, float x, uint32_t mode)
{
    float amt = d->k3;
    switch (mode) {
    case DM_TAPE: return dm_tanh(x * d->k1 * 0.8f + 0.05f * x * x) * d->k2;
    case DM_TUBE: {
        float v = x * d->k1;
        v = v >= 0.0f ? dm_tanh(v) : dm_tanh(v * 0.6f) * 0.75f;
        return (v + 0.1f * dm_absf(v)) * d->k2;
    }
    case DM_BITCRUSH:
        if (amt <= 0.0001f)
            return x;
        return dm_roundf(dm_clampf(x, -1.0f, 1.0f) * d->k1) * d->k2;
    case DM_RATECRUSH:
        if (amt <= 0.0001f)
            return x;
        d->acc += d->k1;
        if (d->acc >= 1.0f) {
            d->acc -= 1.0f;
            d->held = x;
        }
        return d->held;
    case DM_WAVEFOLD: return fold_f(x * d->k1);
    case DM_PHASE: {
        float del, y;
        if (!d->micro.b)
            return x;
        dm_dl_push(&d->micro, x);
        del = 2.0f + (0.5f + 0.5f * dm_tanh(d->last * 3.0f)) * d->k1;
        y = dm_dl_read(&d->micro, del);
        d->last = y;
        return dm_lerp(x, y, d->k2);
    }
    case DM_SQUARE: {
        float s = x >= 0.0f ? 1.0f : -1.0f, m = dm_powf(dm_absf(x), d->k1);
        return s * dm_minf(1.0f, m) * d->k2;
    }
    case DM_DROPOUT:
        if (d->last <= 0.0f && x > 0.0f)
            d->dropping = dm_rng_uni(&d->drng) < d->k1;
        d->last = x;
        return d->dropping ? 0.0f : x;
    case DM_MINIGLITCH: return amt <= 0.001f || !d->gbuf.b ? x : glitch(d, x, amt, 0);
    case DM_MEGAGLITCH: return amt <= 0.001f || !d->gbuf.b ? x : glitch(d, x, amt, 1);
    case DM_CONTRAST: {
        float c = dm_clampf(x, -1.5f, 1.5f), y = dm_sin_cyc(0.25f * c + d->k1 * dm_sin_cyc(c));
        return dm_lerp(x, y, d->k2);
    }
    default: return x;
    }
}

/* --------------------------------------------------------------- layout --- */
/* The voice's arena of int16 delay lines, cut for what the sound needs now: the engine's line (KOMBUCHA,
 * COMBINATORIC; one per instance), the comb filter's (one per channel), PHASE's or the glitch's (one per channel).
 * The glitch takes what is left (186 ms with nothing else; the original's buffer is 250 ms). Redone (lines cleared)
 * when the engine, STEREO, the filter mode or the distortion mode change. */
static void voice_layout(dm_voice_t *v, const dm_vset_t *s, uint32_t index)
{
    uint32_t eng = s->engine < ENG_COUNT ? s->engine : 0u, stereo = s->stereo != 0, fmode = s->fmode % FM_COUNT,
             dmode = s->dmode % DM_COUNT, nch, used = 0, avail, c, eline = dm_eng_needs_line(eng), comb = 0, dl = 0;
    int new_eng = !v->laid || eng != v->lay_eng;
    nch = stereo || dmode == DM_MEGAGLITCH ? 2u : 1u;
    for (c = 0; c < 2u; c++) {
        uint32_t want = c == 0u || stereo ? eline : 0u;
        if (new_eng || (c == 1u && stereo != v->lay_stereo))
            dm_eng_reset(&v->eng[c], eng, 0x1000u + eng * 7u + c * 131u, want ? v->arena + used : 0, want);
        else if (want)
            dm_dl_init(&v->eng[c].dl, v->arena + used, want, 1.0f);
        else
            v->eng[c].dl.b = 0;
        used += want;
    }
    avail = (DM_ARENA - used) / nch;
    if (fmode >= FM_COMB_FBP) {
        comb = dmode == DM_MINIGLITCH || dmode == DM_MEGAGLITCH ? avail / 3u : avail;
        comb = comb > 2048u ? 2048u : comb;
    }
    if (dmode == DM_PHASE)
        dl = 512u;
    else if (dmode == DM_MINIGLITCH || dmode == DM_MEGAGLITCH) {
        dl = avail - comb;
        dl = dl > 11025u ? 11025u : dl;
    }
    for (c = 0; c < 2u; c++) {
        if (c < nch && comb >= 64u) {
            dm_dl_init(&v->filt[c].comb, v->arena + used, comb, 1.0f);
            used += comb;
        } else
            v->filt[c].comb.b = 0;
        v->dist[c].micro.b = v->dist[c].gbuf.b = 0;
        if (c < nch && dl >= 64u) {
            if (dmode == DM_PHASE)
                dm_dl_init(&v->dist[c].micro, v->arena + used, dl, 4.0f);
            else
                dm_dl_init(&v->dist[c].gbuf, v->arena + used, dl, 1.0f);
            used += dl;
        }
        v->dist[c].gmax = dm_minf(0.24f, ((float)(dl > 8u ? dl : 8u) - 4.0f) * DM_ISR);
    }
    v->nch = (uint8_t)nch;
    v->k_cut = v->k_dist = -1.0e9f;                     /* (the lines moved: the block's constants again) */
    v->k_fm = v->k_dm = 0xFF;
    v->lay_eng = (uint8_t)eng;
    v->lay_stereo = (uint8_t)stereo;
    v->lay_fmode = (uint8_t)fmode;
    v->lay_dmode = (uint8_t)dmode;
    v->laid = 1;
    (void)index;
}

void dm_voice_init(dm_voice_t *v, uint32_t index, int16_t *arena)
{
    uint8_t *b = (uint8_t *)v;
    uint32_t i, c;
    for (i = 0; i < sizeof *v; i++)
        b[i] = 0;
    v->arena = arena;
    v->rng.s = 0xC0FFEEu;
    for (c = 0; c < 2u; c++) {
        v->dist[c].rng.s = 0x6117u;
        v->dist[c].grng.s = 0x6117u + c * 7919u;
        v->dist[c].drng.s = 0xD15u;
        v->dist[c].gtime = v->dist[c].gprev = 0.05f;
        v->dist[c].gxf = 1.0f;
        v->dc[c].a = dm_onepole_a(12.0f);
        v->lfo[c].rng.s = 0xBEEFu;
        v->lfo[c].amp = 1.0f;
        v->lfo[c].shape = 0.5f;
        v->lfo[c].bip = 1;
    }
    v->env1.attack = v->env2.attack = 0.001f;
    v->env1.hold = v->env2.hold = 0.001f;
    v->env1.decay = 0.2f;
    v->env2.decay = 0.2f;
    v->env1.curve = v->env2.curve = 1.0f;
    v->env1.tscale = v->env2.tscale = 1.0f;
    v->choke_fade = 1.0f;
    v->trig.vel = 1.0f;
    v->trig.envscale = 1.0f;
    v->trig.macro2 = -1.0f;
    for (i = 0; i < DT_N; i++) {
        v->last_n[i] = dm_norm(i, DM_MAP[i].def);
        v->cur[i] = dm_denorm(i, v->last_n[i]);
    }
    v->k_cut = v->k_res = v->k_dist = v->k_gain = v->k_pan = -1.0e9f;
    v->k_fm = v->k_dm = 0xFF;
    (void)index;
}

int dm_voice_active(const dm_voice_t *v) { return v->env1.stage != ST_IDLE || v->env2.stage != ST_IDLE; }

static void compute_targets(dm_voice_t *v, const dm_vset_t *s)
{
    float src[DS_N];
    uint32_t t, k;
    src[S_RANDOM] = v->rand_val;
    src[S_VELOCITY] = v->vel_val;
    src[S_MACRO1] = s->macro1;
    src[S_MACRO2] = v->trig.macro2 >= 0.0f ? v->trig.macro2 : s->macro2;
    src[S_ENV1] = v->env1.value;
    src[S_ENV2] = v->env2.value;
    src[S_LFO1] = v->lfo_val[0];
    src[S_LFO2] = v->lfo_val[1];
    for (t = 0; t < DT_N; t++) {
        float n = s->base[t] + v->var_off[t];
        for (k = 0; k < DS_N; k++) {
            float d = s->depth[k][t];
            if (d != 0.0f)
                n += d * src[k] * s->modamp;
        }
        n = dm_clamp01(n);
        if (n != v->last_n[t]) {
            v->last_n[t] = n;
            v->cur[t] = dm_denorm(t, n);
        }
    }
    v->cur_dist = dm_clamp01(v->cur[T_DIST] + (v->trig.accent ? 0.3f : 0.0f));
}

static void voice_blockk(dm_voice_t *v, const dm_vset_t *s)      /* engines, TONE, gains from cur */
{
    float f = dm_eng_base_freq(v->lay_eng) * dm_semis(v->cur[T_PITCH] + v->trig.pitch), p[4], pan, pl, pr, g, ds, rs;
    uint32_t c;
    p[0] = v->cur[T_P1];
    p[1] = v->cur[T_P2];
    p[2] = v->cur[T_P3];
    p[3] = v->cur[T_P4];
    dm_eng_ctrl(&v->eng[0], f, p);
    if (s->stereo)
        dm_eng_ctrl(&v->eng[1], f * 1.0045f, p);
    dm_svf_guard(&v->filt[0].svf);
    dm_svf_guard(&v->filt[1].svf);
    if (v->cur[T_CUTOFF] != v->k_cut || v->cur[T_RES] != v->k_res || v->lay_fmode != v->k_fm) {
        v->k_cut = v->cur[T_CUTOFF];
        v->k_res = v->cur[T_RES];
        v->k_fm = v->lay_fmode;
        filt_ctrl(v, v->lay_fmode, v->cur[T_CUTOFF], v->cur[T_RES]);
    }
    if (v->cur_dist != v->k_dist || v->lay_dmode != v->k_dm) {
        v->k_dist = v->cur_dist;
        v->k_dm = v->lay_dmode;
        for (c = 0; c < 2u; c++)
            dist_ctrl(&v->dist[c], v->lay_dmode, v->cur_dist);
    }
    pan = dm_clampf(v->cur[T_PAN] + v->trig.pan, -1.0f, 1.0f);
    if (v->cur[T_GAIN] != v->k_gain || pan != v->k_pan) {
        v->k_gain = v->cur[T_GAIN];
        v->k_pan = pan;
        g = dm_db2gain(v->cur[T_GAIN] <= -59.9f ? -200.0f : v->cur[T_GAIN]);
        pl = dm_cos_cyc((pan + 1.0f) * 0.125f) * 1.4142f;      /* cos((pan + 1) pi / 4) */
        pr = dm_sin_cyc((pan + 1.0f) * 0.125f) * 1.4142f;
        v->g_l = g * dm_minf(1.0f, pl);
        v->g_r = g * dm_minf(1.0f, pr);
    }
    ds = dm_clamp01(v->cur[T_DSEND] + v->trig.sdly);
    rs = dm_clamp01(v->cur[T_RSEND] + v->trig.srev);
    v->s_d = ds * ds;
    v->s_r = rs * rs;
}

static void lfo_settings(dm_voice_t *v, const dm_vset_t *s)
{
    v->lfo[0].shape = s->l1shape;
    v->lfo[0].bip = s->l1bip;
    v->lfo[0].wave = s->l1wave;
    v->lfo[0].reset = s->l1reset;
    v->lfo[1].shape = s->l2shape;
    v->lfo[1].bip = s->l2bip;
    v->lfo[1].wave = s->l2wave;
    v->lfo[1].reset = s->l2reset;
}

void dm_voice_ctrl(dm_voice_t *v, const dm_vset_t *s, float bpm)
{
    uint32_t k;
    if (!v->laid || s->engine != v->lay_eng || (s->stereo != 0) != v->lay_stereo || s->fmode != v->lay_fmode ||
        s->dmode != v->lay_dmode)
        voice_layout(v, s, 0);
    lfo_settings(v, s);
    for (k = 0; k < 2u; k++) {
        float spn = v->cur[k ? T_L2SPD : T_L1SPD], hz;
        int sync = k ? s->l2sync : s->l1sync;
        v->lfo[k].amp = v->cur[k ? T_L2AMP : T_L1AMP];
        hz = sync ? (bpm / 60.0f) / DM_LFODIV_BEATS[dm_lfo_div(spn)] : dm_lfo_hz(spn);
        v->lfo_val[k] = lfo_advance(&v->lfo[k], (float)DM_CTL * DM_ISR, hz);
    }
    if (!dm_voice_active(v) && !v->choking) {    /* silent: only what the LFOs read (their speed and depth) */
        static const uint8_t LT[4] = {T_L1SPD, T_L1AMP, T_L2SPD, T_L2AMP};
        float src[DS_N];
        uint32_t j, q;
        src[S_RANDOM] = v->rand_val;
        src[S_VELOCITY] = v->vel_val;
        src[S_MACRO1] = s->macro1;
        src[S_MACRO2] = v->trig.macro2 >= 0.0f ? v->trig.macro2 : s->macro2;
        src[S_ENV1] = src[S_ENV2] = 0.0f;
        src[S_LFO1] = v->lfo_val[0];
        src[S_LFO2] = v->lfo_val[1];
        for (j = 0; j < 4u; j++) {
            uint32_t t = LT[j];
            float n = s->base[t] + v->var_off[t];
            for (q = 0; q < DS_N; q++)
                if (s->depth[q][t] != 0.0f)
                    n += s->depth[q][t] * src[q] * s->modamp;
            n = dm_clamp01(n);
            if (n != v->last_n[t]) {
                v->last_n[t] = n;
                v->cur[t] = dm_denorm(t, n);
            }
        }
        return;
    }
    compute_targets(v, s);
    voice_blockk(v, s);
}

void dm_voice_trigger(dm_voice_t *v, const dm_vset_t *s, const dm_trig_t *t, float bpm)
{
    int was_silent = !dm_voice_active(v);
    uint32_t i, c;
    float var;
    if (!v->laid || s->engine != v->lay_eng || (s->stereo != 0) != v->lay_stereo || s->fmode != v->lay_fmode ||
        s->dmode != v->lay_dmode)
        voice_layout(v, s, 0);
    v->trig = *t;
    v->rand_val = dm_rng_uni(&v->rng);
    v->vel_val = dm_clamp01(t->vel);
    var = dm_clamp01(s->variation + t->var);
    for (i = 0; i < DT_N; i++) {
        float range = 0.18f;
        if (i == T_GAIN || i == T_DSEND || i == T_RSEND)
            range = 0.08f;
        if (i == T_PITCH)
            range = 0.04f;
        v->var_off[i] = (s->locks >> i) & 1u ? 0.0f : dm_rng_bi(&v->rng) * var * var * range * 2.0f;
    }
    compute_targets(v, s);
    v->env1.attack = v->cur[T_E1A];
    v->env1.hold = v->cur[T_E1H];
    v->env1.decay = v->cur[T_E1D];
    v->env1.curve = s->e1curve;
    v->env2.attack = v->cur[T_E2A];
    v->env2.hold = v->cur[T_E2H];
    v->env2.decay = v->cur[T_E2D];
    v->env2.curve = s->e2curve;
    v->env1.tscale = v->env2.tscale = t->envscale;
    ahd_trigger(&v->env1, v->vel_val, s->e1vel);
    ahd_trigger(&v->env2, v->vel_val, s->e2vel);
    ahd_prepare(&v->env1);
    ahd_prepare(&v->env2);
    lfo_settings(v, s);
    lfo_trigger(&v->lfo[0], was_silent);
    lfo_trigger(&v->lfo[1], was_silent);
    for (c = 0; c < 2u; c++)
        if (c == 0u || s->stereo)
            dm_eng_trigger(&v->eng[c], v->vel_val);
    if (was_silent)
        for (c = 0; c < 2u; c++) {
            filt_reset(&v->filt[c]);
            dist_reset(&v->dist[c], v->lay_dmode);
            v->dc[c].z = 0.0f;
        }
    v->choking = 0;
    v->choke_fade = 1.0f;
    (void)bpm;
    voice_blockk(v, s);
}

void dm_voice_choke(dm_voice_t *v)
{
    ahd_choke(&v->env1);
    ahd_choke(&v->env2);
    ahd_prepare(&v->env1);
    ahd_prepare(&v->env2);
    v->choke_fade = 1.0f;
    v->choking = 1;
}

void dm_voice_transport_start(dm_voice_t *v)
{
    v->lfo[0].phase = v->lfo[1].phase = 0.0f;
    v->lfo[0].done = v->lfo[1].done = 0;
}

static inline float tone(dm_voice_t *v, uint32_t c, float x, uint32_t post)
{
    if (post) {
        x = filt_run(&v->filt[c], v, x, v->lay_fmode);
        x = dist_run(&v->dist[c], x, v->lay_dmode);
    } else {
        x = dist_run(&v->dist[c], x, v->lay_dmode);
        x = filt_run(&v->filt[c], v, x, v->lay_fmode);
    }
    return dm_finite(x) ? x : 0.0f;
}

void dm_voice_render(dm_voice_t *v, const dm_vset_t *s, float *outl, float *outr, float *dl, float *dr, float *rl,
                     float *rr, uint32_t n)
{
    uint32_t i, stereo = s->stereo != 0 && v->lay_stereo, post = s->post != 0, two = v->nch == 2u, env2 = s->env2 != 0;
    float peak = 0.0f, gl = v->g_l, gr = v->g_r, sd = v->s_d, sr = v->s_r;
    for (i = 0; i < n; i++) {
        float a1, a2, amp, xl, xr, ol, or_;
        if (!dm_voice_active(v) && !v->choking)
            break;
        a1 = ahd_run(&v->env1);
        a2 = ahd_run(&v->env2);
        amp = env2 ? a2 : a1;
        if (amp > 0.0f) {                         /* (silent amplitude: the engine is not heard, skip it) */
            xl = dm_eng_run(&v->eng[0]) * amp;
            xr = stereo ? dm_eng_run(&v->eng[1]) * amp : xl;
        } else
            xl = xr = 0.0f;
        xl = dm_op_hp(&v->dc[0], xl);
        if (two) {
            xr = stereo ? dm_op_hp(&v->dc[1], xr) : xl;
            xl = tone(v, 0, xl, post);
            xr = tone(v, 1, xr, post);
        } else
            xl = xr = tone(v, 0, xl, post);
        if (v->choking) {
            v->choke_fade *= 0.997f;
            xl *= v->choke_fade;
            xr *= v->choke_fade;
            if (v->choke_fade < 0.001f) {
                v->choking = 0;
                filt_reset(&v->filt[0]);
                filt_reset(&v->filt[1]);
            }
        }
        ol = xl * gl;
        or_ = xr * gr;
        outl[i] += ol;
        outr[i] += or_;
        dl[i] += ol * sd;
        dr[i] += or_ * sd;
        rl[i] += ol * sr;
        rr[i] += or_ * sr;
        ol = dm_absf(ol) > dm_absf(or_) ? dm_absf(ol) : dm_absf(or_);
        if (ol > peak)
            peak = ol;
    }
    v->level = dm_maxf(peak, v->level * 0.85f);
}
