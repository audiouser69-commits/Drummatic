/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the synth engines (see dm_engines.h). Phases are uint32 (2^32 = one cycle): a tick adds the
 * increment and the engines read the new phase, as Phasor::tick of the original. */
#include "dm_engines.h"

const dm_eng_info_t DM_ENG_INFO[ENG_COUNT] = {
    {"KICK", {"P.DECAY", "P.DEPTH", "WAVE", "SUB"}, 1},
    {"SNARE", {"P.DECAY", "P.DEPTH", "CUTOFF", "S.RATE"}, 0},
    {"HAT", {"BLEND", "S.RATE", "CUTOFF", "HARMONIC"}, 0},
    {"CLAP", {"CLAPS", "SPACING", "CUTOFF", "REV DEC"}, 0},
    {"FM", {"RATIO", "DEPTH", "FEEDBACK", "ENV"}, 0},
    {"MODAL", {"HARMONIC", "DAMPEN", "VARY", "BRIGHT"}, 0},
    {"COWBELL", {"FILTER", "CLICK", "NOISE", "HARMONIC"}, 0},
    {"ZAP", {"ENV", "CLICK", "OVERTONE", "SUB"}, 1},
    {"PILLOW", {"PILLOW", "CLICK", "RATTLE", "SUB"}, 1},
    {"TRIPWIRE", {"P.DECAY", "P.DEPTH", "TRANSNT", "SQUARE"}, 1},
    {"RADIATOR", {"ENV", "TICK", "OVERTONE", "ROAST"}, 1},
    {"KOMBUCHA", {"S.RATE", "FEEDBACK", "P.ENV", "DAMP"}, 0},
    {"LPG", {"FOLD", "SHAPE", "NOISE", "DAMP"}, 0},
    {"BUILDER", {"OSC WAVE", "OSC GAIN", "N.TILT", "N.GAIN"}, 0},
    {"ADDITIVE", {"F.RATIO", "A.RATIO", "PARTIALS", "AMP ENV"}, 0},
    {"CHAOS", {"RATIO", "CHAOS", "FILTER", "CH.ENV"}, 0},
    {"VOSIM", {"PART 1", "PART 2", "FM", "FM ENV"}, 0},
    {"SUMSYN", {"RATIO", "BRIGHT", "PM", "PM ENV"}, 0},
    {"RESON", {"HARMONIC", "DAMPEN", "MATERIAL", "RING"}, 0},
    {"COMBINAT", {"C.RATIO", "FEEDBACK", "P.ENV", "FM"}, 0},
    {"TERRAIN", {"PITCH 2", "SHAPE 1", "SHAPE 2", "TERRAIN"}, 0},
    {"SHAME", {"PITCH 2", "FM", "FEEDBACK", "SHAME"}, 0},
    {"ENTANGLE", {"PITCH 2", "FM", "FEEDBACK", "TANGLE"}, 0},
    {"HAYWIRE", {"OVERTONE", "P.DEPTH", "BRIGHT", "RATTLE"}, 0},
    {"SCRAPYRD", {"RATIO", "ZAP", "MATERIAL", "FM"}, 0},
};

#define PH_QUARTER 0x40000000u
#define NYQ_INC 2147483520.0f

static inline float noise(dm_eng_t *e) { return dm_rng_bi(&e->rng); }
static inline uint32_t finc(float v) { return !(v > 0.0f) ? 0u : v >= NYQ_INC ? (uint32_t)NYQ_INC : (uint32_t)v; }
static inline float tick(uint32_t *ph, uint32_t inc) { *ph += inc; return dm_sin_ph(*ph); }
static inline float morph_wave(uint32_t ph, float shape)     /* sine -> triangle -> saw -> square */
{
    float p = dm_ph01(ph), s = dm_clamp01(shape) * 3.0f, sine, tri, saw, sq;
    if (s < 1.0f) {
        sine = dm_sin_ph(ph);
        tri = 4.0f * dm_absf(p - 0.5f) - 1.0f;
        return dm_lerp(sine, tri, s);
    }
    tri = 4.0f * dm_absf(p - 0.5f) - 1.0f;
    saw = 2.0f * p - 1.0f;
    if (s < 2.0f)
        return dm_lerp(tri, saw, s - 1.0f);
    sq = p < 0.5f ? 1.0f : -1.0f;
    return dm_lerp(saw, sq, s - 2.0f);
}
static inline float dm_fold(float v, int n)
{
    int i;
    for (i = 0; i < n && (v > 1.0f || v < -1.0f); i++)
        v = v > 1.0f ? 2.0f - v : -2.0f - v;
    return v;
}

static const float BAR[8] = {1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f, 24.8f, 31.9f};
static const float MEMB[8] = {1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f, 3.156f, 3.501f};
static const float PLATE[8] = {1.0f, 1.42f, 2.08f, 2.79f, 3.41f, 4.27f, 5.05f, 6.18f};
static const float R808[6] = {1.0f, 1.483f, 1.80f, 2.546f, 2.630f, 3.896f};

uint32_t dm_eng_needs_line(uint32_t id) { return id == ENG_KOMBUCHA || id == ENG_COMBINATORIC ? 2048u : 0u; }
float dm_eng_base_freq(uint32_t id) { return DM_ENG_INFO[id < ENG_COUNT ? id : 0].octave_down ? 65.41f : 130.81f; }

void dm_eng_reset(dm_eng_t *e, uint32_t id, uint32_t seed, int16_t *line, uint32_t line_n)
{
    uint8_t *b = (uint8_t *)&e->u;
    uint32_t i;
    for (i = 0; i < sizeof e->u; i++)
        b[i] = 0;
    e->id = (uint8_t)(id < ENG_COUNT ? id : 0);
    e->vary = 0;
    e->rng.s = seed ? seed : 1u;
    e->f = 0;
    e->vel = 1.0f;
    for (i = 0; i < 4u; i++)
        e->p[i] = 0.5f;
    e->dl.b = 0;
    if (line && line_n >= 64u && dm_eng_needs_line(e->id))
        dm_dl_init(&e->dl, line, line_n, 1.0f);
}

void dm_eng_trigger(dm_eng_t *e, float vel)
{
    uint32_t i;
    e->vel = vel;
    switch (e->id) {
    case ENG_KICK: e->u.kick.ph = PH_QUARTER; e->u.kick.sub = PH_QUARTER; e->u.kick.pe.v = 1.0f; break;
    case ENG_SNARE: e->u.snare.ph = 0; e->u.snare.pe.v = 1.0f; e->u.snare.ne.v = 1.0f; break;
    case ENG_HAT: break;
    case ENG_CLAP:
        e->u.clap.left = 1;
        e->u.clap.cnt = 0;
        e->u.clap.burst.v = 1.0f;
        e->u.clap.tail.v = 0.0f;
        break;
    case ENG_FM: e->u.fm.env.v = 1.0f; break;
    case ENG_MODAL:
        for (i = 0; i < DM_MODAL_N; i++) {
            e->u.modal.amp[i] = 1.0f;
            e->u.modal.ph[i] = 0;
        }
        e->vary = 1;
        break;
    case ENG_COWBELL: e->u.cow.click.v = 1.0f; break;
    case ENG_ZAP: e->u.zap.ph = PH_QUARTER; e->u.zap.pe.v = 1.0f; e->u.zap.click.v = 1.0f; break;
    case ENG_PILLOW: e->u.pillow.ph = PH_QUARTER; e->u.pillow.pe.v = 1.0f; e->u.pillow.click.v = 1.0f; break;
    case ENG_TRIPWIRE:
        e->u.trip.ph = PH_QUARTER;
        e->u.trip.e1.v = e->u.trip.e2.v = e->u.trip.tr.v = 1.0f;
        break;
    case ENG_RADIATOR: e->u.rad.ph = PH_QUARTER; e->u.rad.pe.v = 1.0f; e->u.rad.tick.v = 1.0f; break;
    case ENG_KOMBUCHA: e->u.kom.burst.v = 1.0f; e->u.kom.pe.v = 1.0f; break;
    case ENG_LPG: e->u.lpg.env.v = 1.0f; e->u.lpg.ph = PH_QUARTER; break;
    case ENG_BUILDER: e->u.bld.ph = 0; break;
    case ENG_ADDITIVE:
        e->u.add.env.v = 1.0f;
        for (i = 0; i < DM_ADD_N; i++)
            e->u.add.ph[i] = 0;
        break;
    case ENG_CHAOS: e->u.chaos.env.v = 1.0f; break;
    case ENG_VOSIM: e->u.vosim.env.v = 1.0f; e->u.vosim.win = 0; break;
    case ENG_SUMSYN: e->u.sum.env.v = 1.0f; e->u.sum.o1 = e->u.sum.o2 = e->u.sum.o3 = 0; break;
    case ENG_RESON: e->u.reson.burst.v = 1.0f; break;
    case ENG_COMBINATORIC: e->u.comb.pe.v = 1.0f; break;
    case ENG_TERRAINING: e->u.terr.a = e->u.terr.b = 0; break;
    case ENG_SHAME: break;
    case ENG_ENTANGLED: e->u.ent.a = e->u.ent.b = 0; break;
    case ENG_HAYWIRE:
        e->u.hay.pe.v = e->u.hay.re.v = e->u.hay.body.v = 1.0f;
        e->u.hay.o1 = e->u.hay.o2 = PH_QUARTER;
        break;
    case ENG_SCRAPYARD:
        for (i = 0; i < 8u; i++) {
            e->u.scrap.amp[i] = 1.0f;
            e->u.scrap.ph[i] = 0;
        }
        e->u.scrap.zap.v = 1.0f;
        break;
    default: break;
    }
}

void dm_eng_ctrl(dm_eng_t *e, float f, const float *p)
{
    uint32_t i;
    float x;
    /* the same frequency and knobs: the coefficients stand (LPG, ADDITIVE and MODAL read their state here: always) */
    if (f == e->f && p[0] == e->p[0] && p[1] == e->p[1] && p[2] == e->p[2] && p[3] == e->p[3] && e->id != ENG_LPG &&
        e->id != ENG_ADDITIVE && !(e->id == ENG_MODAL && e->vary))
        return;
    e->f = f;
    for (i = 0; i < 4u; i++)
        e->p[i] = p[i];
    switch (e->id) {
    case ENG_KICK:
        e->u.kick.pe.c = dm_decay_coef(0.003f + p[0] * p[0] * 0.25f);
        e->u.kick.k1 = p[1] * 14.0f;
        e->u.kick.inc = f * DM_PH_PER_HZ;
        break;
    case ENG_SNARE: {
        float c = 400.0f * dm_powf(40.0f, p[2]);
        e->u.snare.pe.c = dm_decay_coef(0.002f + p[0] * p[0] * 0.08f);
        e->u.snare.ne.c = dm_decay_coef(0.12f);
        e->u.snare.inc = f * 1.4f * DM_PH_PER_HZ;
        e->u.snare.k = p[1] * 3.0f;
        e->u.snare.rate = dm_lerp(0.03f, 1.0f, p[3]);
        dm_svf_set(&e->u.snare.hp, c * 0.5f, 0.7f);
        dm_svf_set(&e->u.snare.lp, dm_minf(c * 6.0f, DM_FS * 0.45f), 0.7f);
        break;
    }
    case ENG_HAT: {
        float harm = p[3], c = 2000.0f * dm_powf(8.0f, p[2]);
        for (i = 0; i < 6u; i++) {
            float r = dm_lerp(1.0f + (float)i * 0.5f, R808[i], dm_clamp01(harm * 2.0f)) *
                      (1.0f + dm_maxf(0.0f, harm - 0.5f) * (float)i * 0.4f);
            e->u.hat.inc[i] = finc(f * 1.55f * r * DM_PH_PER_HZ);
        }
        e->u.hat.rate = dm_lerp(0.05f, 1.0f, p[1]);
        dm_svf_set(&e->u.hat.bp, c, 2.0f);
        dm_svf_set(&e->u.hat.hp, c * 0.8f, 0.7f);
        break;
    }
    case ENG_CLAP: {
        float c = 600.0f * dm_powf(12.0f, p[2]);
        e->u.clap.claps = 1 + (int32_t)dm_roundf(p[0] * 7.0f);
        e->u.clap.spacing = (int32_t)(DM_FS * (0.002f + p[1] * p[1] * 0.06f));
        e->u.clap.burst.c = dm_decay_coef(0.0035f);
        e->u.clap.tail.c = dm_decay_coef(0.02f + p[3] * p[3] * 1.2f);
        dm_svf_set(&e->u.clap.bp, c, 1.5f);
        e->u.clap.lp.a = dm_onepole_a(c * 2.5f);
        break;
    }
    case ENG_FM: {
        float r = dm_exp2(dm_lerp(-2.0f, 4.0f, p[0]));
        e->u.fm.env.c = dm_decay_coef(0.005f + (1.0f - p[3]) * 0.3f + 0.02f);
        e->u.fm.envk = p[3] * 4.0f;
        e->u.fm.depth0 = p[1] * 6.0f;
        e->u.fm.fb0 = p[2] * 1.4f;
        e->u.fm.cinc = finc(f * DM_PH_PER_HZ);
        e->u.fm.minc = finc(f * r * DM_PH_PER_HZ);
        break;
    }
    case ENG_MODAL: {
        float stretch = dm_lerp(0.6f, 1.6f, p[3]);
        if (e->vary) {
            for (i = 0; i < DM_MODAL_N; i++)
                e->u.modal.amp[i] = 1.0f - p[2] * dm_rng_uni(&e->rng);
            e->vary = 0;
        }
        for (i = 0; i < DM_MODAL_N; i++) {
            float rr = dm_lerp(MEMB[i], BAR[i], p[0]), ratio = 1.0f + (rr - 1.0f) * stretch, hz = f * 2.0f * ratio;
            float dec = 1.6f / (1.0f + (float)i * p[1] * 6.0f);
            e->u.modal.on[i] = hz <= DM_FS * 0.45f;
            e->u.modal.inc[i] = finc(hz * DM_PH_PER_HZ);
            e->u.modal.c[i] = dm_expf(-1.0f / (dec * DM_FS));
            e->u.modal.g[i] = 1.0f / (1.0f + (float)i * 0.6f);
        }
        break;
    }
    case ENG_COWBELL: {
        float ratio = dm_lerp(1.2f, 2.0f, p[3]), base = f * 4.1f;
        e->u.cow.i1 = finc(base * DM_PH_PER_HZ);
        e->u.cow.i2 = finc(base * ratio * DM_PH_PER_HZ);
        e->u.cow.namt = p[2] * 0.8f;
        e->u.cow.camt = p[1] * 3.0f;
        e->u.cow.click.c = dm_decay_coef(0.002f);
        dm_svf_set(&e->u.cow.hp, 100.0f * dm_powf(40.0f, p[0]), 0.7f);
        dm_svf_set(&e->u.cow.bp, base * 1.4f, 1.2f);
        break;
    }
    case ENG_ZAP:
        e->u.zap.pe.c = dm_decay_coef(0.02f + p[0] * 0.25f);
        e->u.zap.k = p[0] * 60.0f;
        e->u.zap.click.c = dm_decay_coef(0.004f);
        e->u.zap.camt = p[1] * 2.0f;
        e->u.zap.mix = p[2] * 0.7f;
        e->u.zap.inc = f * DM_PH_PER_HZ;
        break;
    case ENG_PILLOW: {
        float cut = 60.0f * dm_powf(200.0f, p[0]);
        e->u.pillow.pe.c = dm_decay_coef(0.03f);
        e->u.pillow.click.c = dm_decay_coef(0.0025f);
        e->u.pillow.camt = p[1] * 1.5f;
        e->u.pillow.inc = f * DM_PH_PER_HZ;
        dm_svf_set(&e->u.pillow.lp1, cut, 0.6f);
        dm_svf_set(&e->u.pillow.lp2, cut, 0.6f);
        break;
    }
    case ENG_TRIPWIRE:
        e->u.trip.e1.c = dm_decay_coef(0.002f + p[0] * p[0] * 0.08f);
        e->u.trip.e2.c = dm_decay_coef(0.01f + p[0] * 0.4f);
        e->u.trip.tr.c = dm_decay_coef(0.0015f);
        e->u.trip.drive = 1.0f + p[3] * 20.0f;
        e->u.trip.norm = 1.0f / dm_tanh(e->u.trip.drive);
        e->u.trip.tamt = p[2] * 2.0f;
        e->u.trip.inc = f * DM_PH_PER_HZ;
        break;
    case ENG_RADIATOR:
        e->u.rad.pe.c = dm_decay_coef(0.01f + p[0] * 0.12f);
        e->u.rad.k = p[0] * 12.0f;
        e->u.rad.tick.c = dm_decay_coef(0.003f);
        e->u.rad.tamt = p[1] * 2.5f;
        e->u.rad.lp.a = dm_onepole_a(300.0f);
        e->u.rad.inc = f * DM_PH_PER_HZ;
        break;
    case ENG_KOMBUCHA:
        e->u.kom.burst.c = dm_decay_coef(0.004f);
        e->u.kom.pe.c = dm_decay_coef(0.03f);
        e->u.kom.rate = dm_lerp(0.03f, 1.0f, p[0]);
        e->u.kom.fb = dm_lerp(0.2f, 0.998f, dm_powf(p[1], 0.4f));
        e->u.kom.damp.a = dm_onepole_a(20000.0f * dm_powf(0.02f, p[3]));
        e->u.kom.k = p[2] * 3.0f;
        e->u.kom.f2 = f * 2.0f;
        break;
    case ENG_LPG: {
        float e0 = e->u.lpg.env.v;
        e->u.lpg.env.c = dm_decay_coef(0.08f + p[3] * 0.4f);
        e->u.lpg.fold = 1.0f + p[0] * 6.0f;
        e->u.lpg.veloK = dm_lerp(0.3f, 1.0f, e->vel);
        e->u.lpg.dampK = dm_lerp(0.2f, 1.0f, p[3]);
        e->u.lpg.inc = f * 2.0f * DM_PH_PER_HZ;
        /* the gate's cutoff follows the envelope: once a block (the original set it every sample) */
        dm_svf_set(&e->u.lpg.lp, 80.0f + 16000.0f * e0 * e0 * e->u.lpg.veloK * e->u.lpg.dampK, 0.9f);
        break;
    }
    case ENG_BUILDER:
        e->u.bld.inc = f * DM_PH_PER_HZ;
        if (p[2] < 0.5f) {
            dm_svf_set(&e->u.bld.nf, 200.0f * dm_powf(100.0f, p[2] * 2.0f), 0.7f);
            e->u.bld.hp = 0;
        } else {
            dm_svf_set(&e->u.bld.nf, 50.0f * dm_powf(200.0f, (p[2] - 0.5f) * 2.0f), 0.7f);
            e->u.bld.hp = 1;
        }
        break;
    case ENG_ADDITIVE: {
        float fr = dm_lerp(0.5f, 2.5f, p[0]), ar, a, base, norm = 0.0f;
        int32_t parts = 1 + (int32_t)(p[2] * (float)(DM_ADD_N - 1)), n = 0;
        e->u.add.env.c = dm_decay_coef(0.01f + p[3] * 0.5f);
        ar = dm_clamp01(p[1] + e->u.add.env.v * p[3] * 0.5f);   /* (per sample in the original; per block here) */
        base = dm_lerp(0.2f, 0.98f, ar);
        a = 1.0f;
        for (n = 0; n < parts; n++) {
            float hz = f * (1.0f + (float)n * fr);
            if (hz > DM_FS * 0.45f)
                break;
            e->u.add.inc[n] = finc(hz * DM_PH_PER_HZ);
            e->u.add.a[n] = a;
            norm += a;
            a *= base;
        }
        e->u.add.n = n;
        e->u.add.norm = 1.0f / dm_maxf(1.0f, norm * 0.6f);
        e->noise_k = ar;
        break;
    }
    case ENG_CHAOS:
        e->u.chaos.env.c = dm_decay_coef(0.02f + p[3] * 0.4f);
        e->u.chaos.c0 = p[1] * 4.0f;
        e->u.chaos.ck = p[3] * 6.0f;
        e->u.chaos.r = dm_exp2(dm_lerp(-2.0f, 3.0f, p[0]));
        break;
    case ENG_VOSIM:
        e->u.vosim.env.c = dm_decay_coef(0.01f + p[3] * 0.3f);
        e->u.vosim.iw = finc(f * DM_PH_PER_HZ);
        e->u.vosim.i1 = finc(f * dm_lerp(1.0f, 12.0f, p[0]) * DM_PH_PER_HZ);
        e->u.vosim.i2 = finc(f * dm_lerp(1.0f, 16.0f, p[1]) * DM_PH_PER_HZ);
        break;
    case ENG_SUMSYN: {
        float r = dm_lerp(0.5f, 8.0f, p[0]), a = dm_clampf(dm_lerp(0.2f, 0.92f, p[1]), 0.0f, 0.95f);
        e->u.sum.env.c = dm_decay_coef(0.005f + p[3] * 0.25f);
        e->u.sum.i1 = finc(f * DM_PH_PER_HZ);
        e->u.sum.i2 = finc(f * r * DM_PH_PER_HZ);
        e->u.sum.i3 = finc(dm_sqrtf(r) * f * DM_PH_PER_HZ);
        e->u.sum.a = a;
        e->u.sum.pm0 = p[2] * 3.0f;
        e->u.sum.pmk = p[3] * 8.0f;
        e->u.sum.c1 = (1.0f - a) * 1.2f;
        e->u.sum.aa = 1.0f + a * a;
        e->u.sum.fold = p[1] > 0.6f ? 1.0f + (p[1] - 0.6f) * 6.0f : 0.0f;
        break;
    }
    case ENG_RESON: {
        float ampb = dm_lerp(0.3f, 1.0f, p[0]), amp = 1.0f;
        e->u.reson.burst.c = dm_decay_coef(0.003f);
        for (i = 0; i < 6u; i++) {
            float hz = f * 2.0f * dm_lerp(MEMB[i], BAR[i], p[2]);
            e->u.reson.on[i] = hz <= DM_FS * 0.45f;
            if (e->u.reson.on[i])
                dm_svf_set(&e->u.reson.bank[i], hz, dm_lerp(5.0f, 400.0f, p[3]) / (1.0f + (float)i * p[1] * 3.0f));
            e->u.reson.amp[i] = amp;
            amp *= ampb;
        }
        break;
    }
    case ENG_COMBINATORIC:
        e->u.comb.pe.c = dm_decay_coef(0.04f);
        e->u.comb.inc = finc(f * DM_PH_PER_HZ);
        e->u.comb.ratio = dm_exp2(dm_lerp(-2.0f, 3.0f, p[0]));
        e->u.comb.fb = dm_lerp(0.0f, 0.985f, p[1]);
        e->u.comb.k = p[2] * 4.0f;
        e->u.comb.fm = p[3] * 2.0f;
        break;
    case ENG_TERRAINING:
        e->u.terr.ia = finc(f * DM_PH_PER_HZ);
        e->u.terr.ib = finc(f * dm_exp2(dm_lerp(-2.0f, 3.0f, p[0])) * DM_PH_PER_HZ);
        break;
    case ENG_SHAME:
        e->u.shame.ib = finc(f * dm_exp2(dm_lerp(-2.0f, 4.0f, p[0])) * DM_PH_PER_HZ);
        break;
    case ENG_ENTANGLED:
        e->u.ent.ia = finc(f * DM_PH_PER_HZ);
        e->u.ent.ib = finc(f * dm_exp2(dm_lerp(-2.0f, 4.0f, p[0])) * DM_PH_PER_HZ);
        break;
    case ENG_HAYWIRE: {
        float c = 1500.0f * dm_powf(6.0f, p[2]);
        e->u.hay.pe.c = dm_decay_coef(0.012f);
        e->u.hay.re.c = dm_decay_coef(0.18f);
        e->u.hay.body.c = dm_decay_coef(0.06f);
        dm_svf_set(&e->u.hay.bp1, c, 1.2f);
        dm_svf_set(&e->u.hay.bp2, c * 2.3f, 1.4f);
        e->u.hay.k = p[1] * 4.0f;
        e->u.hay.ta = 0.4f + p[0] * 0.6f;
        e->u.hay.tb = p[0] * 0.8f;
        e->u.hay.ramt = p[3] * 2.5f;
        e->u.hay.i1 = f * 1.5f * DM_PH_PER_HZ;
        e->u.hay.i2 = f * 2.65f * DM_PH_PER_HZ;
        break;
    }
    case ENG_SCRAPYARD: {
        float stretch = dm_lerp(0.7f, 1.8f, p[2]);
        e->u.scrap.zap.c = dm_decay_coef(0.015f);
        e->u.scrap.zk = p[1] * 6.0f;
        e->u.scrap.mamt = p[3] * 2.0f;
        e->u.scrap.minc = finc(f * 2.0f * dm_exp2(dm_lerp(-1.0f, 3.0f, p[0])) * DM_PH_PER_HZ);
        for (i = 0; i < 8u; i++)
            e->u.scrap.base[i] = f * 2.0f * (1.0f + (PLATE[i] - 1.0f) * stretch) * DM_PH_PER_HZ;
        break;
    }
    default: break;
    }
    (void)x;
}

/* SCRAPYARD's partials: the decay per sample of each (constant) and its level */
static float scrap_c[8], scrap_g[8];
static void scrap_tables(void)
{
    uint32_t i;
    if (scrap_g[0] != 0.0f)
        return;
    for (i = 0; i < 8u; i++) {
        scrap_c[i] = dm_expf(-1.0f / ((1.2f / (1.0f + (float)i * 0.7f)) * DM_FS));
        scrap_g[i] = 1.0f / (1.0f + (float)i * 0.5f);
    }
}

float dm_eng_run(dm_eng_t *e)
{
    const float *p = e->p;
    float f = e->f;
    uint32_t i;
    switch (e->id) {
    case ENG_KICK: {
        float ev = dm_denv_next(&e->u.kick.pe), inc = e->u.kick.inc * (1.0f + e->u.kick.k1 * ev * ev), s, shaped, x, w;
        uint32_t ii = finc(inc);
        e->u.kick.ph += ii;
        s = dm_sin_ph(e->u.kick.ph);
        w = p[2];
        if (w < 0.5f) {
            x = dm_ph01(e->u.kick.ph);
            {
                float tri = 4.0f * dm_absf(dm_fracf(x + 0.75f) - 0.5f) - 1.0f;
                shaped = dm_lerp(tri, s, w * 2.0f);
            }
        } else
            shaped = dm_tanh(s * (1.0f + (w - 0.5f) * 14.0f));
        e->u.kick.sub += ii >> 1;
        return shaped * 0.9f + dm_sin_ph(e->u.kick.sub) * p[3] * 0.8f;
    }
    case ENG_SNARE: {
        float ev = dm_denv_next(&e->u.snare.pe), tone;
        e->u.snare.ph += finc(e->u.snare.inc * (1.0f + e->u.snare.k * ev));
        tone = dm_sin_ph(e->u.snare.ph);
        e->u.snare.acc += e->u.snare.rate;
        if (e->u.snare.acc >= 1.0f) {
            e->u.snare.acc -= 1.0f;
            e->u.snare.held = noise(e);
        }
        dm_svf_run(&e->u.snare.hp, e->u.snare.held);
        dm_svf_run(&e->u.snare.lp, e->u.snare.hp.hp);
        return tone * 0.65f + e->u.snare.lp.lp * 0.9f * (0.3f + 0.7f * dm_denv_next(&e->u.snare.ne));
    }
    case ENG_HAT: {
        float sq = 0.0f;
        for (i = 0; i < 6u; i++) {
            e->u.hat.ph[i] += e->u.hat.inc[i];
            sq += (e->u.hat.ph[i] & 0x80000000u) ? -1.0f : 1.0f;
        }
        sq *= (1.0f / 6.0f);
        e->u.hat.acc += e->u.hat.rate;
        if (e->u.hat.acc >= 1.0f) {
            e->u.hat.acc -= 1.0f;
            e->u.hat.held = noise(e);
        }
        dm_svf_run(&e->u.hat.bp, sq);
        dm_svf_run(&e->u.hat.hp, e->u.hat.held);
        return dm_lerp(e->u.hat.bp.bp * 2.5f, e->u.hat.hp.hp, p[0]);
    }
    case ENG_CLAP: {
        float n;
        if (e->u.clap.left > 0 && ++e->u.clap.cnt >= e->u.clap.spacing) {
            e->u.clap.cnt = 0;
            if (e->u.clap.left < e->u.clap.claps) {
                e->u.clap.left++;
                e->u.clap.burst.v = 1.0f;
            } else {
                e->u.clap.left = 0;
                e->u.clap.tail.v = 0.6f;
            }
        }
        n = noise(e);
        dm_svf_run(&e->u.clap.bp, n * (dm_denv_next(&e->u.clap.burst) + dm_denv_next(&e->u.clap.tail)));
        return dm_op_lp(&e->u.clap.lp, e->u.clap.bp.bp) * 3.0f;
    }
    case ENG_FM: {
        float ev = dm_denv_next(&e->u.fm.env) * e->u.fm.envk, depth = e->u.fm.depth0 + ev,
              fb = dm_minf(1.6f, e->u.fm.fb0 + ev * 0.2f), m, out;
        m = tick(&e->u.fm.mod, e->u.fm.minc);
        e->u.fm.car += e->u.fm.cinc;
        out = dm_sin_cyc(dm_ph01(e->u.fm.car) + (depth * m + fb * e->u.fm.last) * (1.0f / DM_TWOPI));
        e->u.fm.last = 0.5f * (e->u.fm.last + out);
        return out * 0.85f;
    }
    case ENG_MODAL: {
        float out = 0.0f;
        for (i = 0; i < DM_MODAL_N; i++) {
            if (!e->u.modal.on[i])
                continue;
            out += tick(&e->u.modal.ph[i], e->u.modal.inc[i]) * e->u.modal.amp[i] * e->u.modal.g[i];
            e->u.modal.amp[i] *= e->u.modal.c[i];
        }
        return out * 0.55f;
    }
    case ENG_COWBELL: {
        float x;
        e->u.cow.p1 += e->u.cow.i1;
        e->u.cow.p2 += e->u.cow.i2;
        x = (((e->u.cow.p1 & 0x80000000u) ? -1.0f : 1.0f) + ((e->u.cow.p2 & 0x80000000u) ? -1.0f : 1.0f)) * 0.5f;
        x += noise(e) * e->u.cow.namt;
        x += noise(e) * dm_denv_next(&e->u.cow.click) * e->u.cow.camt;
        dm_svf_run(&e->u.cow.hp, x);
        dm_svf_run(&e->u.cow.bp, e->u.cow.hp.hp);
        return e->u.cow.bp.bp * 1.6f + e->u.cow.hp.hp * 0.3f;
    }
    case ENG_ZAP: {
        float ev = dm_denv_next(&e->u.zap.pe), inc = e->u.zap.inc * (1.0f + e->u.zap.k * ev * ev), mn, ov, c;
        uint32_t ii = finc(inc);
        e->u.zap.ph += ii;
        mn = dm_sin_ph(e->u.zap.ph);
        e->u.zap.ov += finc(inc * 4.0f);
        ov = dm_sin_ph(e->u.zap.ov);
        if (++e->u.zap.cnt >= 6) {
            e->u.zap.cnt = 0;
            e->u.zap.held = noise(e);
        }
        c = e->u.zap.held * dm_denv_next(&e->u.zap.click) * e->u.zap.camt;
        e->u.zap.sub += ii >> 1;
        return dm_lerp(mn, ov, e->u.zap.mix) + c + dm_sin_ph(e->u.zap.sub) * p[3] * 0.8f;
    }
    case ENG_PILLOW: {
        float ev = dm_denv_next(&e->u.pillow.pe), inc = e->u.pillow.inc * (1.0f + 2.5f * ev * ev), s, pn, x, c;
        uint32_t ii = finc(inc);
        e->u.pillow.ph += ii;
        s = dm_sin_ph(e->u.pillow.ph);
        pn = dm_pink(&e->u.pillow.pink, noise(e));
        x = dm_lerp(s, s * pn * 4.0f, p[2]);
        c = noise(e) * dm_denv_next(&e->u.pillow.click) * e->u.pillow.camt;
        e->u.pillow.sub += ii >> 1;
        dm_svf_run(&e->u.pillow.lp1, x + c + dm_sin_ph(e->u.pillow.sub) * p[3] * 0.8f);
        dm_svf_run(&e->u.pillow.lp2, e->u.pillow.lp1.lp);
        return e->u.pillow.lp2.lp * 1.3f;
    }
    case ENG_TRIPWIRE: {
        float a = dm_denv_next(&e->u.trip.e1), b = dm_denv_next(&e->u.trip.e2), s, sq;
        e->u.trip.ph += finc(e->u.trip.inc * (1.0f + p[1] * (20.0f * a * a + 3.0f * b * b * b)));
        s = dm_sin_ph(e->u.trip.ph);
        sq = dm_tanh(s * e->u.trip.drive) * e->u.trip.norm;
        return dm_lerp(s, sq, p[3]) + noise(e) * dm_denv_next(&e->u.trip.tr) * e->u.trip.tamt;
    }
    case ENG_RADIATOR: {
        float ev = dm_denv_next(&e->u.rad.pe), inc = e->u.rad.inc * (1.0f + e->u.rad.k * ev * ev), s, o, t, r;
        uint32_t ii = finc(inc);
        e->u.rad.ph += ii;
        s = dm_tanh(dm_sin_ph(e->u.rad.ph) * 2.5f);
        e->u.rad.ov += finc(inc * 4.0f);
        o = dm_sin_ph(e->u.rad.ov);
        t = noise(e) * dm_denv_next(&e->u.rad.tick) * e->u.rad.tamt;
        e->u.rad.sub += ii >> 1;
        r = dm_sin_ph(e->u.rad.sub) * (0.4f + dm_op_lp(&e->u.rad.lp, noise(e)) * 6.0f) * p[3];
        return dm_tanh((dm_lerp(s, o, p[2]) + t + r) * 1.4f);
    }
    case ENG_KOMBUCHA: {
        float ex, hz, y;
        e->u.kom.acc += e->u.kom.rate;
        if (e->u.kom.acc >= 1.0f) {
            e->u.kom.acc -= 1.0f;
            e->u.kom.held = noise(e);
        }
        ex = e->u.kom.held * dm_denv_next(&e->u.kom.burst);
        hz = e->u.kom.f2 * (1.0f + e->u.kom.k * dm_denv_next(&e->u.kom.pe));
        if (!e->dl.b)
            return ex;
        y = ex + e->u.kom.fb * dm_op_lp(&e->u.kom.damp, dm_dl_read(&e->dl, DM_FS / dm_maxf(20.0f, hz)));
        dm_dl_push(&e->dl, dm_tanh(y));
        return y * 0.9f;
    }
    case ENG_LPG: {
        float ev = dm_denv_next(&e->u.lpg.env), v;
        v = tick(&e->u.lpg.ph, finc(e->u.lpg.inc)) * e->u.lpg.fold;
        v = dm_fold(v, 4);
        v = dm_lerp(v, dm_tanh(v * 4.0f), p[1]);
        v = dm_lerp(v, noise(e), p[2]);
        dm_svf_run(&e->u.lpg.lp, v);
        return e->u.lpg.lp.lp * (0.3f + 0.7f * ev) * 1.2f;
    }
    case ENG_BUILDER: {
        float x, sine, tri, saw, sq, w = p[0] * 3.0f, osc, n;
        e->u.bld.ph += finc(e->u.bld.inc);
        x = dm_ph01(e->u.bld.ph);
        if (w < 1.0f) {
            sine = dm_sin_ph(e->u.bld.ph);
            tri = 2.0f * dm_absf(2.0f * x - 1.0f) - 1.0f;
            osc = dm_lerp(sine, tri, w);
        } else if (w < 2.0f) {
            tri = 2.0f * dm_absf(2.0f * x - 1.0f) - 1.0f;
            saw = 2.0f * x - 1.0f;
            osc = dm_lerp(tri, saw, w - 1.0f);
        } else {
            saw = 2.0f * x - 1.0f;
            sq = x < 0.5f ? 1.0f : -1.0f;
            osc = dm_lerp(saw, sq, w - 2.0f);
        }
        n = noise(e);
        dm_svf_run(&e->u.bld.nf, n);
        return osc * p[1] + (e->u.bld.hp ? e->u.bld.nf.hp : e->u.bld.nf.lp) * p[3];
    }
    case ENG_ADDITIVE: {
        float out = 0.0f;
        int32_t n;
        (void)dm_denv_next(&e->u.add.env);
        for (n = 0; n < e->u.add.n; n++)
            out += tick(&e->u.add.ph[n], e->u.add.inc[n]) * e->u.add.a[n];
        out *= e->u.add.norm;
        return e->noise_k > 0.85f ? dm_tanh(out * (1.0f + (e->noise_k - 0.85f) * 20.0f)) : out;
    }
    case ENG_CHAOS: {
        float chaos = e->u.chaos.c0 + dm_denv_next(&e->u.chaos.env) * e->u.chaos.ck, cut;
        e->u.chaos.a += finc(dm_maxf(1.0f, f * (1.0f + chaos * e->u.chaos.lastB)) * DM_PH_PER_HZ);
        e->u.chaos.b += finc(dm_maxf(1.0f, f * e->u.chaos.r * (1.0f + chaos * e->u.chaos.lastA)) * DM_PH_PER_HZ);
        e->u.chaos.lastA = dm_sin_ph(e->u.chaos.a);
        e->u.chaos.lastB = ((e->u.chaos.b & 0x80000000u) ? -0.7f : 0.7f) + dm_sin_ph(e->u.chaos.b) * 0.3f;
        cut = dm_clampf(f * 4.0f * dm_exp2((e->u.chaos.lastA + e->u.chaos.lastB) * p[2] * 4.0f), 40.0f, DM_FS * 0.45f);
        dm_svf_set(&e->u.chaos.filt, cut, 2.5f);
        dm_svf_run(&e->u.chaos.filt, e->u.chaos.lastA * 0.6f + e->u.chaos.lastB * 0.4f);
        return dm_tanh(e->u.chaos.filt.lp * 1.5f);
    }
    case ENG_VOSIM: {
        float w, fm, s2, s1, win;
        e->u.vosim.win += e->u.vosim.iw;
        if (e->u.vosim.win < e->u.vosim.iw) {           /* the window wrapped: the pulse starts again */
            e->u.vosim.s1 = 0;
            e->u.vosim.s2 = 0;
        }
        w = dm_ph01(e->u.vosim.win);
        e->u.vosim.s2 += e->u.vosim.i2;
        e->u.vosim.s1 += e->u.vosim.i1;
        fm = (p[2] + dm_denv_next(&e->u.vosim.env) * p[3]) * 3.0f;
        s2 = dm_sin_ph(e->u.vosim.s2);
        s1 = dm_sin_cyc(dm_ph01(e->u.vosim.s1) + fm * s2 * (1.0f / DM_TWOPI));
        win = 1.0f - w;
        return (s1 * 0.6f + s2 * 0.4f) * win * win * 1.6f;
    }
    case ENG_SUMSYN: {
        float x3 = tick(&e->u.sum.o3, e->u.sum.i3), pm, thc, bec, num, den, y, a = e->u.sum.a;
        pm = (e->u.sum.pm0 + dm_denv_next(&e->u.sum.env) * e->u.sum.pmk) * x3;
        e->u.sum.o1 += e->u.sum.i1;
        e->u.sum.o2 += e->u.sum.i2;
        thc = dm_ph01(e->u.sum.o1) + pm * (1.0f / DM_TWOPI);
        bec = dm_ph01(e->u.sum.o2);
        num = dm_sin_cyc(thc) - a * dm_sin_cyc(thc - bec);
        den = e->u.sum.aa - 2.0f * a * dm_sin_ph(e->u.sum.o2 + 0x40000000u);
        y = num / dm_maxf(0.05f, den) * e->u.sum.c1;
        if (e->u.sum.fold > 0.0f)
            y = dm_fold(y * e->u.sum.fold, 3);
        return y;
    }
    case ENG_RESON: {
        float ex = noise(e) * dm_denv_next(&e->u.reson.burst), out = 0.0f;
        for (i = 0; i < 6u; i++) {
            if (!e->u.reson.on[i])
                continue;
            dm_svf_run(&e->u.reson.bank[i], ex);
            out += e->u.reson.bank[i].bp * e->u.reson.amp[i];
        }
        return dm_tanh(out * 2.0f);
    }
    case ENG_COMBINATORIC: {
        float x = tick(&e->u.comb.osc, e->u.comb.inc), cf, y;
        cf = f * e->u.comb.ratio * (1.0f + e->u.comb.k * dm_denv_next(&e->u.comb.pe)) * (1.0f + x * e->u.comb.fm);
        cf = dm_clampf(cf, 15.0f, DM_FS * 0.45f);
        if (!e->dl.b)
            return dm_tanh(x * 0.6f);
        y = x + e->u.comb.fb * dm_dl_read(&e->dl, DM_FS / cf);
        dm_dl_push(&e->dl, dm_tanh(y));
        return dm_tanh(y * 0.6f);
    }
    case ENG_TERRAINING: {
        float x, y, t = p[3];
        e->u.terr.a += e->u.terr.ia;
        e->u.terr.b += e->u.terr.ib;
        x = morph_wave(e->u.terr.a, p[1]);
        y = morph_wave(e->u.terr.b, p[2]);
        if (t < 0.5f) {
            float k = t * 6.0f;
            if (k < 1.0f)
                return dm_lerp((x + y) * 0.5f, x * y, k);
            if (k < 2.0f)
                return dm_lerp(x * y, dm_sin_cyc(x * y), k - 1.0f);
            return dm_lerp(dm_sin_cyc(x * y), dm_cos_cyc((x + y) * 0.5f) * x, k - 2.0f);
        }
        {
            int32_t qx = (int32_t)((x * 0.5f + 0.5f) * 255.0f), qy = (int32_t)((y * 0.5f + 0.5f) * 255.0f), v;
            float k = (t - 0.5f) * 2.0f;
            v = k < 0.33f ? (qx ^ qy) : k < 0.66f ? ((qx & qy) | (qx >> 2)) : ((qx | qy) - (qx & qy) * 2);
            return ((float)(v & 255) / 127.5f - 1.0f) * 0.8f;
        }
    }
    case ENG_SHAME: {
        uint32_t pa0 = e->u.shame.a, pb0 = e->u.shame.b;
        float y, x, sh = p[3] * 4.0f, pbf;
        int trigA, trigB;
        e->u.shame.b += e->u.shame.ib;
        pbf = dm_ph01(e->u.shame.b);
        y = pbf < 0.5f ? 1.0f : -1.0f;
        e->u.shame.a += finc(dm_maxf(1.0f, f * (1.0f + y * p[1] * 3.0f) + e->u.shame.last * p[2] * f * 4.0f) * DM_PH_PER_HZ);
        x = 2.0f * dm_ph01(e->u.shame.a) - 1.0f;
        e->u.shame.last = x;
        trigA = e->u.shame.a < pa0;
        trigB = e->u.shame.b < pb0;
        if (sh < 1.0f) {
            if (trigB)
                e->u.shame.held = x;
            return dm_lerp(x, e->u.shame.held, sh);
        }
        if (sh < 2.0f) {
            if (trigA)
                e->u.shame.held = y * pbf;
            return dm_lerp(e->u.shame.held, x * e->u.shame.held, sh - 1.0f);
        }
        if (sh < 3.0f) {
            if (trigA || trigB)
                e->u.shame.held = (x + y) * 0.5f;
            return e->u.shame.held;
        }
        if (trigB)
            e->u.shame.held = dm_roundf(x * 4.0f) * 0.25f;
        return dm_lerp(e->u.shame.held, e->u.shame.held * y, sh - 3.0f);
    }
    case ENG_ENTANGLED: {
        float y = tick(&e->u.ent.b, e->u.ent.ib), x, t = p[3] * 4.0f, sum, ring, win, terr;
        e->u.ent.a += e->u.ent.ia;
        x = dm_sin_cyc(dm_ph01(e->u.ent.a) + (y * p[1] * 4.0f + e->u.ent.last * p[2] * 2.5f) * (1.0f / DM_TWOPI));
        e->u.ent.last = x;
        sum = (x + y) * 0.5f;
        ring = x * y;
        if (t < 1.0f)
            return dm_lerp(sum, ring, t);
        win = x * (0.5f - 0.5f * dm_sin_ph(e->u.ent.b + 0x40000000u));
        if (t < 2.0f)
            return dm_lerp(ring, win, t - 1.0f);
        terr = dm_sin_cyc((x * x - y * y) * 0.5f);
        if (t < 3.0f)
            return dm_lerp(win, terr, t - 2.0f);
        return dm_lerp(terr, terr * ring * 2.0f, t - 3.0f);
    }
    case ENG_HAYWIRE: {
        float ev = dm_denv_next(&e->u.hay.pe), m = 1.0f + e->u.hay.k * ev * ev, a, b, bo, tone, rattle;
        a = tick(&e->u.hay.o1, finc(e->u.hay.i1 * m));
        b = tick(&e->u.hay.o2, finc(e->u.hay.i2 * m));
        bo = dm_denv_next(&e->u.hay.body);
        tone = (a * e->u.hay.ta + b * e->u.hay.tb * bo) * (0.6f + 0.4f * bo);
        dm_svf_run(&e->u.hay.bp1, noise(e));
        dm_svf_run(&e->u.hay.bp2, noise(e));
        rattle = (e->u.hay.bp1.bp + e->u.hay.bp2.bp * 0.7f) * dm_denv_next(&e->u.hay.re) * e->u.hay.ramt;
        return dm_tanh((tone + rattle) * 1.2f);
    }
    case ENG_SCRAPYARD: {
        float z = 1.0f + e->u.scrap.zk * dm_denv_next(&e->u.scrap.zap), m, mul, out = 0.0f;
        scrap_tables();
        m = tick(&e->u.scrap.mod, e->u.scrap.minc) * e->u.scrap.mamt;
        mul = z * (1.0f + m * 0.5f);
        for (i = 0; i < 8u; i++) {
            float inc = e->u.scrap.base[i] * mul;
            if (!(inc >= 0.0f && inc <= DM_FS * 0.45f * DM_PH_PER_HZ))
                continue;
            out += tick(&e->u.scrap.ph[i], (uint32_t)inc) * e->u.scrap.amp[i] * scrap_g[i];
            e->u.scrap.amp[i] *= scrap_c[i];
        }
        return dm_tanh(out * 0.6f);
    }
    default: return 0.0f;
    }
}
