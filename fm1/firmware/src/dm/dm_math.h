/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — maths for the audio code.
 * The FM-1's CPU (JieLi pi32v2, -mfprev1) has single-precision float instructions that work on the ordinary
 * registers, so float is safe in the audio ISR (the ISR wrapper saves r0..r3, the C body the rest) and costs about
 * as much as integer maths. There is no libm: every function the engines need is here, cheap and good enough for
 * audio (sine from a table, exp2 / log2 as polynomials). Control-rate maths (filters' coefficients, pow) may be
 * slower; per-sample code uses only +, -, *, the table sine, fast_tanh and the occasional divide. */
#ifndef DM_MATH_H
#define DM_MATH_H
#include <stdint.h>

#define DM_FS 44100.0f
#define DM_ISR (1.0f / 44100.0f)
#define DM_PI 3.14159265358979f
#define DM_TWOPI 6.28318530717959f
#define DM_PH_PER_HZ (4294967296.0f / 44100.0f)      /* uint32 phase increment per Hz */

static inline float dm_minf(float a, float b) { return a < b ? a : b; }
static inline float dm_maxf(float a, float b) { return a > b ? a : b; }
static inline float dm_clampf(float x, float lo, float hi) { return !(x >= lo) ? lo : x > hi ? hi : x; }   /* NaN -> lo */
static inline float dm_clamp01(float x) { return !(x >= 0.0f) ? 0.0f : x > 1.0f ? 1.0f : x; }
static inline float dm_lerp(float a, float b, float t) { return a + (b - a) * t; }
static inline float dm_absf(float x) { return x < 0.0f ? -x : x; }
static inline int dm_clampi(int x, int lo, int hi) { return x < lo ? lo : x > hi ? hi : x; }
static inline int dm_finite(float x) { return x == x && x < 1.0e30f && x > -1.0e30f; }

/* floor / round for the ranges the engines use (|x| < 2^31) */
static inline float dm_floorf(float x)
{
    int32_t i = (int32_t)x;
    float f = (float)i;
    return f > x ? f - 1.0f : f;
}
static inline float dm_roundf(float x) { return dm_floorf(x + 0.5f); }
static inline float dm_fracf(float x) { return x - dm_floorf(x); }

/* the original's rational tanh x (27 + x^2) / (27 + 9 x^2), |x| clamped to 4.5, from a 512-segment table with
 * linear interpolation (error < 3e-5): no divide on the audio path */
#define DM_TANH_N 512u
#define DM_TANH_K (512.0f / 9.0f)
extern float dm_tanh_tab[DM_TANH_N + 2u];
static inline float dm_tanh_exact(float x)
{
    float x2;
    x = dm_clampf(x, -4.5f, 4.5f);
    x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
static inline float dm_tanh(float x)
{
    float u = (x + 4.5f) * DM_TANH_K, f;
    int32_t i;
    if (!(u > 0.0f))                               /* (NaN too: never an index from it) */
        return -1.0f;
    if (u >= (float)DM_TANH_N)
        return 1.0f;
    i = (int32_t)u;
    f = u - (float)i;
    return dm_tanh_tab[i] + (dm_tanh_tab[i + 1] - dm_tanh_tab[i]) * f;
}

/* ---- sine: 1024-point table with linear interpolation (dm_math.c fills it) ---- */
#define DM_SIN_BITS 10
#define DM_SIN_N (1u << DM_SIN_BITS)
extern float dm_sin_tab[DM_SIN_N + 1u];
void dm_math_init(void);

/* sine of a uint32 phase (2^32 = one cycle) */
static inline float dm_sin_ph(uint32_t ph)
{
    uint32_t i = ph >> (32 - DM_SIN_BITS);
    float f = (float)(ph & ((1u << (32 - DM_SIN_BITS)) - 1u)) * (1.0f / (float)(1u << (32 - DM_SIN_BITS)));
    float a = dm_sin_tab[i];
    return a + (dm_sin_tab[i + 1u] - a) * f;
}
/* phase in cycles (any float) -> uint32 phase */
static inline uint32_t dm_cyc2ph(float c)
{
    float fr = c - (float)(int32_t)c;               /* (-1, 1) */
    if (fr < 0.0f)
        fr += 1.0f;
    return (uint32_t)(fr * 4294967040.0f);
}
static inline float dm_sin_cyc(float c) { return dm_sin_ph(dm_cyc2ph(c)); }        /* sin(2 pi c) */
static inline float dm_cos_cyc(float c) { return dm_sin_ph(dm_cyc2ph(c) + 0x40000000u); }
/* phase (uint32) -> 0..1 */
static inline float dm_ph01(uint32_t ph) { return (float)(ph >> 8) * (1.0f / 16777216.0f); }
static inline uint32_t dm_hz2inc(float hz)
{
    float v = hz * DM_PH_PER_HZ;
    if (!(v > 0.0f))
        v = 0.0f;
    if (v > 2147483520.0f)
        v = 2147483520.0f;
    return (uint32_t)v;
}

/* ---- exp2 / log2 / pow / exp (control rate) ---- */
static inline float dm_exp2(float x)
{
    union { float f; uint32_t u; } v;
    float fi, t, p;
    int32_t i;
    if (!(x >= -126.0f))
        return 0.0f;
    if (x > 126.0f)
        x = 126.0f;
    fi = dm_floorf(x + 0.5f);                      /* nearest integer: the rest in [-0.5, 0.5] */
    i = (int32_t)fi;
    t = (x - fi) * 0.69314718f;                    /* e^t, |t| <= 0.347: Taylor to t^7, error < 1e-8 */
    p = 1.0f + t * (1.0f + t * (0.5f + t * (0.16666667f + t * (0.041666668f + t * (0.0083333338f +
        t * (0.0013888889f + t * 0.00019841270f))))));
    v.f = p;
    v.u += (uint32_t)i << 23;
    return v.f;
}
static inline float dm_log2(float x)
{
    union { float f; uint32_t u; } v;
    float m, e, t, t2;
    if (x <= 0.0f)
        return -126.0f;
    v.f = x;
    e = (float)((int32_t)((v.u >> 23) & 255u) - 127);
    v.u = (v.u & 0x007FFFFFu) | 0x3F800000u;      /* mantissa 1..2 */
    m = v.f;
    if (m > 1.41421356f) {                         /* to [0.707, 1.414]: |t| <= 0.172 */
        m *= 0.5f;
        e += 1.0f;
    }
    t = (m - 1.0f) / (m + 1.0f);                   /* log2(m) = 2/ln2 atanh(t) */
    t2 = t * t;
    return e + 2.88539008f * t * (1.0f + t2 * (0.33333333f + t2 * (0.2f + t2 * (0.14285714f + t2 * 0.11111111f))));
}
static inline float dm_powf(float a, float b) { return a <= 0.0f ? 0.0f : dm_exp2(b * dm_log2(a)); }
static inline float dm_expf(float x) { return dm_exp2(x * 1.44269504f); }
static inline float dm_semis(float st) { return dm_exp2(st * (1.0f / 12.0f)); }
static inline float dm_db2gain(float db) { return db <= -96.0f ? 0.0f : dm_exp2(db * 0.16609640f); }
static inline float dm_sqrtf(float x)
{
    union { float f; uint32_t u; } v;
    float y;
    if (x <= 0.0f)
        return 0.0f;
    v.f = x;
    v.u = 0x1FBD1DF5u + (v.u >> 1);
    y = v.f;
    y = 0.5f * (y + x / y);
    y = 0.5f * (y + x / y);
    return y;
}
/* tan(pi * hz / FS) for the SVF, hz clamped as the original (10 Hz .. 0.47 FS) */
static inline float dm_svf_g(float hz)
{
    float c = dm_clampf(hz, 10.0f, DM_FS * 0.47f) * (0.5f / DM_FS);    /* cycles: pi*hz/FS = 2 pi * (hz / 2FS) */
    return dm_sin_cyc(c) / dm_cos_cyc(c);
}
/* one-pole coefficient exp(-2 pi hz / FS), hz clamped 5 Hz .. 0.49 FS */
static inline float dm_onepole_a(float hz) { return dm_expf(-DM_TWOPI * dm_clampf(hz, 5.0f, DM_FS * 0.49f) * DM_ISR); }
/* decay coefficient for a time constant of sec seconds */
static inline float dm_decay_coef(float sec) { return dm_expf(-1.0f / dm_maxf(1.0e-4f, sec * DM_FS)); }

/* ---- random ---- */
typedef struct { uint32_t s; } dm_rng_t;
static inline uint32_t dm_rng_next(dm_rng_t *r)
{
    uint32_t s = r->s ? r->s : 1u;
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    r->s = s;
    return s;
}
static inline float dm_rng_uni(dm_rng_t *r) { return (float)(dm_rng_next(r) >> 8) * (1.0f / 16777216.0f); }
static inline float dm_rng_bi(dm_rng_t *r) { return dm_rng_uni(r) * 2.0f - 1.0f; }
static inline uint32_t dm_hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}
static inline float dm_hash_uni(uint32_t a, uint32_t b, uint32_t c)
{
    return (float)((dm_hash32(a * 0x9E3779B1u ^ dm_hash32(b + 0x85ebca6bu) ^ dm_hash32(c + 0xc2b2ae35u))) >> 8) *
           (1.0f / 16777216.0f);
}

/* ---- small DSP blocks ---- */
typedef struct { float z, a; } dm_onepole_t;
static inline float dm_op_lp(dm_onepole_t *f, float x) { f->z = x + f->a * (f->z - x); return f->z; }
static inline float dm_op_hp(dm_onepole_t *f, float x) { return x - dm_op_lp(f, x); }

/* TPT state-variable filter (Zavalishin), coefficients at control rate */
typedef struct { float ic1, ic2, k, a1, a2, a3, lp, bp, hp; } dm_svf_t;
static inline void dm_svf_set(dm_svf_t *f, float hz, float q)
{
    float g = dm_svf_g(hz);
    f->k = 1.0f / dm_maxf(0.025f, q);
    f->a1 = 1.0f / (1.0f + g * (g + f->k));
    f->a2 = g * f->a1;
    f->a3 = g * f->a2;
}
static inline void dm_svf_run(dm_svf_t *f, float x)
{
    float v3 = x - f->ic2, v1 = f->a1 * f->ic1 + f->a2 * v3, v2 = f->ic2 + f->a2 * f->ic1 + f->a3 * v3;
    f->ic1 = 2.0f * v1 - f->ic1;
    f->ic2 = 2.0f * v2 - f->ic2;
    f->lp = v2;
    f->bp = v1;
    f->hp = x - f->k * v1 - v2;
}
static inline void dm_svf_guard(dm_svf_t *f)       /* once a block: a filter that ran away starts again */
{
    if (!dm_finite(f->ic1) || !dm_finite(f->ic2) || dm_absf(f->ic1) > 1.0e6f || dm_absf(f->ic2) > 1.0e6f)
        f->ic1 = f->ic2 = 0.0f;
}

/* Paul Kellet's cheap pink noise */
typedef struct { float b0, b1, b2; } dm_pink_t;
static inline float dm_pink(dm_pink_t *p, float w)
{
    p->b0 = 0.99765f * p->b0 + w * 0.0990460f;
    p->b1 = 0.96300f * p->b1 + w * 0.2965164f;
    p->b2 = 0.57000f * p->b2 + w * 1.0526913f;
    return (p->b0 + p->b1 + p->b2 + w * 0.1848f) * 0.25f;
}

/* ---- int16 delay lines (the RAM is small): a buffer of n samples, q = full scale (32767 for +-1, 8191 for +-4) ---- */
typedef struct { int16_t *b; uint32_t n, w; float q, iq; } dm_dline_t;
static inline void dm_dl_init(dm_dline_t *d, int16_t *buf, uint32_t n, float range)
{
    uint32_t i;
    d->b = buf;
    d->n = n;
    d->w = 0;
    d->q = 32767.0f / range;
    d->iq = range / 32767.0f;
    for (i = 0; i < n; i++)
        buf[i] = 0;
}
static inline void dm_dl_push(dm_dline_t *d, float x)
{
    float v = x * d->q;
    v = v > 32767.0f ? 32767.0f : v > -32767.0f ? v : -32767.0f;   /* (NaN -> the bottom) */
    d->b[d->w] = (int16_t)(v < 0.0f ? v - 0.5f : v + 0.5f);
    if (++d->w >= d->n)
        d->w = 0;
}
/* fractional read, delay in samples (clamped 1 .. n-2) */
static inline float dm_dl_read(const dm_dline_t *d, float del)
{
    float rp, f;
    int32_t i0, i1, n = (int32_t)d->n;
    if (!(del >= 1.0f))
        del = 1.0f;
    if (del > (float)(n - 2))
        del = (float)(n - 2);
    rp = (float)d->w - del;
    if (rp < 0.0f)
        rp += (float)n;
    i0 = (int32_t)rp;
    f = rp - (float)i0;
    i1 = i0 + 1;
    if (i1 >= n)
        i1 = 0;
    return ((float)d->b[i0] + (float)(d->b[i1] - d->b[i0]) * f) * d->iq;
}
#endif
