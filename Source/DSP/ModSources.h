#pragma once
#include "Common.h"

namespace mp
{
// Inviluppo AHD (Attack-Hold-Decay) con curva, sensibilità alla velocity e "soft reset"
struct AhdEnvelope
{
    enum Stage { Idle, Attack, Hold, Decay };
    Stage stage = Idle;
    float value = 0, peak = 1, startLevel = 0;
    float pos = 0;              // secondi trascorsi nella fase
    float attack = 0.001f, hold = 0.001f, decay = 0.2f, curve = 1.0f, timeScale = 1.0f;

    void trigger (float velocity01, float velAmount)
    {
        peak = 1.0f - clamp01 (velAmount) * (1.0f - clamp01 (velocity01));
        startLevel = value;     // soft reset: riparte dal valore corrente, niente click
        stage = Attack; pos = 0;
    }
    void choke() { if (stage != Idle) { stage = Decay; pos = 0; decay = 0.015f; peak = value; curve = 0; timeScale = 1; } }
    bool active() const { return stage != Idle; }

    // avanza di dt secondi e restituisce il valore
    float advance (float dt)
    {
        switch (stage)
        {
            case Idle: value = 0; break;
            case Attack:
            {
                const float a = std::max (1.0e-4f, attack * timeScale);
                pos += dt;
                if (pos >= a) { stage = Hold; pos = 0; value = peak; }
                else value = startLevel + (peak - startLevel) * (pos / a);
                break;
            }
            case Hold:
                pos += dt; value = peak;
                if (pos >= hold * timeScale) { stage = Decay; pos = 0; }
                break;
            case Decay:
            {
                const float d = std::max (1.0e-4f, decay * timeScale);
                pos += dt;
                const float t = clamp01 (pos / d);
                // curve 0 = lineare, 1 = esponenziale "naturale"
                const float lin = 1.0f - t;
                const float ex = (std::exp (-6.9f * t) - 0.001f) / 0.999f;
                value = peak * std::max (0.0f, lerp (lin, ex, clamp01 (curve)));
                if (t >= 1.0f) { stage = Idle; value = 0; }
                break;
            }
        }
        return value;
    }
};

// LFO multimodo con modalità di reset
struct Lfo
{
    enum Wave { Wavetable, TriTilt, Pulse, SmoothRandom };
    enum Reset { FreeRun, AlwaysReset, SoftReset, SampleHold, OneShot };
    double phase = 0;
    float shape = 0.5f, amp = 1.0f;
    bool bipolar = true;
    int wave = Wavetable, resetMode = FreeRun;
    float held = 0, rndA = 0, rndB = 0;
    bool oneShotDone = false;
    Rng rng { 0xBEEF };

    void onTransportStart() { phase = 0; oneShotDone = false; }
    void onTrigger (bool voiceWasSilent)
    {
        switch (resetMode)
        {
            case AlwaysReset: phase = 0; break;
            case SoftReset: if (voiceWasSilent) phase = 0; break;
            case SampleHold: held = raw(); break;
            case OneShot: phase = 0; oneShotDone = false; break;
            default: break;
        }
    }

    float raw() const
    {
        const float p = (float) phase;
        switch (wave)
        {
            case Wavetable:
            {
                // morph sine → triangle → saw → square → random
                const float s = clamp01 (shape) * 4.0f;
                const float sine = std::sin (kTwoPi * p);
                const float tri = 1.0f - 4.0f * std::abs (p - 0.5f);
                const float tri2 = (p < 0.25f) ? p * 4.0f : (p < 0.75f ? 2.0f - p * 4.0f : p * 4.0f - 4.0f);
                (void) tri;
                const float saw = 2.0f * p - 1.0f;
                const float sq = p < 0.5f ? 1.0f : -1.0f;
                const float rnd = rndA;
                if (s < 1) return lerp (sine, tri2, s);
                if (s < 2) return lerp (tri2, saw, s - 1);
                if (s < 3) return lerp (saw, sq, s - 2);
                return lerp (sq, rnd, s - 3);
            }
            case TriTilt:
            {
                const float k = clampf (shape, 0.001f, 0.999f);
                return p < k ? (2.0f * p / k - 1.0f) : (1.0f - 2.0f * (p - k) / (1.0f - k));
            }
            case Pulse: return p < clampf (shape, 0.02f, 0.98f) ? 1.0f : -1.0f;
            case SmoothRandom:
            {
                const float sm = clamp01 (shape);
                const float t = sm <= 0.001f ? 1.0f : clamp01 (p / sm);
                const float tt = t * t * (3 - 2 * t);
                return lerp (rndB, rndA, tt);
            }
            default: return 0;
        }
    }

    // avanza di dt secondi alla frequenza hz; restituisce valore già scalato (amp, uni/bipolare)
    float advance (float dt, float hz)
    {
        if (! std::isfinite (phase)) phase = 0;              // (uno stato guasto non resta per sempre)
        if (! std::isfinite (hz)) hz = 0;
        if (! (resetMode == OneShot && oneShotDone))
        {
            phase += (double) hz * dt;
            if (phase >= 1.0)
            {
                phase -= std::floor (phase);
                rndB = rndA; rndA = rng.bi();
                if (resetMode == OneShot) { oneShotDone = true; phase = 0.9999; }
            }
        }
        float v = (resetMode == SampleHold) ? held : raw();
        if (! bipolar) v = 0.5f * (v + 1.0f);
        return v * amp;
    }
};
} // namespace mp
