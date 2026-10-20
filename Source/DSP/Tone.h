#pragma once
#include "Common.h"

namespace mp
{
// ---------- FILTRO MULTIMODO ----------
enum FilterMode
{
    ScreamLP, DarkLP, ScreamHP, DarkHP, ScreamBP, DarkBP,
    CombFBPlus, CombFBMinus, CombFFPlus, CombFFMinus, CombAP,
    kNumFilterModes
};
inline const char* const* filterModeNames()
{
    static const char* n[] = { "Scream LP", "Dark LP", "Scream HP", "Dark HP", "Scream BP", "Dark BP",
                               "Comb FB+", "Comb FB-", "Comb FF+", "Comb FF-", "Comb AP" };
    return n;
}

struct ToneFilter
{
    Svf svf; OnePole dark; DelayLine comb; float sr = 48000;
    float lastCombOut = 0;
    void prepare (float sampleRate) { sr = sampleRate; comb.init ((int) (sr / 15.0f) + 8); reset(); }
    void reset() { svf.reset(); dark.reset(); comb.clear(); lastCombOut = 0; }

    // cutoffHz, res 0..1
    float process (float x, int mode, float cutoffHz, float res)
    {
        res = clamp01 (res);
        if (mode <= DarkBP)
        {
            const bool scream = (mode % 2) == 0;
            const float q = scream ? 0.5f + res * res * 40.0f : 0.5f + res * 4.0f;
            svf.set (cutoffHz, q, sr);
            // Scream: feedback non limitato, saturato nel loop
            const float in = scream ? fastTanh (x * 1.2f) : x;
            svf.process (in);
            float y = (mode <= DarkLP) ? svf.lpOut : (mode <= DarkHP ? svf.hpOut : svf.bpOut);
            if (scream) y = fastTanh (y * 0.9f) * 1.05f;
            else { dark.setCutoff (cutoffHz * 1.3f + 200.0f, sr); y = dark.lp (y); }
            if (mode >= ScreamBP && ! scream) y *= 1.0f + res * 1.5f;
            return y;
        }
        const float d = clampf (sr / std::max (15.0f, cutoffHz), 2.0f, (float) comb.buf.size() - 4);
        const float g = res * 0.985f;
        float y = 0;
        switch (mode)
        {
            case CombFBPlus:  { const float del = comb.read (d); y = x + g * del; comb.push (fastTanh (y)); break; }
            case CombFBMinus: { const float del = comb.read (d); y = x - g * del; comb.push (fastTanh (y)); break; }
            case CombFFPlus:  { comb.push (x); y = (x + g * comb.read (d)) * 0.7f; break; }
            case CombFFMinus: { comb.push (x); y = (x - g * comb.read (d)) * 0.7f; break; }
            case CombAP:
            {
                const float del = comb.read (d);
                const float v = x + g * del;
                y = del - g * v;
                comb.push (fastTanh (v));
                break;
            }
            default: y = x;
        }
        return y;
    }
};

// ---------- DISTORSIONE ----------
enum DistMode { Tape, Tube, Bitcrush, RateCrush, Wavefold, PhaseDist, Square, Dropout, MiniGlitch, MegaGlitch, Contrast, kNumDistModes };
inline const char* const* distModeNames()
{
    static const char* n[] = { "Tape", "Tube", "Bitcrush", "Rate Crush", "Wavefold", "Phase", "Square", "Dropout", "MiniGlitch", "MegaGlitch", "Contrast" };
    return n;
}

struct Distortion
{
    float sr = 48000, held = 0, phaseAcc = 0, lastIn = 0;
    bool dropping = false; Rng rng { 0xD15 };
    DelayLine micro, glitchBuf; Rng monoRng { 0x6117 }, stereoRng { 0x6117 };
    float gPos = 0, gVel = 0, gTime = 0.05f, gPrev = 0.05f, gXf = 1;
    int channel = 0;
    void prepare (float sampleRate, int ch = 0)
    {
        sr = sampleRate; channel = ch;
        micro.init ((int) (sr * 0.01f) + 8); glitchBuf.init ((int) (sr * 0.25f) + 8);
        stereoRng = Rng (0x6117u + (uint32_t) ch * 7919u);
        reset();
    }
    void reset() { held = phaseAcc = lastIn = 0; dropping = false; micro.clear(); glitchBuf.clear(); gPos = gVel = 0; }

    // versione ridotta dello Shatter Delay dentro la voce
    float glitch (float x, float amt, bool stereo)
    {
        Rng& r = stereo ? stereoRng : monoRng;
        gVel = gVel * 0.999f + r.bi() * 0.0004f * (0.2f + amt);
        gPos = clampf (gPos + gVel, -1, 1);
        const float target = clampf (amt * 0.2f * (1.0f + gPos * amt * 0.9f), 0.001f, 0.24f);
        if (std::abs (target - gTime) > 0.004f && gXf >= 1) { gPrev = gTime; gTime = target; gXf = 0; }
        if (gXf < 1) gXf = std::min (1.0f, gXf + 1.0f / (0.01f * sr));
        const float y = glitchBuf.read (gPrev * sr) * (1 - gXf) + glitchBuf.read (gTime * sr) * gXf;
        glitchBuf.push (fastTanh (x + y * amt * 0.75f));
        return lerp (x, x * 0.6f + y, std::min (1.0f, amt * 1.5f));
    }

    float process (float x, int mode, float amt)
    {
        amt = clamp01 (amt);
        switch (mode)
        {
            case Tape:
            {
                const float drive = 1.0f + amt * 12.0f;
                const float s = fastTanh (x * drive * 0.8f + 0.05f * x * x);
                return s / (0.8f + amt * 0.6f);
            }
            case Tube:
            {
                const float drive = 1.0f + amt * 15.0f;
                float v = x * drive;
                v = v >= 0 ? fastTanh (v) : fastTanh (v * 0.6f) * 0.75f;   // asimmetrica (raddrizzamento)
                return (v + 0.1f * std::abs (v)) / (1.0f + amt * 0.5f);
            }
            case Bitcrush:
            {
                if (amt <= 0.0001f) return x;
                const float bits = lerp (16.0f, 1.0f, std::pow (amt, 0.6f));
                const float steps = std::pow (2.0f, bits - 1.0f);
                return std::round (clampf (x, -1, 1) * steps) / steps;
            }
            case RateCrush:
            {
                if (amt <= 0.0001f) return x;
                const float ratio = std::pow (amt, 1.5f) * 0.995f;
                phaseAcc += 1.0f - ratio;
                if (phaseAcc >= 1.0f) { phaseAcc -= 1.0f; held = x; }
                return held;
            }
            case Wavefold:
            {
                float v = x * (1.0f + amt * 8.0f);
                for (int i = 0; i < 6 && (v > 1.0f || v < -1.0f); ++i)
                    v = v > 1.0f ? 2.0f - v : (v < -1.0f ? -2.0f - v : v);
                return v;
            }
            case PhaseDist:
            {
                micro.push (x);
                const float d = 2.0f + (0.5f + 0.5f * fastTanh (lastIn * 3.0f)) * amt * sr * 0.004f;
                const float y = micro.read (d);
                lastIn = y;
                return lerp (x, y, std::min (1.0f, amt * 2.0f));
            }
            case Square:
            {
                // arrotonda verso l'onda quadra (lato destro di "SHAPE")
                const float s = x >= 0 ? 1.0f : -1.0f;
                const float m = std::pow (std::abs (x), 1.0f - amt * 0.98f);
                return s * std::min (1.0f, m) * (0.6f + 0.4f * (1.0f - amt));
            }
            case Dropout:
            {
                // fa cadere cicli d'onda interi (waveset)
                if ((lastIn <= 0.0f && x > 0.0f))
                    dropping = rng.uni() < amt * 0.85f;
                lastIn = x;
                return dropping ? 0.0f : x;
            }
            case MiniGlitch: return amt <= 0.001f ? x : glitch (x, amt, false);
            case MegaGlitch: return amt <= 0.001f ? x : glitch (x, amt, true);
            case Contrast:
            {
                // dolcificatore a distorsione di fase: enfatizza i transitori dei motori modali
                const float k = amt * 3.0f;
                const float y = std::sin (kPi * 0.5f * clampf (x, -1.5f, 1.5f) + k * std::sin (kTwoPi * clampf (x, -1.5f, 1.5f)) * 0.25f);
                return lerp (x, y, std::min (1.0f, amt * 2.0f));
            }
            default: return x;
        }
    }
};
} // namespace mp
