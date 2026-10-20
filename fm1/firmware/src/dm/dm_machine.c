/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the machine (see dm_machine.h). Song time is a beat count in 32.32 fixed point; the sequencer
 * (Sequencer::collect of the original) looks for the hits of each voice in the window of the half buffer and gives
 * them their sample offset; the voices are rendered in segments between the hits, as DrumMachine::process does. */
#include "dm_machine.h"

#ifndef DM_POOL
#define DM_POOL __attribute__((section(".pool")))
#endif

dm_machine_t dm_m;
static dm_voice_t voice[DM_NV];
static int16_t arena[DM_NV][DM_ARENA] DM_POOL;
static int16_t dly_buf[2][DM_DLY_N] DM_POOL;
static int16_t rev_pre[DM_PRE_N] DM_POOL;
#define REV_LINES 21600u                 /* >= dm_reverb_lines_len() (checked at init) */
static int16_t rev_lines[REV_LINES] DM_POOL;
static dm_delay_t delay;
static dm_reverb_t reverb;
static dm_master_t master;
static dm_vset_t vs[DM_NV];
static dm_seqset_t ss[DM_NV];

#define Q32 4294967296.0f
typedef int64_t beat_t;                  /* beats in 32.32 */

typedef struct { beat_t beat; dm_trig_t ti; } pend_t;
#define NPEND 16u
typedef struct {
    int32_t lane_cnt[DM_NLANE];
    int64_t last_step;
    pend_t pend[NPEND];
    uint32_t npend;
    uint8_t hits[64];
    uint32_t hsig;
} seqrt_t;
static seqrt_t srt[DM_NV];

typedef struct { uint8_t off, voice, choke; dm_trig_t ti; } ev_t;
#define NEV 64u
static ev_t evs[NEV];
static uint32_t nev;

static beat_t song_beat;
static uint8_t was_playing;
static uint32_t seed = 0x1234u;
/* the external clock */
static uint32_t clk_n;                   /* clocks since START */
static uint64_t sample_now;              /* samples rendered */
static uint64_t clk_t[25];               /* the sample time of the last 25 clocks (a ring) */
static uint32_t clk_seen, clk_quiet;     /* clocks counted; halves without a clock */
static float clk_bpm = 120.0f;
static uint8_t ext_run;                  /* START / CONTINUE seen, no STOP since */
static beat_t clk_err;                   /* where the clock says we are minus where we are (pulled in over the halves) */

static float buf[10][DM_HALF];           /* out L R, delay send L R, reverb send L R, delay out L R, reverb out L R */
static float sink[6][DM_HALF];           /* a muted voice renders here */

void dm_machine_init(void)
{
    uint32_t v;
    uint8_t *mb = (uint8_t *)&dm_m;
    for (v = 0; v < sizeof dm_m; v++)
        mb[v] = 0;
    song_beat = 0;
    was_playing = 0;
    seed = 0x1234u;
    clk_n = clk_seen = clk_quiet = 0;
    clk_err = 0;
    clk_bpm = 120.0f;
    ext_run = 0;
    sample_now = 0;
    nev = 0;
    dm_math_init();
    for (v = 0; v < DM_NV; v++) {
        dm_voice_init(&voice[v], v, arena[v]);
        dm_m.play_step[v] = -1;
        srt[v].last_step = -1;
        srt[v].hsig = 0xFFFFFFFFu;
    }
    dm_delay_init(&delay, dly_buf[0], dly_buf[1]);
    if (dm_reverb_lines_len() > REV_LINES)
        for (;;)
            ;                            /* (a build error in effect: REV_LINES too small) */
    dm_reverb_init(&reverb, rev_pre, rev_lines);
    dm_master_init(&master);
    dm_m.audition = -1;
    dm_m.bpm_now = 120.0f;
}

/* ------------------------------------------------------------ pattern --- */
static void pattern_cache(seqrt_t *r, const dm_seqset_t *s)
{
    uint32_t sig = (uint32_t)s->steps | (uint32_t)s->fill << 8 | (uint32_t)s->pattern << 16, i, pass;
    int steps = s->steps, fill = s->fill;
    if (sig == r->hsig)
        return;
    r->hsig = sig;
    if (fill > steps)
        fill = steps;
    for (i = 0; i < (uint32_t)steps; i++)
        r->hits[i] = (uint8_t)(((int)i * fill) % steps < fill);
    if (s->pattern > 1 && fill > 0 && fill < steps)
        for (pass = 0; pass < 2u; pass++)            /* a deterministic permutation: hits moved one by one */
            for (i = 0; i < (uint32_t)steps; i++)
                if (r->hits[i]) {
                    float h = dm_hash_uni((uint32_t)s->pattern, i, pass);
                    int dir = h < 0.33f ? -1 : h < 0.66f ? 0 : 1 + (int)(h * 3.0f) % 2, j;
                    j = (((int)i + dir) % steps + steps) % steps;
                    if (dir != 0 && !r->hits[j]) {
                        r->hits[i] = 0;
                        r->hits[j] = 1;
                    }
                }
}
static int pattern_hit(const seqrt_t *r, const dm_seqset_t *s, int idx)
{
    int k = ((idx - s->rotate) % s->steps + s->steps) % s->steps;
    return s->invert ? !r->hits[k] : r->hits[k];
}
uint32_t dm_machine_hits(uint32_t v, uint8_t *hits, uint32_t n)   /* returns the steps playing (Performance too) */
{
    dm_vset_t a;
    dm_seqset_t q;
    static seqrt_t r;                     /* (main loop only; not on the stack) */
    uint32_t i;
    dm_effective(&dm_kit, v % DM_NV, &a, &q);
    r.hsig = 0xFFFFFFFFu;
    pattern_cache(&r, &q);
    for (i = 0; i < n; i++)
        hits[i] = (uint8_t)(i < (uint32_t)q.steps ? pattern_hit(&r, &q, (int)i) : 0);
    return (uint32_t)q.steps;
}

/* ---------------------------------------------------------- sequencer --- */
#ifdef DM_TEST_HOOK
void (*dm_test_event)(uint64_t sample, uint32_t v, const dm_trig_t *ti);
#endif
static void add_event(uint32_t off, uint32_t v, const dm_trig_t *ti, int choke)
{
    uint32_t i;
#ifdef DM_TEST_HOOK
    if (dm_test_event && ti)
        dm_test_event(sample_now + off, v, ti);
#endif
    if (nev >= NEV)
        return;
    i = nev++;
    while (i > 0u && evs[i - 1u].off > off) {        /* sorted by offset, stable */
        evs[i] = evs[i - 1u];
        i--;
    }
    evs[i].off = (uint8_t)(off < DM_HALF ? off : DM_HALF - 1u);
    evs[i].voice = (uint8_t)v;
    evs[i].choke = (uint8_t)choke;
    if (ti)
        evs[i].ti = *ti;
}
static void trig_default(dm_trig_t *t, float vel)
{
    t->vel = vel;
    t->pitch = 0.0f;
    t->envscale = 1.0f;
    t->pan = t->var = t->sdly = t->srev = 0.0f;
    t->macro2 = -1.0f;
    t->accent = 0;
}
static uint32_t lane_index(const dm_vkit_t *k, const seqrt_t *r, uint32_t l, int32_t m, uint32_t L)
{
    uint32_t sm = k->lane_sync[l], len = k->lane_len[l] ? k->lane_len[l] : 1u;
    if (sm == LS_SYNC)
        return (uint32_t)m % L;
    if (sm == LS_FREE)
        return (uint32_t)m % len;
    return (uint32_t)r->lane_cnt[l] % len;
}
static void seq_collect(uint32_t v, beat_t b0, beat_t b1, beat_t inc, float humanize)
{
    const dm_seqset_t *s = &ss[v];
    const dm_vkit_t *k = &dm_kit.v[v];
    seqrt_t *r = &srt[v];
    float step_beats = DM_SPEED_BEATS[s->speed] / DM_MULT_F[s->mult], e;
    beat_t stepq = (beat_t)(step_beats * Q32);
    int32_t steps = s->steps, n, nfirst, nlast;
    uint32_t L = dm_seq_length(s->steps, s->length, s->snap), i, l;
    if (stepq <= 0)
        return;
    pattern_cache(r, s);
    e = s->shape != 0.0f ? dm_powf(3.0f, -dm_clampf(s->shape, -1.0f, 1.0f)) : 1.0f;
    nfirst = (int32_t)(b0 / stepq) - 2;
    nlast = (int32_t)(b1 / stepq) + 3;
    for (n = nfirst < 0 ? 0 : nfirst; n <= nlast; n++) {
        int32_t m = s->reset_on ? n % s->reset_len : n, seq_idx = (int32_t)((uint32_t)m % L), pat_idx = m % steps,
                g0 = (n / steps) * steps, ratch;
        float t = (float)(n - g0) / (float)steps, tw = e == 1.0f ? t : (t > 0.0f ? dm_powf(t, e) : 0.0f), prob, r1, r2;
        beat_t bt = (beat_t)g0 * stepq + (beat_t)(tw * (float)steps * step_beats * Q32), nominal = (beat_t)n * stepq;
        int active, fire;
        int8_t lv[DM_NLANE];
        dm_trig_t ti;
        if (n & 1)
            bt += (beat_t)(s->swing * 0.5f * step_beats * Q32);
        bt -= (beat_t)(s->shift * 0.5f * step_beats * Q32);
        active = dm_manual(k, (uint32_t)seq_idx) || pattern_hit(r, s, pat_idx);
        if (nominal >= b0 && nominal < b1 && (int64_t)n != r->last_step) {   /* the step playing, for the UI */
            r->last_step = n;
            dm_m.play_step[v] = (int16_t)seq_idx;
            if (seq_idx == 0)
                for (l = 0; l < DM_NLANE; l++)
                    if (k->lane_sync[l] == LS_RETRIG)
                        r->lane_cnt[l] = 0;
        }
        for (l = 0; l < DM_NLANE; l++)
            lv[l] = k->lane[l][lane_index(k, r, l, m, L) % DM_MAXSTEPS];
        bt += (beat_t)((float)lv[L_SHIFT] * 0.01f * 0.5f * step_beats * Q32);
        if (humanize > 0.0f) {   /* shared by the voices: it depends only on the 1/16 grid */
            uint32_t q16 = (uint32_t)dm_roundf((float)n * step_beats * 4.0f);
            bt += (beat_t)((dm_hash_uni(seed, q16, 77u) - 0.5f) * humanize * humanize * 0.2f * Q32);
        }
        if (!(bt >= b0 && bt < b1))
            continue;
        r1 = dm_hash_uni(seed, (uint32_t)n, 1000u + v);
        r2 = dm_hash_uni(seed, (uint32_t)n, 2000u + v);
        fire = active;
        if (active && s->density < 0.0f)
            fire = r1 < 1.0f + s->density;
        if (!active && s->density > 0.0f)
            fire = r1 < s->density;
        prob = dm_clampf((float)lv[L_PROB] * 0.01f + s->prob_off, -1.0f, 1.0f);
        ratch = (int32_t)lv[L_RATCH] + s->ratch_off;
        if (ratch < 1)
            ratch = 1;
        if (fire && prob < 0.0f)
            fire = r2 < 1.0f + prob;
        if (fire && prob > 0.0f && r2 < prob)
            ratch *= 2;
        if (!fire)
            continue;
        for (l = 0; l < DM_NLANE; l++) {               /* now the TRIG / RETRIG lanes move on */
            uint32_t sm = k->lane_sync[l];
            if (sm == LS_TRIG || sm == LS_RETRIG) {
                uint32_t len = k->lane_len[l] ? k->lane_len[l] : 1u;
                lv[l] = k->lane[l][(uint32_t)r->lane_cnt[l] % len % DM_MAXSTEPS];
                r->lane_cnt[l]++;
            }
        }
        ti.vel = dm_clamp01((float)lv[L_VEL] / 127.0f);
        ti.accent = lv[L_VEL] >= 127;
        ti.pitch = (float)lv[L_PITCH];
        ti.envscale = dm_exp2((float)lv[L_ENV] * 0.02f);
        ti.pan = (float)lv[L_PAN] * 0.01f;
        ti.var = (float)lv[L_VAR] * 0.01f;
        {
            float snd = (float)lv[L_SENDS] * 0.01f;
            ti.sdly = dm_maxf(0.0f, snd);
            ti.srev = dm_maxf(0.0f, -snd);
        }
        ti.macro2 = lv[L_MACRO2] > 0 ? (float)lv[L_MACRO2] * 0.01f : -1.0f;
        if (ratch > 16)
            ratch = 16;
        for (i = 0; i < (uint32_t)ratch; i++) {
            beat_t rb = bt + (beat_t)((float)i * step_beats / (float)ratch * Q32);
            if (rb < b1 || i == 0u)
                add_event((uint32_t)((rb - b0) / inc), v, &ti, 0);
            else if (r->npend < NPEND) {
                r->pend[r->npend].beat = rb;
                r->pend[r->npend].ti = ti;
                r->npend++;
            }
        }
    }
    for (i = 0; i < r->npend;) {                       /* ratchets that fall in later windows */
        if (r->pend[i].beat < b1) {
            if (r->pend[i].beat >= b0)
                add_event((uint32_t)((r->pend[i].beat - b0) / inc), v, &r->pend[i].ti, 0);
            r->pend[i] = r->pend[--r->npend];
        } else if (r->pend[i].beat > b1 + ((beat_t)64 << 32))
            r->pend[i] = r->pend[--r->npend];
        else
            i++;
    }
}

static void seq_reset(void)
{
    uint32_t v, l;
    for (v = 0; v < DM_NV; v++) {
        for (l = 0; l < DM_NLANE; l++)
            srt[v].lane_cnt[l] = 0;
        srt[v].last_step = -1;
        srt[v].npend = 0;
    }
}

/* --------------------------------------------------------------- MIDI --- */
static void act_push(uint32_t a)
{
    uint8_t w = dm_m.act_w;
    if ((uint8_t)(w - dm_m.act_r) >= 16u)
        return;
    dm_m.act_q[w & 15u] = (uint8_t)a;
    dm_m.act_w = (uint8_t)(w + 1u);
}
int dm_machine_action(void)
{
    uint8_t r = dm_m.act_r;
    if (r == dm_m.act_w)
        return DA_NONE;
    dm_m.act_r = (uint8_t)(r + 1u);
    return dm_m.act_q[r & 15u];
}
void dm_machine_audition(uint32_t v, float vel)
{
    dm_trig_t t;
    trig_default(&t, vel);
    add_event(0, v % DM_NV, &t, 0);
}

static void clock_tick(void)
{
    uint32_t n;
    if (clk_seen && sample_now - clk_t[(clk_seen - 1u) % 25u] > (uint64_t)(0.1f * DM_FS))
        clk_seen = 0;                     /* a gap: the tempo from the clocks after it only */
    n = clk_seen < 24u ? clk_seen : 24u;
    clk_t[clk_seen % 25u] = sample_now;
    clk_seen++;
    clk_quiet = 0;
    dm_m.clk_ext_seen = 1;
    if (n >= 6u) {                                    /* the tempo from the last clocks (up to a beat) */
        uint64_t dt = clk_t[(clk_seen - 1u) % 25u] - clk_t[(clk_seen - 1u - n) % 25u];
        if (dt > 0u) {
            float bpm = 60.0f * DM_FS * (float)n / (24.0f * (float)dt);
            if (bpm >= 20.0f && bpm <= 400.0f)
                clk_bpm += (bpm - clk_bpm) * 0.2f;
        }
    }
    if (ext_run && dm_m.clk == DM_CLK_EXT) {
        clk_err = (((beat_t)clk_n << 32) / 24) - song_beat;   /* (render: the windows stay contiguous) */
        clk_n++;
    }
}

void dm_machine_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    uint32_t ch = (status & 15u) + 1u, type = status & 0xF0u;
    dm_trig_t t;
    if (status == 0xF8u) {
        clock_tick();
        return;
    }
    if (status == 0xFAu || status == 0xFBu) {         /* START (from the top) / CONTINUE */
        if (status == 0xFAu) {
            clk_n = 0;
            song_beat = 0;
            clk_err = 0;
            was_playing = 0;                          /* (a fresh start: new seed, sequencers reset) */
        }
        clk_seen = 0;
        ext_run = 1;
        if (dm_m.clk == DM_CLK_EXT)
            dm_m.want_play = 1;
        return;
    }
    if (status == 0xFCu) {
        ext_run = 0;
        if (dm_m.clk == DM_CLK_EXT)
            dm_m.want_play = 0;
        return;
    }
    if (type == 0x90u && d2 == 0u)
        type = 0x80u;
    if (type == 0x90u) {
        float vel = (float)d2 / 127.0f;
        if (ch == 1u) {
            if (d1 % 12u < DM_NV) {
                trig_default(&t, vel);
                add_event(0, d1 % 12u, &t, 0);
            }
        } else if (ch >= 2u && ch <= 9u) {
            trig_default(&t, vel);
            t.pitch = (float)((int)d1 - 60);
            add_event(0, ch - 2u, &t, 0);
        } else if (ch == 10u) {
            if (d1 == 0u)
                dm_m.want_play = !dm_m.playing;
            else if (d1 == 1u)
                dm_m.want_play = 1;
            else if (d1 == 2u)
                dm_m.want_play = 0;
            else if (d1 >= 12u && d1 < 20u)
                dm_kit.v[d1 - 12u].mute = !dm_kit.v[d1 - 12u].mute;
            else if (d1 >= 24u && d1 < 32u)
                dm_kit.v[d1 - 24u].solo = !dm_kit.v[d1 - 24u].solo;
            else if (d1 >= 36u && d1 < 44u)
                dm_m.mom_mute[d1 - 36u] = 1;
            else if (d1 >= 48u && d1 < 56u)
                dm_m.mom_solo[d1 - 48u] = 1;
            else if (d1 >= 60u && d1 < 68u)
                add_event(0, d1 - 60u, 0, 1);
        } else if (ch == 11u) {
            if (d1 == 0u) act_push(DA_PREV_KIT);
            else if (d1 == 1u) act_push(DA_NEXT_KIT);
            else if (d1 == 2u) act_push(DA_RANDOM_KIT);
            else if (d1 == 3u) act_push(DA_RANDOMIZE_ALL);
            else if (d1 == 12u) act_push(DA_PERF_RANDOM);
            else if (d1 == 13u) act_push(DA_PERF_RESET);
            else if (d1 == 14u) act_push(DA_PERF_COMMIT);
            else if (d1 >= 24u && d1 < 32u) act_push(DA_RANDOMIZE_V0 + d1 - 24u);
        }
    } else if (type == 0x80u && ch == 10u) {
        if (d1 >= 36u && d1 < 44u)
            dm_m.mom_mute[d1 - 36u] = 0;
        else if (d1 >= 48u && d1 < 56u)
            dm_m.mom_solo[d1 - 48u] = 0;
    }
}

/* ------------------------------------------------------------- render --- */
static void fire(const ev_t *e, int depth)
{
    static uint32_t seen;                 /* the voices this hit has reached (a TRIGGER chain plays each once) */
    uint32_t v = e->voice, ck, lk;
    if (!depth)
        seen = 0;
    if (v >= DM_NV || depth > 7 || ((seen >> v) & 1u))
        return;
    seen |= 1u << v;
    if (e->choke) {
        dm_voice_choke(&voice[v]);
        return;
    }
    ck = dm_kit.v[v].choke;
    if (ck > 0u && ck - 1u != v)
        dm_voice_choke(&voice[ck - 1u]);
    dm_voice_trigger(&voice[v], &vs[v], &e->ti, dm_m.bpm_now);
    dm_m.trig_count[v]++;
    lk = dm_kit.v[v].link;
    if (lk > 0u && lk - 1u != v) {
        ev_t l = *e;
        l.voice = (uint8_t)(lk - 1u);
        fire(&l, depth + 1);
    }
}

static void fx_settings(float bpm)
{
    const dm_gkit_t *g = &dm_kit.g;
    float dtime = g->d_sync ? DM_LFODIV_BEATS[g->d_div % DM_NLFODIV] * 60.0f / bpm : g->d_time_ms * 0.001f;
    float rpre = g->r_sync ? DM_LFODIV_BEATS[g->r_div % DM_NLFODIV] * 60.0f / bpm : g->r_pre_ms * 0.001f;
    dm_delay_set(&delay, dtime, g->d_fb, g->d_glitch, g->d_fbtilt, g->d_duck, g->d_srate, dm_db2gain(g->d_gain_db),
                 g->d_stereo, g->d_pong);
    dm_reverb_set(&reverb, dm_minf(rpre, 0.499f), g->r_decay, g->r_srate, g->r_intilt, g->r_outtilt,
                  dm_db2gain(g->r_gain_db), g->r_mode);
    dm_master_set(&master, g->m_max, g->m_low, g->m_mid, g->m_high, g->m_gain, g->m_clip);
}

void dm_machine_render(float *out)
{
    uint32_t v, i, pos, ei, any_solo = 0;
    float bpm = dm_m.clk == DM_CLK_EXT && dm_m.clk_ext_seen ? clk_bpm : dm_clampf(dm_kit.g.bpm, 40.0f, 300.0f);
    beat_t inc = (beat_t)(bpm / 60.0f / DM_FS * Q32), b0, b1;
    int play;
    dm_m.bpm_now = bpm;
    for (v = 0; v < DM_NV; v++)
        dm_effective(&dm_kit, v, &vs[v], &ss[v]);
    if (dm_m.audition >= 0) {
        dm_machine_audition((uint32_t)dm_m.audition, 0.8f);
        dm_m.audition = -1;
    }
    /* the clock: gone for half a second while following it, the machine stops */
    if (dm_m.clk == DM_CLK_EXT && dm_m.playing && ++clk_quiet > (uint32_t)(0.5f * DM_FS / DM_HALF)) {
        dm_m.want_play = 0;
        ext_run = 0;
    }
    play = dm_m.want_play != 0;
    if (play && !was_playing) {
        seed = dm_hash32(seed + 0x51u);
        seq_reset();
        for (v = 0; v < DM_NV; v++)
            dm_voice_transport_start(&voice[v]);
        if (dm_m.clk != DM_CLK_EXT)
            song_beat = 0;
    }
    was_playing = (uint8_t)play;
    dm_m.playing = (uint8_t)play;
    if (play && dm_m.clk == DM_CLK_EXT && ext_run) {
        /* the external clock's phase: a jump forward when it is a quarter beat ahead (the gap's hits are lost:
         * a resync), otherwise an eighth of the error this half through the step (never backwards: no hit twice) */
        if (clk_err > ((beat_t)1 << 30)) {
            song_beat += clk_err;
            clk_err = 0;
        } else {
            beat_t corr = clk_err / 8, lim = inc * DM_HALF / 2;
            corr = corr > lim ? lim : corr < -lim ? -lim : corr;
            inc += corr / (beat_t)DM_HALF;
            clk_err -= corr;
        }
    }
    b0 = song_beat;
    b1 = b0 + inc * DM_HALF;
    if (play) {
        for (v = 0; v < DM_NV; v++)
            seq_collect(v, b0, b1, inc, dm_kit.g.humanize);
        song_beat = b1;
    } else
        for (v = 0; v < DM_NV; v++)
            dm_m.play_step[v] = -1;
    fx_settings(bpm);
    for (i = 0; i < 6u; i++)
        for (pos = 0; pos < DM_HALF; pos++)
            buf[i][pos] = 0.0f;
    for (v = 0; v < DM_NV; v++)
        any_solo |= dm_kit.v[v].solo | dm_m.mom_solo[v];
    pos = 0;
    ei = 0;
    while (pos < DM_HALF) {
        uint32_t next = (pos / DM_CTL + 1u) * DM_CTL;
        if (pos % DM_CTL == 0u)
            for (v = 0; v < DM_NV; v++)
                dm_voice_ctrl(&voice[v], &vs[v], bpm);
        while (ei < nev && evs[ei].off <= pos)
            fire(&evs[ei++], 0);
        if (ei < nev && evs[ei].off < next)
            next = evs[ei].off;
        for (v = 0; v < DM_NV; v++) {
            int audible = !(dm_kit.v[v].mute || dm_m.mom_mute[v]) && (!any_solo || dm_kit.v[v].solo || dm_m.mom_solo[v]);
            if (!dm_voice_active(&voice[v]) && !voice[v].choking)
                continue;
            if (audible)
                dm_voice_render(&voice[v], &vs[v], buf[0] + pos, buf[1] + pos, buf[2] + pos, buf[3] + pos,
                                buf[4] + pos, buf[5] + pos, next - pos);
            else
                dm_voice_render(&voice[v], &vs[v], sink[0], sink[1], sink[2], sink[3], sink[4], sink[5], next - pos);
        }
        pos = next;
    }
    nev = 0;
    for (v = 0; v < DM_NV; v++)
        dm_m.level[v] = voice[v].level;
    /* sends and master */
    dm_delay_run(&delay, buf[2], buf[3], buf[6], buf[7], DM_HALF);
    {
        float tr = dm_kit.g.d_torev;
        if (tr != 0.0f)
            for (i = 0; i < DM_HALF; i++) {
                buf[4][i] += buf[6][i] * tr;
                buf[5][i] += buf[7][i] * tr;
            }
    }
    dm_reverb_run(&reverb, buf[4], buf[5], buf[8], buf[9], DM_HALF);
    for (i = 0; i < DM_HALF; i++) {
        buf[0][i] += buf[6][i] + buf[8][i];
        buf[1][i] += buf[7][i] + buf[9][i];
    }
    dm_master_run(&master, buf[0], buf[1], DM_HALF);
    for (i = 0; i < DM_HALF; i++) {
        out[2u * i] = buf[0][i];
        out[2u * i + 1u] = buf[1][i];
    }
    sample_now += DM_HALF;
}
