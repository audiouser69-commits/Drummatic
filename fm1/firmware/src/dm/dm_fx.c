/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — send effects and master (see dm_fx.h). */
#include "dm_fx.h"

static float onepole_a2(float hz) { return dm_expf(-DM_TWOPI * dm_clampf(hz, 5.0f, DM_FS2 * 0.49f) / DM_FS2); }

void dm_tilt_set(dm_tilt_t *t, float tilt)
{
    float v = dm_clampf(tilt, -1.0f, 1.0f);
    t->glo = (1.0f - dm_maxf(0.0f, v)) * (1.0f + dm_maxf(0.0f, -v) * 0.5f);
    t->ghi = (1.0f + dm_minf(0.0f, v)) * (1.0f + dm_maxf(0.0f, v) * 0.5f);
}

/* ------------------------------------------------------- Shatter Delay --- */
void dm_delay_init(dm_delay_t *d, int16_t *buf_l, int16_t *buf_r)
{
    uint8_t *b = (uint8_t *)d;
    uint32_t i;
    for (i = 0; i < sizeof *d; i++)
        b[i] = 0;
    dm_dl_init(&d->dl[0], buf_l, DM_DLY_N, 1.0f);
    dm_dl_init(&d->dl[1], buf_r, DM_DLY_N, 1.0f);
    d->cur[0] = d->cur[1] = d->prev[0] = d->prev[1] = 0.25f;
    d->xf[0] = d->xf[1] = 1.0f;
    d->rng.s = 0xAB1Eu;
    d->tl[0].lp.a = d->tl[1].lp.a = onepole_a2(800.0f);
    dm_tilt_set(&d->tl[0], 0.0f);
    dm_tilt_set(&d->tl[1], 0.0f);
    d->rate = 1.0f;
}

void dm_delay_set(dm_delay_t *d, float time, float fb, float glitch, float fbtilt, float ducking, float srate, float gain,
                  int stereo, int pong)
{
    d->time = time;
    d->fb = dm_minf(0.98f, fb * 1.02f);
    d->glitch = glitch;
    d->ducking = ducking;
    d->rate = dm_lerp(0.08f, 1.0f, dm_clamp01(srate));
    d->gain = gain;
    d->stereo = (uint8_t)(stereo != 0);
    d->pong = (uint8_t)(pong != 0);
    dm_tilt_set(&d->tl[0], fbtilt);
    dm_tilt_set(&d->tl[1], fbtilt);
}

/* silence: the input below -100 dB and the output below -90 dB for longer than the longest echo (the int16 line
 * rounds what is left to 0 anyway) - the delay sleeps until something is sent to it */
#define FX_IN_EPS 1.0e-5f
#define FX_OUT_EPS 3.0e-5f
static int quiet_in(const float *a, const float *b, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] > FX_IN_EPS || a[i] < -FX_IN_EPS || b[i] > FX_IN_EPS || b[i] < -FX_IN_EPS)
            return 0;
    return 1;
}
static int quiet_out(const float *a, const float *b, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (a[i] > FX_OUT_EPS || a[i] < -FX_OUT_EPS || b[i] > FX_OUT_EPS || b[i] < -FX_OUT_EPS)
            return 0;
    return 1;
}

void dm_delay_run(dm_delay_t *d, const float *inl, const float *inr, float *outl, float *outr, uint32_t n)
{
    uint32_t i, c, k;
    float tmax = ((float)DM_DLY_N - 4.0f) / DM_FS2;
    int qin = quiet_in(inl, inr, n);
    if (qin && d->quiet > 2u * DM_DLY_N + 4096u) {      /* asleep */
        for (i = 0; i < n; i++)
            outl[i] = outr[i] = 0.0f;
        return;
    }
    for (i = 0; i + 1u < n; i += 2u) {
        float in[2], wet[2], target, lvl, duck, fl, fr, y0, y1;
        int tick;
        in[0] = (inl[i] + inl[i + 1u]) * 0.5f;
        in[1] = (inr[i] + inr[i + 1u]) * 0.5f;
        d->sracc += d->rate * 2.0f;                 /* (two samples of the original per step) */
        tick = d->sracc >= 1.0f;
        while (d->sracc >= 1.0f)
            d->sracc -= 1.0f;
        for (c = 0; c < 2u; c++) {
            if (c == 0u || d->stereo)
                for (k = 0; k < 2u; k++) {
                    d->gvel[c] = d->gvel[c] * 0.9995f + dm_rng_bi(&d->rng) * 0.00008f;
                    d->gst[c] = dm_clampf(d->gst[c] + d->gvel[c], -1.0f, 1.0f);
                }
            else
                d->gst[1] = d->gst[0];
            target = dm_clampf(d->time * (1.0f + d->glitch * d->glitch * d->gst[c] * 0.9f) / d->rate, 0.001f, tmax);
            if (dm_absf(target - d->cur[c]) > 0.002f && d->xf[c] >= 1.0f) {
                d->prev[c] = d->cur[c];
                d->cur[c] = target;
                d->xf[c] = 0.0f;
            }
            if (d->xf[c] < 1.0f) {
                d->xf[c] = dm_minf(1.0f, d->xf[c] + 1.0f / (0.03f * DM_FS2));
                wet[c] = dm_dl_read(&d->dl[c], d->prev[c] * DM_FS2) * (1.0f - d->xf[c]) +
                         dm_dl_read(&d->dl[c], d->cur[c] * DM_FS2) * d->xf[c];
            } else
                wet[c] = dm_dl_read(&d->dl[c], d->cur[c] * DM_FS2);
        }
        lvl = dm_maxf(dm_maxf(dm_absf(inl[i]), dm_absf(inr[i])), dm_maxf(dm_absf(inl[i + 1u]), dm_absf(inr[i + 1u])));
        d->duck = lvl > d->duck ? lvl : d->duck * 0.99940f;
        duck = d->ducking > 0.0f ? 1.0f / (1.0f + d->ducking * d->duck * 8.0f) : 1.0f;
        if (tick) {
            d->held[0] = in[0];
            d->held[1] = in[1];
        }
        fl = dm_tilt_run(&d->tl[0], d->pong ? wet[1] : wet[0]);
        fr = dm_tilt_run(&d->tl[1], d->pong ? wet[0] : wet[1]);
        dm_dl_push(&d->dl[0], dm_tanh(d->held[0] + fl * d->fb));
        dm_dl_push(&d->dl[1], dm_tanh(d->held[1] + fr * d->fb));
        y0 = wet[0] * duck * d->gain;
        y1 = wet[1] * duck * d->gain;
        outl[i] = (d->yprev[0] + y0) * 0.5f;
        outr[i] = (d->yprev[1] + y1) * 0.5f;
        outl[i + 1u] = y0;
        outr[i + 1u] = y1;
        d->yprev[0] = y0;
        d->yprev[1] = y1;
    }
    if (qin && quiet_out(outl, outr, n))
        d->quiet += n;
    else
        d->quiet = 0;
}

/* ---------------------------------------------------- Headspace Reverb --- */
static const float REV_BASE[8] = {0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0503f, 0.0571f, 0.0613f, 0.0683f};

uint32_t dm_reverb_lines_len(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < 8u; i++)
        n += (uint32_t)(DM_FS2 * REV_BASE[i] * DM_REV_K) + 16u;
    return n;
}

void dm_reverb_init(dm_reverb_t *r, int16_t *pre, int16_t *lines)
{
    uint8_t *b = (uint8_t *)r;
    uint32_t i;
    for (i = 0; i < sizeof *r; i++)
        b[i] = 0;
    dm_dl_init(&r->pre, pre, DM_PRE_N, 4.0f);
    for (i = 0; i < 8u; i++) {
        uint32_t n = (uint32_t)(DM_FS2 * REV_BASE[i] * DM_REV_K) + 16u;
        r->lens[i] = REV_BASE[i];
        dm_dl_init(&r->line[i], lines, n, 1.0f);
        lines += n;
    }
    r->in.lp.a = r->outl.lp.a = r->outr.lp.a = onepole_a2(800.0f);
    dm_tilt_set(&r->in, 0.0f);
    dm_tilt_set(&r->outl, 0.0f);
    dm_tilt_set(&r->outr, 0.0f);
    r->rate = 1.0f;
    r->size = 1.0f;
}

void dm_reverb_set(dm_reverb_t *r, float predelay, float decay, float srate, float intilt, float outtilt, float gain, int mode)
{
    static const float DAMP_HZ[4] = {3500.0f, 12000.0f, 6000.0f, 9000.0f};
    uint32_t i;
    float t60 = dm_maxf(0.05f, decay), a;
    mode &= 3;
    r->rate = dm_lerp(0.1f, 1.0f, dm_clamp01(srate));
    r->size = (mode == 3 ? 0.25f : mode == 2 ? 1.6f : 1.0f) / r->rate;
    r->predelay = predelay;
    r->gain = gain;
    a = onepole_a2(DAMP_HZ[mode]);
    for (i = 0; i < 8u; i++) {
        float len = dm_minf(r->lens[i] * r->size, ((float)r->line[i].n - 4.0f) / DM_FS2);
        r->damp[i].a = a;
        r->g[i] = dm_exp2(-3.0f * 3.32192809f * len / t60);    /* 10^(-3 len / t60) */
    }
    dm_tilt_set(&r->in, intilt);
    dm_tilt_set(&r->outl, outtilt);
    dm_tilt_set(&r->outr, outtilt);
}

void dm_reverb_run(dm_reverb_t *r, const float *inl, const float *inr, float *outl, float *outr, uint32_t n)
{
    uint32_t i, k;
    float dd[8], lim[8], pre = dm_maxf(1.0f, r->predelay * DM_FS2);
    int qin = quiet_in(inl, inr, n);
    if (qin && r->quiet > (uint32_t)DM_FS * 2u + 2u * DM_PRE_N) {   /* asleep: nothing in, the tail gone */
        for (i = 0; i < n; i++)
            outl[i] = outr[i] = 0.0f;
        return;
    }
    /* the lines' slow modulation (0.4 Hz): once a block */
    for (k = 0; k < 8u; k++) {
        float mod = 1.0f + 0.003f * dm_sin_cyc(r->lfoph + (float)k * 0.125f);
        lim[k] = (float)r->line[k].n - 4.0f;
        dd[k] = dm_clampf(r->lens[k] * r->size * mod * DM_FS2, 4.0f, lim[k]);
    }
    r->lfoph += 0.4f * (float)(n / 2u) / DM_FS2;
    if (r->lfoph > 1.0f)
        r->lfoph -= 1.0f;
    for (i = 0; i + 1u < n; i += 2u) {
        float in = dm_tilt_run(&r->in, (inl[i] + inr[i] + inl[i + 1u] + inr[i + 1u]) * 0.25f), x, o[8], sum, l, rr, y0, y1;
        r->sracc += r->rate * 2.0f;
        if (r->sracc >= 1.0f) {
            while (r->sracc >= 1.0f)
                r->sracc -= 1.0f;
            r->held = in;
        }
        dm_dl_push(&r->pre, r->held);
        x = dm_dl_read(&r->pre, pre) * 0.35f;
        for (k = 0; k < 8u; k++)
            o[k] = dm_op_lp(&r->damp[k], dm_dl_read(&r->line[k], dd[k]));
        sum = (o[0] + o[1] + o[2] + o[3] + o[4] + o[5] + o[6] + o[7]) * 0.25f;
        for (k = 0; k < 8u; k++)
            dm_dl_push(&r->line[k], dm_tanh((o[k] - sum) * r->g[k] + x));
        l = (o[0] + o[2] + o[4] + o[6]) * 0.5f;
        rr = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
        y0 = dm_tilt_run(&r->outl, l) * r->gain;
        y1 = dm_tilt_run(&r->outr, rr) * r->gain;
        outl[i] = (r->yprev[0] + y0) * 0.5f;
        outr[i] = (r->yprev[1] + y1) * 0.5f;
        outl[i + 1u] = y0;
        outr[i + 1u] = y1;
        r->yprev[0] = y0;
        r->yprev[1] = y1;
    }
    if (qin && quiet_out(outl, outr, n))
        r->quiet += n;
    else
        r->quiet = 0;
}

/* -------------------------------------------------------------- master --- */
void dm_master_init(dm_master_t *m)
{
    uint8_t *b = (uint8_t *)m;
    uint32_t i;
    for (i = 0; i < sizeof *m; i++)
        b[i] = 0;
    m->gain = 1.0f;
    m->clip = 1;
}

void dm_master_set(dm_master_t *m, float maximize, float low, float mid, float high, float gaindb, int clip)
{
    static const float HZ[3] = {150.0f, 1000.0f, 6000.0f};
    float db[3];
    uint32_t b;
    db[0] = low;
    db[1] = mid;
    db[2] = high;
    m->maximize = maximize;
    if (maximize > 0.001f) {
        m->att = dm_expf(-1.0f / (0.002f * DM_FS));
        m->rel = dm_expf(-1.0f / (dm_lerp(0.15f, 0.05f, maximize) * DM_FS));
        m->thr = dm_db2gain(-30.0f * maximize);
        m->mk = dm_db2gain(24.0f * maximize * maximize + 6.0f * maximize);
    }
    for (b = 0; b < 3u; b++) {
        if (dm_absf(db[b]) < 0.05f) {
            m->eqg[b] = 0.0f;
            continue;
        }
        m->eqg[b] = dm_db2gain(db[b]) - 1.0f;
        dm_svf_set(&m->eq[b][0], HZ[b], 0.7f);
        m->eq[b][1].k = m->eq[b][0].k;
        m->eq[b][1].a1 = m->eq[b][0].a1;
        m->eq[b][1].a2 = m->eq[b][0].a2;
        m->eq[b][1].a3 = m->eq[b][0].a3;
    }
    m->gain = dm_db2gain(gaindb);
    m->clip = (uint8_t)(clip & 3);
}

static inline float clipf(float x, uint32_t clip)
{
    int i;
    switch (clip) {
    case 1: return dm_tanh(x);
    case 2: return dm_clampf(x, -1.0f, 1.0f);
    case 3:
        for (i = 0; i < 6 && (x > 1.0f || x < -1.0f); i++)
            x = x > 1.0f ? 2.0f - x : -2.0f - x;
        return x;
    default: return x;
    }
}

void dm_master_run(dm_master_t *m, float *l, float *r, uint32_t n)
{
    uint32_t i, b;
    for (i = 0; i < n; i++) {
        float L = l[i], R = r[i];
        if (m->maximize > 0.001f) {
            float lvl = dm_maxf(dm_absf(L), dm_absf(R)), c = lvl > m->env ? m->att : m->rel, g;
            m->env = lvl + c * (m->env - lvl);
            g = m->env > m->thr ? dm_powf(m->thr / m->env, 0.8f) : 1.0f;
            g *= m->mk;
            L *= g;
            R *= g;
        }
        for (b = 0; b < 3u; b++) {
            if (m->eqg[b] == 0.0f)
                continue;
            dm_svf_run(&m->eq[b][0], L);
            dm_svf_run(&m->eq[b][1], R);
            L += m->eqg[b] * (b == 0u ? m->eq[b][0].lp : b == 1u ? m->eq[b][0].bp : m->eq[b][0].hp);
            R += m->eqg[b] * (b == 0u ? m->eq[b][1].lp : b == 1u ? m->eq[b][1].bp : m->eq[b][1].hp);
        }
        L = clipf(L * m->gain, m->clip);
        R = clipf(R * m->gain, m->clip);
        l[i] = dm_finite(L) ? L : 0.0f;
        r[i] = dm_finite(R) ? R : 0.0f;
    }
    for (b = 0; b < 3u; b++) {
        dm_svf_guard(&m->eq[b][0]);
        dm_svf_guard(&m->eq[b][1]);
    }
}
