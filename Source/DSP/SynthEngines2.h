#pragma once
// Motori di sintesi della Fase 2 (incluso da SynthEngines.h)
namespace mp
{
// ===================== CHAOS: due oscillatori in retroazione reciproca + filtro modulato =====================
struct ChaosEngine : Engine
{
    Phasor a, b; float lastA = 0, lastB = 0; DecayEnv env; Svf filt;
    void trigger (float) override { env.trigger(); }
    float process (float f, const float* p) override
    {
        env.setTime (0.02f + p[3] * 0.4f, sr);
        const float chaos = p[1] * 4.0f + env.next() * p[3] * 6.0f;
        const float r = std::pow (2.0f, lerp (-2.0f, 3.0f, p[0]));
        const float pa = a.tick (std::max (1.0f, f * (1.0f + chaos * lastB)), sr);
        const float pb = b.tick (std::max (1.0f, f * r * (1.0f + chaos * lastA)), sr);
        lastA = sineOf (pa); lastB = (pb < 0.5f ? 1.0f : -1.0f) * 0.7f + sineOf (pb) * 0.3f;
        const float cut = clampf (f * 4.0f * std::pow (2.0f, (lastA + lastB) * p[2] * 4.0f), 40.0f, sr * 0.45f);
        filt.set (cut, 2.5f, sr); filt.process (lastA * 0.6f + lastB * 0.4f);
        return fastTanh (filt.lpOut * 1.5f);
    }
};

// ===================== VOSIM: due seni sommati, finestrati da una rampa =====================
struct VosimEngine : Engine
{
    Phasor win; double s1 = 0, s2 = 0; DecayEnv env;
    void trigger (float) override { env.trigger(); win.reset(); }
    float process (float f, const float* p) override
    {
        env.setTime (0.01f + p[3] * 0.3f, sr);
        const float w = win.tick (f, sr);
        if (w < f / sr) { s1 = 0; s2 = 0; }                    // sincronizzazione sulla finestra
        const float r1 = lerp (1.0f, 12.0f, p[0]), r2 = lerp (1.0f, 16.0f, p[1]);
        s2 += (double) f * r2 / sr; s1 += (double) f * r1 / sr;
        const float fm = (p[2] + env.next() * p[3]) * 3.0f;
        const float sin2 = std::sin (kTwoPi * (float) (s2 - std::floor (s2)));
        const float sin1 = std::sin (kTwoPi * (float) (s1 - std::floor (s1)) + fm * sin2);
        const float window = 1.0f - w;                         // rampa discendente
        return (sin1 * 0.6f + sin2 * 0.4f) * window * window * 1.6f;
    }
};

// ===================== SUMSYN: sintesi per sommatoria (Moorer) =====================
struct SumSynEngine : Engine
{
    Phasor o1, o2, o3; DecayEnv env;
    void trigger (float) override { env.trigger(); o1.reset(); o2.reset(); o3.reset(); }
    float process (float f, const float* p) override
    {
        env.setTime (0.005f + p[3] * 0.25f, sr);
        const float r = lerp (0.5f, 8.0f, p[0]);
        const float f3 = std::sqrt (r) * f;
        const float x3 = sineOf (o3.tick (f3, sr));
        const float pm = (p[2] * 3.0f + env.next() * p[3] * 8.0f) * x3;
        const float th = kTwoPi * o1.tick (f, sr) + pm;
        const float be = kTwoPi * o2.tick (f * r, sr);
        // formula chiusa della somma di serie di armoniche smorzate
        const float a = clampf (lerp (0.2f, 0.92f, p[1]), 0.0f, 0.95f);
        const float num = std::sin (th) - a * std::sin (th - be);
        const float den = 1.0f + a * a - 2.0f * a * std::cos (be);
        float y = num / std::max (0.05f, den) * (1.0f - a) * 1.2f;
        if (p[1] > 0.6f)                                       // BRIGHT alto: wavefolding
        {
            y *= 1.0f + (p[1] - 0.6f) * 6.0f;
            for (int i = 0; i < 3 && (y > 1 || y < -1); ++i) y = y > 1 ? 2 - y : -2 - y;
        }
        return y;
    }
};

// ===================== RESON: burst di rumore in un banco di filtri risonanti =====================
struct ResonEngine : Engine
{
    static constexpr int N = 6; Svf bank[N]; DecayEnv burst;
    void trigger (float) override { burst.trigger(); }
    float process (float f, const float* p) override
    {
        static const float bar[N] = { 1.0f, 2.756f, 5.404f, 8.933f, 13.34f, 18.64f };
        static const float memb[N] = { 1.0f, 1.594f, 2.136f, 2.296f, 2.653f, 2.918f };
        burst.setTime (0.003f, sr);
        const float ex = noise() * burst.next();
        float out = 0;
        for (int i = 0; i < N; ++i)
        {
            const float ratio = lerp (memb[i], bar[i], p[2]);
            const float hz = f * 2.0f * ratio;
            if (hz > sr * 0.45f) continue;
            const float q = lerp (5.0f, 400.0f, p[3]) / (1.0f + (float) i * p[1] * 3.0f);
            bank[i].set (hz, q, sr); bank[i].process (ex);
            const float amp = std::pow (lerp (0.3f, 1.0f, p[0]), (float) i);
            out += bank[i].bpOut * amp;
        }
        return fastTanh (out * 2.0f);
    }
};

// ===================== COMBINATORIC: seno attraverso un filtro a pettine modulato =====================
struct CombinatoricEngine : Engine
{
    Phasor osc; DelayLine dl; DecayEnv penv;
    void prepare (float s) override { Engine::prepare (s); dl.init ((int) (s / 15.0f) + 8); }
    void trigger (float) override { penv.trigger(); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.04f, sr);
        const float x = sineOf (osc.tick (f, sr));
        const float ratio = std::pow (2.0f, lerp (-2.0f, 3.0f, p[0]));
        float cf = f * ratio * (1.0f + p[2] * 4.0f * penv.next()) * (1.0f + x * p[3] * 2.0f);
        cf = clampf (cf, 15.0f, sr * 0.45f);
        const float fb = lerp (0.0f, 0.985f, p[1]);
        const float y = x + fb * dl.read (sr / cf);
        dl.push (fastTanh (y));
        return fastTanh (y * 0.6f);
    }
};

// ---- forme d'onda morfabili per i motori a due oscillatori ----
inline float morphWave (float ph, float shape)
{
    const float sine = sineOf (ph), tri = 4.0f * std::abs (ph - 0.5f) - 1.0f, saw = 2.0f * ph - 1.0f, sq = ph < 0.5f ? 1.0f : -1.0f;
    const float s = clamp01 (shape) * 3.0f;
    return s < 1 ? lerp (sine, tri, s) : (s < 2 ? lerp (tri, saw, s - 1) : lerp (saw, sq, s - 2));
}

// ===================== TERRAINING: wave terrain + operatori bitwise =====================
struct TerrainEngine : Engine
{
    Phasor a, b;
    void trigger (float) override { a.reset(); b.reset(); }
    float process (float f, const float* p) override
    {
        const float r2 = std::pow (2.0f, lerp (-2.0f, 3.0f, p[0]));
        const float x = morphWave (a.tick (f, sr), p[1]);
        const float y = morphWave (b.tick (f * r2, sr), p[2]);
        const float t = p[3];
        if (t < 0.5f)
        {
            // morph fra superfici: somma → prodotto → sin(x*y) → cos(x+y)*x
            const float k = t * 6.0f;
            const float s0 = (x + y) * 0.5f, s1 = x * y, s2 = std::sin (kPi * x * y * 2.0f), s3 = std::cos (kPi * (x + y)) * x;
            return k < 1 ? lerp (s0, s1, k) : (k < 2 ? lerp (s1, s2, k - 1) : lerp (s2, s3, k - 2));
        }
        // operatori bitwise sui campioni quantizzati
        const int qx = (int) ((x * 0.5f + 0.5f) * 255), qy = (int) ((y * 0.5f + 0.5f) * 255);
        const float k = (t - 0.5f) * 2.0f;
        int v = k < 0.33f ? (qx ^ qy) : (k < 0.66f ? (qx & qy) | (qx >> 2) : (qx | qy) - (qx & qy) * 2);
        return ((float) (v & 255) / 127.5f - 1.0f) * 0.8f;
    }
};

// ===================== SHAME: sample & hold digitali, FM e feedback =====================
struct ShameEngine : Engine
{
    Phasor a, b; float last = 0, held = 0, prevA = 0, prevB = 0;
    void trigger (float) override {}
    float process (float f, const float* p) override
    {
        const float r2 = std::pow (2.0f, lerp (-2.0f, 4.0f, p[0]));
        const float pb = b.tick (f * r2, sr);
        const float y = pb < 0.5f ? 1.0f : -1.0f;
        const float pa = a.tick (std::max (1.0f, f * (1.0f + y * p[1] * 3.0f) + last * p[2] * f * 4.0f), sr);
        const float x = 2.0f * pa - 1.0f;
        last = x;
        // SHAME: diverse logiche di campionamento incrociato
        const float sh = p[3] * 4.0f;
        const bool trigA = pa < prevA, trigB = pb < prevB;
        prevA = pa; prevB = pb;
        if (sh < 1) { if (trigB) held = x; return lerp (x, held, sh); }
        if (sh < 2) { if (trigA) held = y * pb; return lerp (held, x * held, sh - 1); }
        if (sh < 3) { if (trigA || trigB) held = (x + y) * 0.5f; return held; }
        if (trigB) held = std::round (x * 4.0f) / 4.0f;
        return lerp (held, held * y, sh - 3);
    }
};

// ===================== ENTANGLED: ring mod, finestre, terrain su seni =====================
struct EntangledEngine : Engine
{
    Phasor a, b; float last = 0;
    void trigger (float) override { a.reset(); b.reset(); }
    float process (float f, const float* p) override
    {
        const float r2 = std::pow (2.0f, lerp (-2.0f, 4.0f, p[0]));
        const float pb = b.tick (f * r2, sr);
        const float y = sineOf (pb);
        const float pa = a.tick (f, sr);
        const float x = std::sin (kTwoPi * pa + y * p[1] * 4.0f + last * p[2] * 2.5f);
        last = x;
        const float t = p[3] * 4.0f;
        const float sum = (x + y) * 0.5f, ring = x * y, window = x * (0.5f - 0.5f * std::cos (kTwoPi * pb)), terr = std::sin (kPi * (x * x - y * y));
        return t < 1 ? lerp (sum, ring, t) : (t < 2 ? lerp (ring, window, t - 1) : (t < 3 ? lerp (window, terr, t - 2) : lerp (terr, terr * ring * 2, t - 3)));
    }
};

// ===================== HAYWIRE: rullante "acustico" con sezione rattle =====================
struct HaywireEngine : Engine
{
    Phasor o1, o2; DecayEnv penv, rattleEnv, body; Svf bp1, bp2;
    void trigger (float) override { penv.trigger(); rattleEnv.trigger(); body.trigger(); o1.reset (0.25); o2.reset (0.25); }
    float process (float f, const float* p) override
    {
        penv.setTime (0.012f, sr); rattleEnv.setTime (0.18f, sr); body.setTime (0.06f, sr);
        const float e = penv.next();
        const float m = 1.0f + p[1] * 4.0f * e * e;
        const float a = sineOf (o1.tick (f * 1.5f * m, sr)), b = sineOf (o2.tick (f * 2.65f * m, sr));
        const float bo = body.next();
        const float tone = (a * (0.4f + p[0] * 0.6f) + b * p[0] * 0.8f * bo) * (0.6f + 0.4f * bo);
        const float c = 1500.0f * std::pow (6.0f, p[2]);
        bp1.set (c, 1.2f, sr); bp2.set (c * 2.3f, 1.4f, sr);
        bp1.process (noise()); bp2.process (noise());
        const float rattle = (bp1.bpOut + bp2.bpOut * 0.7f) * rattleEnv.next() * p[3] * 2.5f;
        return fastTanh ((tone + rattle) * 1.2f);
    }
};

// ===================== SCRAPYARD: pila modale di 8 seni modulata in FM da un nono =====================
struct ScrapyardEngine : Engine
{
    static constexpr int N = 8; Phasor ph[N], mod; float amp[N] {}; DecayEnv zap;
    void trigger (float) override { for (int i = 0; i < N; ++i) { amp[i] = 1.0f; ph[i].reset(); } zap.trigger(); }
    float process (float f, const float* p) override
    {
        static const float plate[N] = { 1.0f, 1.42f, 2.08f, 2.79f, 3.41f, 4.27f, 5.05f, 6.18f };
        zap.setTime (0.015f, sr);
        const float z = 1.0f + p[1] * 6.0f * zap.next();
        const float mr = std::pow (2.0f, lerp (-1.0f, 3.0f, p[0]));
        const float m = sineOf (mod.tick (f * 2.0f * mr, sr)) * p[3] * 2.0f;
        const float stretch = lerp (0.7f, 1.8f, p[2]);
        float out = 0;
        for (int i = 0; i < N; ++i)
        {
            const float hz = f * 2.0f * (1.0f + (plate[i] - 1.0f) * stretch) * z * (1.0f + m * 0.5f);
            if (hz > sr * 0.45f || hz < 0) continue;
            out += sineOf (ph[i].tick (hz, sr)) * amp[i] / (1.0f + i * 0.5f);
            amp[i] *= std::exp (-1.0f / ((1.2f / (1.0f + i * 0.7f)) * sr));
        }
        return fastTanh (out * 0.6f);
    }
};
} // namespace mp
