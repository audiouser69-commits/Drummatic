#pragma once
// Manipolo — utilità DSP comuni (C++ puro, nessuna dipendenza da JUCE)
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <array>
#include <vector>

namespace mp
{
constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;

inline float clamp01 (float x) { return std::min (1.0f, std::max (0.0f, x)); }
inline float clampf (float x, float lo, float hi) { return std::min (hi, std::max (lo, x)); }
inline float lerp (float a, float b, float t) { return a + (b - a) * t; }
inline float semis (float st) { return std::pow (2.0f, st / 12.0f); }
inline float dbToGain (float db) { return db <= -96.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }
inline float fastTanh (float x)
{
    x = clampf (x, -4.5f, 4.5f);
    const float x2 = x * x;
    return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}
inline float softClip (float x) { return fastTanh (x); }
inline bool finite (float x) { return std::isfinite (x); }

// Generatore pseudo-casuale veloce (xorshift)
struct Rng
{
    uint32_t s = 0x12345678u;
    explicit Rng (uint32_t seed = 0x12345678u) : s (seed ? seed : 1u) {}
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    float uni() { return (next() >> 8) * (1.0f / 16777216.0f); }        // 0..1
    float bi() { return uni() * 2.0f - 1.0f; }                           // -1..1
};

inline uint32_t hash32 (uint32_t x)
{
    x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
    return x;
}
inline float hashUni (uint32_t a, uint32_t b = 0, uint32_t c = 0)
{
    return (hash32 (a * 0x9E3779B1u ^ hash32 (b + 0x85ebca6bu) ^ hash32 (c + 0xc2b2ae35u)) >> 8) * (1.0f / 16777216.0f);
}

// Rumore rosa (Paul Kellet, versione economica)
struct PinkNoise
{
    float b0 = 0, b1 = 0, b2 = 0;
    float process (float white)
    {
        b0 = 0.99765f * b0 + white * 0.0990460f;
        b1 = 0.96300f * b1 + white * 0.2965164f;
        b2 = 0.57000f * b2 + white * 1.0526913f;
        return (b0 + b1 + b2 + white * 0.1848f) * 0.25f;
    }
};

// Filtro a un polo
struct OnePole
{
    float z = 0, a = 0;
    void setCutoff (float hz, float sr) { a = std::exp (-kTwoPi * clampf (hz, 5.0f, sr * 0.49f) / sr); }
    float lp (float x) { z = x + a * (z - x); return z; }
    float hp (float x) { return x - lp (x); }
    void reset() { z = 0; }
};

// SVF TPT (Zavalishin) — LP/BP/HP simultanei
struct Svf
{
    float ic1 = 0, ic2 = 0, g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0;
    float lpOut = 0, bpOut = 0, hpOut = 0;
    void set (float hz, float q, float sr)
    {
        g = std::tan (kPi * clampf (hz, 10.0f, sr * 0.47f) / sr);
        k = 1.0f / std::max (0.025f, q);
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void process (float x)
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lpOut = v2; bpOut = v1; hpOut = x - k * v1 - v2;
        if (! finite (ic1) || ! finite (ic2)) reset();
    }
    void reset() { ic1 = ic2 = 0; }
};

// Linea di ritardo frazionaria
struct DelayLine
{
    std::vector<float> buf; int w = 0;
    void init (int size) { buf.assign ((size_t) std::max (4, size), 0.0f); w = 0; }
    void push (float x) { buf[(size_t) w] = x; if (++w >= (int) buf.size()) w = 0; }
    float read (float delaySamples) const
    {
        const int n = (int) buf.size();
        delaySamples = clampf (delaySamples, 1.0f, (float) (n - 2));
        float rp = (float) w - delaySamples;
        while (rp < 0) rp += (float) n;
        const int i0 = (int) rp; const float f = rp - (float) i0;
        const int i1 = (i0 + 1) % n;
        return buf[(size_t) i0] + (buf[(size_t) i1] - buf[(size_t) i0]) * f;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); }
};

// Inviluppo esponenziale semplice per usi interni ai motori
struct DecayEnv
{
    float v = 0, coef = 0.999f;
    void setTime (float sec, float sr) { coef = std::exp (-1.0f / std::max (1.0e-4f, sec * sr)); }
    void trigger (float level = 1.0f) { v = level; }
    float next() { const float o = v; v *= coef; return o; }
};

inline float polyBlep (float t, float dt)
{
    if (t < dt) { t /= dt; return t + t - t * t - 1.0f; }
    if (t > 1.0f - dt) { t = (t - 1.0f) / dt; return t * t + t + t + 1.0f; }
    return 0.0f;
}
} // namespace mp
