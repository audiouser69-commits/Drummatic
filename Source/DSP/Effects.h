#pragma once
#include "Common.h"

namespace mp
{
// ---------- Tilt EQ (-1..1): negativo scurisce, positivo schiarisce ----------
struct Tilt
{
    OnePole lp;
    float process (float x, float tilt, float sr)
    {
        lp.setCutoff (800.0f, sr);
        const float lo = lp.lp (x), hi = x - lo;
        const float t = clampf (tilt, -1, 1);
        return lo * (1.0f - std::max (0.0f, t)) * (1.0f + std::max (0.0f, -t) * 0.5f)
             + hi * (1.0f + std::min (0.0f, t)) * (1.0f + std::max (0.0f, t) * 0.5f);
    }
};

// ---------- SHATTER DELAY ----------
// Ritardo "istantaneo": quando il tempo cambia, crossfade fra due testine (niente glissati di pitch)
struct ShatterDelay
{
    float sr = 48000;
    DelayLine dl[2];
    float curTime[2] { 0.25f, 0.25f }, prevTime[2] { 0.25f, 0.25f }, xf[2] { 1, 1 };
    float glitchState[2] {}, glitchVel[2] {};
    float duckEnv = 0, srAcc = 0, heldL = 0, heldR = 0;
    Tilt tiltL, tiltR; Rng rng { 0xAB1E };

    void prepare (float sampleRate) { sr = sampleRate; for (auto& d : dl) d.init ((int) (sr * 4.2f)); }
    void reset() { for (auto& d : dl) d.clear(); duckEnv = 0; }

    // time in secondi, feedback 0..1, glitch 0..1, fbTilt -1..1, ducking 0..1, srate 0..1, gain lineare
    void process (float inL, float inR, float& outL, float& outR, float timeSec, float feedback, float glitch,
                  float fbTilt, float ducking, float srate, float gain, bool stereo, bool pong)
    {
        // S.RATE: tutto il DSP rallenta (sample & hold dell'ingresso + lettura scalata)
        const float rate = lerp (0.08f, 1.0f, srate);
        srAcc += rate;
        const bool tick = srAcc >= 1.0f;
        if (tick) srAcc -= 1.0f;

        float target[2];
        for (int c = 0; c < 2; ++c)
        {
            if (c == 0 || stereo)
            {
                glitchVel[c] = glitchVel[c] * 0.9995f + rng.bi() * 0.00008f;
                glitchState[c] = clampf (glitchState[c] + glitchVel[c], -1, 1);
            }
            else glitchState[1] = glitchState[0];
            target[c] = clampf (timeSec * (1.0f + glitch * glitch * glitchState[c] * 0.9f) / rate, 0.001f, 4.0f);
            if (std::abs (target[c] - curTime[c]) > 0.002f && xf[c] >= 1.0f)
            { prevTime[c] = curTime[c]; curTime[c] = target[c]; xf[c] = 0; }
            if (xf[c] < 1.0f) xf[c] = std::min (1.0f, xf[c] + 1.0f / (0.03f * sr));
        }
        float wet[2];
        for (int c = 0; c < 2; ++c)
            wet[c] = dl[c].read (prevTime[c] * sr) * (1.0f - xf[c]) + dl[c].read (curTime[c] * sr) * xf[c];

        const float lvl = std::max (std::abs (inL), std::abs (inR));
        duckEnv = lvl > duckEnv ? lvl : duckEnv * 0.9997f;
        const float duck = 1.0f / (1.0f + ducking * duckEnv * 8.0f);

        if (tick) { heldL = inL; heldR = inR; }
        const float fb = std::min (0.98f, feedback * 1.02f);
        const float fL = tiltL.process (pong ? wet[1] : wet[0], fbTilt, sr);
        const float fR = tiltR.process (pong ? wet[0] : wet[1], fbTilt, sr);
        dl[0].push (fastTanh (heldL + fL * fb));
        dl[1].push (fastTanh (heldR + fR * fb));
        outL = wet[0] * duck * gain;
        outR = wet[1] * duck * gain;
    }
};

// ---------- HEADSPACE REVERB (FDN 8 linee) ----------
struct HeadspaceReverb
{
    static constexpr int N = 8;
    float sr = 48000;
    DelayLine pre, line[N]; OnePole damp[N]; float lens[N] {};
    Tilt inTilt, outTiltL, outTiltR;
    float srAcc = 0, held = 0, lfoPh = 0;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        pre.init ((int) (sr * 2.1f));
        static const float base[N] = { 0.0297f, 0.0371f, 0.0411f, 0.0437f, 0.0503f, 0.0571f, 0.0613f, 0.0683f };
        for (int i = 0; i < N; ++i) { lens[i] = base[i]; line[i].init ((int) (sr * base[i] * 4.0f) + 64); }
    }
    void reset() { pre.clear(); for (auto& l : line) l.clear(); }

    // mode: 0 Dark, 1 Bright, 2 Hall, 3 Tin
    void process (float inL, float inR, float& outL, float& outR, float predelaySec, float decaySec,
                  float srate, float inTiltV, float outTiltV, float gain, int mode)
    {
        const float rate = lerp (0.1f, 1.0f, srate);
        srAcc += rate;
        const float in = inTilt.process ((inL + inR) * 0.5f, inTiltV, sr);
        if (srAcc >= 1.0f) { srAcc -= 1.0f; held = in; }
        pre.push (held);
        const float x = pre.read (std::max (1.0f, predelaySec * sr));

        const float size = (mode == 3 ? 0.25f : (mode == 2 ? 1.6f : 1.0f)) / rate;
        const float dampHz = mode == 0 ? 3500.0f : (mode == 1 ? 12000.0f : (mode == 2 ? 6000.0f : 9000.0f));
        lfoPh += 0.4f / sr; if (lfoPh > 1) lfoPh -= 1;
        float o[N];
        for (int i = 0; i < N; ++i)
        {
            const float mod = 1.0f + 0.003f * std::sin (kTwoPi * (lfoPh + i * 0.125f));
            const float d = clampf (lens[i] * size * mod * sr, 4.0f, (float) line[i].buf.size() - 4);
            damp[i].setCutoff (dampHz, sr);
            o[i] = damp[i].lp (line[i].read (d));
        }
        // matrice di Householder
        float sum = 0; for (float v : o) sum += v;
        sum *= 2.0f / N;
        const float t60 = std::max (0.05f, decaySec);
        for (int i = 0; i < N; ++i)
        {
            const float g = std::pow (10.0f, -3.0f * (lens[i] * size) / t60);
            line[i].push (fastTanh ((o[i] - sum) * g + x * 0.35f));
        }
        const float l = (o[0] + o[2] + o[4] + o[6]) * 0.5f, r = (o[1] + o[3] + o[5] + o[7]) * 0.5f;
        outL = outTiltL.process (l, outTiltV, sr) * gain;
        outR = outTiltR.process (r, outTiltV, sr) * gain;
    }
};

// ---------- MASTER ----------
enum ClipMode { NoClip, SoftClip, HardClip, FoldClip };
struct MasterBus
{
    float sr = 48000, env = 0;
    Svf lowL, lowR, midL, midR, highL, highR;
    void prepare (float s) { sr = s; }
    void reset() { env = 0; for (auto* f : { &lowL, &lowR, &midL, &midR, &highL, &highR }) f->reset(); }
    void process (float& L, float& R, float maximize, float lowDb, float midDb, float highDb, float gainDb, int clip)
    {
        // MAXIMIZE: compressore aggressivo con make-up (alza i segnali deboli e le code)
        if (maximize > 0.001f)
        {
            const float lvl = std::max (std::abs (L), std::abs (R));
            const float att = 0.002f, rel = lerp (0.15f, 0.05f, maximize);
            const float c = lvl > env ? std::exp (-1.0f / (att * sr)) : std::exp (-1.0f / (rel * sr));
            env = lvl + c * (env - lvl);
            const float thr = dbToGain (-30.0f * maximize);
            float g = env > thr ? std::pow (thr / env, 0.8f) : 1.0f;
            g *= dbToGain (24.0f * maximize * maximize + 6.0f * maximize);
            L *= g; R *= g;
        }
        // EQ 3 bande: shelf bassi 150 Hz, campana 1 kHz, shelf alti 6 kHz (approssimazioni SVF)
        auto band = [this] (Svf& f, float x, float hz, float db, int type)
        {
            if (std::abs (db) < 0.05f) return x;
            f.set (hz, 0.7f, sr); f.process (x);
            const float g = dbToGain (db) - 1.0f;
            return x + g * (type == 0 ? f.lpOut : (type == 1 ? f.bpOut : f.hpOut));
        };
        L = band (lowL, L, 150, lowDb, 0);  R = band (lowR, R, 150, lowDb, 0);
        L = band (midL, L, 1000, midDb, 1); R = band (midR, R, 1000, midDb, 1);
        L = band (highL, L, 6000, highDb, 2); R = band (highR, R, 6000, highDb, 2);

        const float g = dbToGain (gainDb);
        L *= g; R *= g;
        auto clipf = [clip] (float x)
        {
            switch (clip)
            {
                case SoftClip: return fastTanh (x);
                case HardClip: return clampf (x, -1.0f, 1.0f);
                case FoldClip:
                {
                    for (int i = 0; i < 6 && (x > 1.0f || x < -1.0f); ++i) x = x > 1.0f ? 2.0f - x : -2.0f - x;
                    return x;
                }
                default: return x;
            }
        };
        L = clipf (L); R = clipf (R);
    }
};
} // namespace mp
