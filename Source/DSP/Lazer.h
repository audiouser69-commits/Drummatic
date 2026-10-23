#pragma once
// Drummatic — strato LAZER: 64 motori glitch in 16 famiglie con 12 manopole condivise,
// 9 code (TAIL), BRK TAPZ (multi-tap che accelera/rallenta) e BRK REPEAT (stutter con buchi, reverse, crush).
// C++ puro, nessuna dipendenza da JUCE.
#include "Common.h"

namespace mp
{
constexpr int kLzFamilies = 16, kLzVariants = 4, kNumLzEngines = kLzFamilies * kLzVariants;
constexpr int kLzKnobs = 12;
enum LzKnob { LK_Tune, LK_Sweep, LK_STime, LK_Timbre, LK_Color, LK_Mod, LK_Rate, LK_Grit, LK_Density, LK_Spread, LK_Decay, LK_Level };

inline const char* lzFamilyName (int f)
{
    static const char* n[kLzFamilies] = { "FM Metals", "Laser Sweeps", "Wavefold Zaps", "Grain Clouds", "Sub Thumps", "Bit Zaps",
                                          "Ring Mods", "Noise Bursts", "Comb Pings", "Chirps", "Formants", "Glitch Steps",
                                          "Data Corrupt", "Phase Distort", "Shimmer Drones", "Static & Radio" };
    return n[std::min (std::max (f, 0), kLzFamilies - 1)];
}
inline const char* lzEngineName (int e)
{
    static const char* n[kNumLzEngines] = {
        "Bell Metal", "Gong", "Anvil", "Tin Can",
        "Laser Down", "Laser Up", "Laser Bounce", "Laser Zigzag",
        "Fold Zap", "Tri Fold", "Double Fold", "FM Fold",
        "Sine Cloud", "Dust Cloud", "Blip Cloud", "Chord Cloud",
        "808 Sub", "Square Sub", "Double Thump", "Pulse Sub",
        "Bit Laser", "Rate Sweep", "Byte Crush", "8-Bit Blip",
        "Ring Sine", "Ring Saw", "Ring Square", "Ring Noise",
        "Band Burst", "Low Burst", "High Burst", "Comb Burst",
        "Pluck Ping", "Metal Ping", "Bowed Ping", "Twin Comb",
        "Accel Chirp", "Decel Chirp", "Geiger", "Bounce Chirp",
        "Vowel Zap", "Talk Box", "Choir Blip", "Robot",
        "S&H Steps", "Scale Steps", "Octave Steps", "Stutter Steps",
        "Bytebeat A", "Bytebeat B", "Bytebeat C", "Corrupt Tape",
        "PD Saw", "PD Square", "PD Reso", "PD Pulse",
        "Fifth Drone", "Octave Drone", "Minor Drone", "Cluster Drone",
        "Vinyl Dust", "Shortwave", "Hum & Buzz", "Modem" };
    return n[std::min (std::max (e, 0), kNumLzEngines - 1)];
}
// famiglie adatte a un tappeto di fondo (suono lungo): suggerite per l'atmosfera
inline bool lzIsAtmos (int e) { const int f = e / kLzVariants; return f == 3 || f == 14 || f == 15 || e == 34; }

// Valori delle 12 manopole già convertiti (tune in semitoni, sweep -1..1, tempi in secondi, il resto 0..1)
struct LzParams { float k[kLzKnobs] {}; };

inline float lzFold (float x)
{
    // wavefolder a seno: ripiega senza esplodere
    return std::sin (x * kPi * 0.5f);
}

// ======================================================================= motore
struct LazerEngine
{
    float sr = 48000; Rng rng { 0x1A2E5u };
    int engine = 4;
    // stato
    double ph[6] {};
    float sweepT = 0;          // secondi dal colpo
    float t = 0;               // secondi dal colpo
    float last = 0, held = 0, heldR = 0, acc = 0, acc2 = 0, lastStep = 0;
    float vel = 1;
    Svf f1, f2, f3, f1r;
    DelayLine comb, comb2;
    float combLp = 0, combLp2 = 0, excite = 0;
    uint32_t bt = 0; float btAcc = 0;
    float pulseTimer = 0, pulseEnv = 0, pulseHz = 0; int pulseCount = 0;
    struct Grain { bool on = false; float ph = 0, inc = 0, pos = 0, len = 1, pan = 0, amp = 0; Svf bp; int kind = 0; };
    static constexpr int kGrains = 16;
    Grain grains[kGrains]; float grainTimer = 0;
    float driftA = 0, driftB = 0;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        comb.init ((int) (sr * 0.06f)); comb2.init ((int) (sr * 0.06f));
    }
    void trigger (int eng, float velocity)
    {
        engine = std::min (std::max (eng, 0), kNumLzEngines - 1);
        vel = velocity; t = 0; sweepT = 0;
        for (auto& p : ph) p = 0;
        last = 0; pulseTimer = 0; pulseEnv = 0; pulseCount = 0; grainTimer = 0;
        excite = 1.0f; bt = 0; btAcc = 0; acc = 1.0f; held = 0; heldR = 1.0f;
        const int fam = engine / kLzVariants;
        // le famiglie "di fondo" non ripartono da zero: continuano il tappeto
        if (fam != 3 && fam != 14 && fam != 15) { comb.clear(); comb2.clear(); combLp = combLp2 = 0; }
    }

    static float phStep (double& p, float hz, float sr)
    {
        p += (double) hz / sr; p -= std::floor (p); return (float) p;
    }
    static float sawOf (float p) { return 2.0f * p - 1.0f; }
    static float sqOf (float p, float pw = 0.5f) { return p < pw ? 1.0f : -1.0f; }
    static float triOf (float p) { return 4.0f * std::abs (p - 0.5f) - 1.0f; }

    // inviluppo di sweep (1 → 0 in STIME): forma per variante
    float sweepEnv (const LzParams& P, int shape)
    {
        const float st = std::max (0.001f, P.k[LK_STime]);
        const float x = sweepT / st;
        switch (shape)
        {
            case 1: return std::max (0.0f, 1.0f - x);                                 // lineare
            case 2: { const float b = std::abs (std::cos (kPi * x * 0.5f * (1.0f + P.k[LK_Rate] * 6.0f))); return b * std::exp (-x); } // rimbalzo
            case 3: return std::floor (std::max (0.0f, 1.0f - x) * (3.0f + P.k[LK_Rate] * 13.0f)) / (3.0f + P.k[LK_Rate] * 13.0f); // a gradini
            default: return std::exp (-4.6f * x);                                     // esponenziale
        }
    }
    float sweepMul (const LzParams& P, int shape = 0)
    {
        const float e = sweepEnv (P, shape);
        return std::pow (2.0f, P.k[LK_Sweep] * 4.0f * e);      // ±4 ottave
    }

    // un campione stereo: f = frequenza base (già trasposta)
    void process (float f, const LzParams& P, float& outL, float& outR)
    {
        const float dt = 1.0f / sr;
        t += dt; sweepT += dt;
        const int fam = engine / kLzVariants, v = engine % kLzVariants;
        const float T = P.k[LK_Timbre], C = P.k[LK_Color], M = P.k[LK_Mod], R = P.k[LK_Rate], D = P.k[LK_Density];
        const float rateHz = 0.1f * std::pow (400.0f, R);    // 0.1 .. 40 Hz
        float x = 0, y = 0; bool stereo = false;
        switch (fam)
        {
            case 0: // ---------------- FM METALS
            {
                static const float r1[4] = { 1.414f, 2.756f, 3.17f, 7.03f }, r2[4] = { 3.73f, 1.53f, 5.43f, 11.3f };
                const float sw = sweepMul (P);
                const float idxEnv = std::exp (-t * (1.0f + R * 30.0f));
                const float idx = T * 10.0f * (0.25f + 0.75f * idxEnv) + M * 3.0f;
                const float fc = f * 2.0f * sw;
                const float m2 = std::sin (kTwoPi * phStep (ph[2], fc * r2[v] * (1.0f + C * 0.5f), sr) + last * P.k[LK_Grit] * 2.0f);
                const float m1 = std::sin (kTwoPi * phStep (ph[1], fc * r1[v], sr) + m2 * idx * D);
                x = std::sin (kTwoPi * phStep (ph[0], fc, sr) + m1 * idx);
                last = x;
                y = std::sin (kTwoPi * phStep (ph[3], fc * 1.0031f, sr) + m1 * idx * 0.97f);
                stereo = true;
                break;
            }
            case 1: // ---------------- LASER SWEEPS
            {
                // Laser Up: stessa curva lineare ma verso l'alto (lo SWEEP negativo predefinito diventa una salita)
                const float swL = v == 1 ? 1.0f / sweepMul (P, 1) : sweepMul (P, v);
                const float fl = f * 2.0f * swL * (1.0f + M * 0.3f * std::sin (kTwoPi * phStep (ph[5], rateHz * 4.0f, sr)));
                const int voices = 1 + (int) (D * 2.99f);
                float s = 0, s2 = 0;
                for (int k = 0; k < voices; ++k)
                {
                    const float det = 1.0f + (float) k * 0.007f * (1.0f + C);
                    const float p = phStep (ph[k], std::min (fl * det, sr * 0.45f), sr);
                    const float sn = std::sin (kTwoPi * p), sw = sawOf (p) - polyBlep (p, fl * det / sr), sq = sqOf (p);
                    const float w = T < 0.5f ? lerp (sn, sw, T * 2.0f) : lerp (sw, sq, (T - 0.5f) * 2.0f);
                    if (k % 2 == 0) s += w; else s2 += w;
                }
                x = s / (float) voices * 1.4f; y = voices > 1 ? s2 / (float) voices * 1.4f + x * 0.3f : x;
                f1.set (300.0f * std::pow (60.0f, C), 1.2f, sr); f1.process (x); x = f1.lpOut;
                f1r.set (300.0f * std::pow (60.0f, C), 1.2f, sr); f1r.process (y); y = f1r.lpOut;
                stereo = true;
                break;
            }
            case 2: // ---------------- WAVEFOLD ZAPS
            {
                const float sw = sweepMul (P);
                float carrier;
                const float p = phStep (ph[0], f * sw, sr);
                if (v == 1) carrier = triOf (p);
                else if (v == 3) carrier = std::sin (kTwoPi * p + std::sin (kTwoPi * phStep (ph[1], f * sw * (1.0f + C * 3.0f), sr)) * M * 4.0f);
                else carrier = std::sin (kTwoPi * p);
                const float fe = std::exp (-t * (2.0f + R * 40.0f));
                const float g = 1.0f + T * 12.0f * (1.0f - M + M * fe);
                const float bias = (v == 3 ? 0.0f : C - 0.5f) * 0.8f;
                x = lzFold (carrier * g + bias);
                if (v == 2) x = lzFold (x * (1.5f + T * 4.0f));
                x *= 0.9f; y = x;
                break;
            }
            case 3: // ---------------- GRAIN CLOUDS (tappeto)
            {
                const float rate = 4.0f * std::pow (100.0f, D);              // 4 .. 400 grani/s
                grainTimer -= dt;
                if (grainTimer <= 0)
                {
                    grainTimer = 1.0f / rate * (0.5f + rng.uni());
                    for (auto& g : grains)
                        if (! g.on)
                        {
                            g.on = true; g.pos = 0; g.len = 0.005f + T * T * 0.25f;
                            static const float chord[] = { 0, 3, 7, 10, 12, 15, 19, 24 };
                            float st = rng.bi() * M * 24.0f;
                            if (v == 3) st = chord[rng.next() % 8] + (rng.uni() < M ? 12.0f : 0.0f);
                            const float drift = std::sin (t * rateHz * 0.25f) * R * 5.0f;
                            g.inc = f * 2.0f * semis (st + drift) * sweepMul (P) / sr;
                            g.ph = rng.uni(); g.pan = rng.bi() * P.k[LK_Spread];
                            g.amp = 0.4f + 0.6f * rng.uni(); g.kind = v;
                            g.bp.set (std::min (sr * 0.4f, 300.0f * std::pow (40.0f, C) * (0.5f + rng.uni())), 4.0f, sr);
                            break;
                        }
                }
                float l = 0, r = 0;
                for (auto& g : grains)
                {
                    if (! g.on) continue;
                    g.pos += dt;
                    const float u = g.pos / g.len;
                    if (u >= 1.0f) { g.on = false; continue; }
                    const float win = std::sin (kPi * u); // finestra
                    g.ph += g.inc; g.ph -= std::floor (g.ph);
                    float s;
                    if (g.kind == 1) { g.bp.process (rng.bi()); s = g.bp.bpOut * 3.0f; }
                    else if (g.kind == 2) s = std::sin (kTwoPi * g.ph + std::sin (kTwoPi * g.ph * 3.01f) * 2.0f * (1.0f - u));
                    else s = std::sin (kTwoPi * g.ph);
                    s *= win * win * g.amp;
                    l += s * (1.0f - g.pan); r += s * (1.0f + g.pan);
                }
                x = l * 0.5f; y = r * 0.5f; stereo = true;
                break;
            }
            case 4: // ---------------- SUB THUMPS
            {
                float tt = t;
                if (v == 2) { const float gap = 0.04f + R * 0.3f; if (t > gap) tt = t - gap; }
                const float drop = std::exp (-tt / (0.004f + P.k[LK_STime] * 0.15f));
                const float hz = f * 0.5f * (1.0f + (1.0f + P.k[LK_Sweep]) * 3.0f * drop);
                const float p = phStep (ph[0], hz, sr);
                float s = v == 1 ? fastTanh (std::sin (kTwoPi * p) * (2.0f + T * 8.0f)) : std::sin (kTwoPi * p);
                if (v == 3) s *= 0.6f + 0.4f * std::sin (kTwoPi * phStep (ph[1], rateHz, sr));
                if (v == 2 && std::abs (t - tt) > 0 && tt < 0.002f) s += rng.bi() * 0.4f;
                const float click = std::exp (-t * 900.0f) * T * rng.bi();
                x = fastTanh ((s + click) * (1.0f + C * 3.0f)) * 0.95f; y = x;
                break;
            }
            case 5: // ---------------- BIT ZAPS
            {
                const float sw = sweepMul (P, v == 1 ? 1 : 0);
                const float p = phStep (ph[0], f * 2.0f * sw, sr);
                float s = v == 3 ? sqOf (p, 0.125f + T * 0.375f) : (T < 0.5f ? sqOf (p) : sawOf (p));
                // riduzione della frequenza di campionamento (che spazza con lo sweep nella variante 1)
                float rf = 0.02f + 0.98f * (1.0f - C);
                if (v == 1) rf *= sweepEnv (P, 0) * 0.9f + 0.1f;
                acc += rf; if (acc >= 1.0f) { acc -= 1.0f; held = s; }
                float bits = 2.0f + (1.0f - P.k[LK_Grit]) * 10.0f;
                if (v == 2) bits = 1.0f + (1.0f - M) * 4.0f;
                const float q = std::pow (2.0f, bits);
                x = std::round (held * q) / q * 0.7f; y = x;
                break;
            }
            case 6: // ---------------- RING MODS
            {
                const float sw = sweepMul (P), inv = 1.0f / std::max (0.25f, sw);
                const float pa = phStep (ph[0], f * 2.0f * sw, sr);
                const float ratio = 0.5f + T * 7.5f;
                const float pb = phStep (ph[1], f * 2.0f * ratio * (M > 0 ? lerp (1.0f, inv, M) : 1.0f), sr);
                float a, b;
                switch (v)
                {
                    case 1: a = sawOf (pa); b = sawOf (pb); break;
                    case 2: a = sqOf (pa); b = sqOf (pb); break;
                    case 3: { f2.set (f * 2.0f * ratio, 6.0f, sr); f2.process (rng.bi()); a = std::sin (kTwoPi * pa); b = f2.bpOut * 4.0f; break; }
                    default: a = std::sin (kTwoPi * pa); b = std::sin (kTwoPi * pb); break;
                }
                x = a * b;
                f1.set (400.0f * std::pow (40.0f, C), 0.8f, sr); f1.process (x); x = f1.lpOut * 0.9f; y = x;
                break;
            }
            case 7: // ---------------- NOISE BURSTS
            {
                const float n = rng.bi();
                const float cut = std::min (sr * 0.45f, 200.0f * std::pow (50.0f, C) * sweepMul (P)
                                                    * (1.0f + M * 0.8f * std::sin (kTwoPi * phStep (ph[0], rateHz * 2.0f, sr))));
                const float q = 0.7f + T * 14.0f;
                if (v == 3)
                {
                    comb.push (n * 0.3f + comb.read (sr / std::max (40.0f, cut * 0.05f)) * (0.6f + T * 0.38f));
                    x = comb.read (sr / std::max (40.0f, cut * 0.05f));
                }
                else
                {
                    f1.set (cut, q, sr); f1.process (n);
                    x = v == 0 ? f1.bpOut * 1.8f : (v == 1 ? f1.lpOut : f1.hpOut * 0.8f);
                }
                f1r.set (cut * 1.02f, q, sr); f1r.process (rng.bi());
                y = v == 3 ? x : (v == 0 ? f1r.bpOut * 1.8f : (v == 1 ? f1r.lpOut : f1r.hpOut * 0.8f));
                stereo = true;
                break;
            }
            case 8: // ---------------- COMB PINGS (Karplus / risonatori)
            {
                const float hz = std::max (30.0f, f * 2.0f * sweepMul (P));
                const float fb = 0.90f + T * 0.0995f;
                const float damp = 0.3f + C * 0.69f;
                float exc = 0;
                if (v == 2) exc = rng.bi() * 0.25f * (0.3f + M);                  // archetto: eccitazione continua (lungo)
                else { exc = excite * rng.bi(); excite *= 0.995f - D * 0.05f; if (excite < 1e-4f) excite = 0; }
                const float d1 = sr / hz;
                float r1 = comb.read (d1);
                combLp += damp * (r1 - combLp);
                float fbSig = combLp;
                if (v == 1) fbSig = fbSig - 0.6f * comb.read (d1 * 0.731f);       // dispersione metallica
                comb.push (fastTanh (exc + fbSig * fb));
                x = r1;
                if (v == 3)
                {
                    const float d2 = sr / (hz * (1.0f + M * 1.5f + 0.13f));
                    const float r2 = comb2.read (d2);
                    combLp2 += damp * (r2 - combLp2);
                    comb2.push (fastTanh (exc * 0.7f + combLp2 * fb));
                    y = r2; stereo = true;
                }
                else y = x;
                break;
            }
            case 9: // ---------------- CHIRPS (treni di impulsi)
            {
                float rate = 3.0f * std::pow (60.0f, R);                           // 3..180 Hz
                const float e = sweepEnv (P, 0);
                if (v == 0) rate *= 1.0f + (1.0f - e) * 6.0f * (0.2f + D);       // accelera
                else if (v == 1) rate *= 0.15f + e * (1.0f + D * 4.0f);          // rallenta
                else if (v == 3) rate *= 0.3f + std::abs (std::cos (t * 3.0f)) * (1.0f + D * 6.0f);
                pulseTimer -= dt;
                if (pulseTimer <= 0)
                {
                    pulseTimer = v == 2 ? -std::log (std::max (1e-4f, rng.uni())) / rate : 1.0f / rate;
                    pulseEnv = 1.0f; ph[0] = 0; pulseHz = f * 4.0f * semis (rng.bi() * M * 12.0f);
                }
                pulseEnv *= std::exp (-dt / (0.0015f + T * 0.02f));
                const float chirp = pulseHz * (1.0f + 3.0f * pulseEnv * (P.k[LK_Sweep] + 1.0f));
                x = std::sin (kTwoPi * phStep (ph[0], std::min (chirp, sr * 0.45f), sr)) * pulseEnv;
                f1.set (500.0f * std::pow (30.0f, C), 0.7f, sr); f1.process (x); x = lerp (x, f1.lpOut, 0.5f) * 1.2f; y = x;
                break;
            }
            case 10: // ---------------- FORMANTS
            {
                static const float F1[5] = { 730, 530, 270, 570, 300 }, F2[5] = { 1090, 1840, 2290, 840, 870 }, F3[5] = { 2440, 2480, 3010, 2410, 2240 };
                float vow = T * 4.0f;
                if (v == 1 || v == 3) vow = clampf (vow + std::sin (kTwoPi * phStep (ph[2], rateHz, sr)) * M * 2.0f, 0.0f, 3.999f);
                else vow = clampf (vow + (1.0f - sweepEnv (P, 0)) * M * 3.0f, 0.0f, 3.999f);
                const int i0 = (int) vow; const float fr = vow - (float) i0;
                const float sw = sweepMul (P);
                const float p = phStep (ph[0], f * sw * (v == 2 ? 2.0f : 1.0f), sr);
                float src = v == 3 ? sqOf (p, 0.1f) : sawOf (p) - polyBlep (p, f * sw / sr);
                if (v == 2) src += sawOf (phStep (ph[1], f * sw * 2.006f, sr));
                const float sh = 0.7f + C * 0.8f;
                f1.set (lerp (F1[i0], F1[i0 + 1], fr) * sh, 8, sr); f2.set (lerp (F2[i0], F2[i0 + 1], fr) * sh, 10, sr); f3.set (lerp (F3[i0], F3[i0 + 1], fr) * sh, 12, sr);
                f1.process (src); f2.process (src); f3.process (src);
                x = (f1.bpOut + f2.bpOut * 0.6f + f3.bpOut * 0.35f) * 0.8f; y = x;
                break;
            }
            case 11: // ---------------- GLITCH STEPS (sample & hold)
            {
                acc += rateHz * 4.0f / sr;
                if (acc >= 1.0f)
                {
                    acc -= std::floor (acc);
                    float st = rng.bi() * (2.0f + M * 34.0f);
                    if (v == 1) { static const int sc[] = { 0, 2, 3, 5, 7, 8, 10 }; const int o = (int) std::floor (st / 12.0f); st = (float) (o * 12 + sc[(int) std::abs (st) % 7]); }
                    if (v == 2) st = std::round (st / 12.0f) * 12.0f;
                    if (v == 3 && rng.uni() < 0.5f) st = lastStep;                 // stutter: ripete il passo
                    lastStep = st; held = semis (st); heldR = (t > 0.01f && rng.uni() < D * 0.6f) ? 0.0f : 1.0f;
                }
                const float p = phStep (ph[0], f * 2.0f * held * sweepMul (P), sr);
                float s = T < 0.33f ? std::sin (kTwoPi * p) : (T < 0.66f ? triOf (p) : sqOf (p));
                f1.set (300.0f * std::pow (50.0f, C), 1.0f, sr); f1.process (s * heldR);
                x = f1.lpOut * 1.6f; y = x;
                break;
            }
            case 12: // ---------------- DATA CORRUPT (bytebeat)
            {
                btAcc += f * 2.0f * sweepMul (P) * (0.5f + D * 15.5f) / 128.0f * 8000.0f / sr * 16.0f;
                while (btAcc >= 1.0f) { btAcc -= 1.0f; ++bt; }
                const uint32_t a = 1 + (uint32_t) (T * 9.0f), b = 3 + (uint32_t) (C * 9.0f), c = 1 + (uint32_t) (M * 15.0f);
                uint32_t val;
                switch (v)
                {
                    case 1: val = (bt * (bt >> a & bt >> b)) | (bt >> c); break;
                    case 2: val = (bt * ((bt >> b | bt >> a) & (c * 4 + 3))) ^ (bt >> 6); break;
                    case 3: val = (uint32_t) ((bt >> (a % 4)) * (rng.uni() < R * 0.02f ? 0u : 1u)) ^ (bt * c); break;
                    default: val = bt * ((bt >> a | bt >> b) & c & (bt >> 4)); break;
                }
                const float s = (float) (val & 255u) / 127.5f - 1.0f;
                f1.set (std::min (sr * 0.45f, 1500.0f * std::pow (10.0f, 1.0f - P.k[LK_Grit] * 0.5f)), 0.7f, sr); f1.process (s);
                x = f1.lpOut * 0.7f; y = x;
                break;
            }
            case 13: // ---------------- PHASE DISTORT (stile CZ)
            {
                const float sw = sweepMul (P);
                const float fe = std::exp (-t * (1.0f + R * 30.0f));
                const float amt = clamp01 (T * (1.0f - M + M * fe)) * 0.98f;
                const float p = phStep (ph[0], f * 2.0f * sw, sr);
                const float d = 0.5f - amt * 0.49f;
                float warped;
                if (v == 1) warped = p < 0.5f ? (p < d ? p / d * 0.25f : 0.25f + (p - d) / (0.5f - d) * 0.25f)
                                              : (p - 0.5f < d ? 0.5f + (p - 0.5f) / d * 0.25f : 0.75f + (p - 0.5f - d) / (0.5f - d) * 0.25f);
                else warped = p < d ? p / d * 0.5f : 0.5f + (p - d) / (1.0f - d) * 0.5f;
                float s = std::cos (kTwoPi * warped);
                if (v == 2) { const float rp = phStep (ph[1], f * 2.0f * sw * (1.0f + amt * 14.0f), sr); s = std::cos (kTwoPi * rp) * (1.0f - p); }
                if (v == 3) s = std::cos (kTwoPi * warped) * sqOf (phStep (ph[1], f * sw * (1.0f + C * 3.0f), sr), 0.25f + C * 0.5f);
                x = s * 0.8f; y = x;
                break;
            }
            case 14: // ---------------- SHIMMER DRONES (tappeto)
            {
                static const float iv[4][4] = { { 0, 7, 12, 19 }, { 0, 12, 24, -12 }, { 0, 3, 7, 15 }, { 0, 1, 5, 6 } };
                driftA += dt * rateHz * 0.05f; driftB += dt * rateHz * 0.037f;
                float l = 0, r = 0;
                const float voicesF = 2.0f + D * 4.0f;
                for (int k = 0; k < 6; ++k)
                {
                    if ((float) k >= voicesF) break;
                    const float st = iv[v][k % 4] + (k >= 4 ? 12.0f : 0.0f);
                    const float det = 1.0f + 0.004f * std::sin (kTwoPi * (driftA + k * 0.17f)) * (0.3f + M);
                    const float p = phStep (ph[k], f * semis (st) * det * sweepMul (P), sr);
                    const float w = lerp (std::sin (kTwoPi * p), sawOf (p) - polyBlep (p, f * semis (st) / sr), T);
                    const float shimmer = 1.0f - M * 0.5f * (0.5f + 0.5f * std::sin (kTwoPi * (driftB * 3.0f + k * 0.31f)));
                    if (k % 2 == 0) l += w * shimmer; else r += w * shimmer;
                }
                f1.set (200.0f * std::pow (60.0f, C), 0.9f, sr); f1.process (l); f1r.set (200.0f * std::pow (60.0f, C) * 1.03f, 0.9f, sr); f1r.process (r);
                x = (f1.lpOut + f1r.lpOut * 0.25f) * 0.45f; y = (f1r.lpOut + f1.lpOut * 0.25f) * 0.45f; stereo = true;
                break;
            }
            default: // 15 ---------------- STATIC & RADIO (tappeto)
            {
                float l = 0, r = 0;
                switch (v)
                {
                    case 0: // polvere di vinile: crepitii radi + fruscio
                    {
                        const float prob = (5.0f + D * 600.0f) / sr;
                        if (rng.uni() < prob) held = rng.bi() * (0.3f + rng.uni() * 0.7f);
                        held *= 0.6f;
                        f1.set (800.0f + C * 6000.0f, 0.7f, sr); f1.process (rng.bi());
                        l = held + f1.lpOut * T * 0.15f;
                        if (rng.uni() < prob) heldR = rng.bi() * (0.3f + rng.uni() * 0.7f);
                        heldR *= 0.6f; r = heldR + f1.lpOut * T * 0.15f;
                        break;
                    }
                    case 1: // onde corte: sintonia che scivola, fischi e rumore
                    {
                        driftA += dt * rateHz * 0.1f;
                        const float tune = f * 4.0f * (1.0f + 0.5f * std::sin (kTwoPi * driftA) * M + 0.3f * std::sin (kTwoPi * driftA * 2.7f) * M);
                        const float whistle = std::sin (kTwoPi * phStep (ph[0], tune * sweepMul (P), sr)) * (0.3f + T * 0.7f);
                        f1.set (tune * 0.6f + 300.0f, 2.0f, sr); f1.process (rng.bi());
                        const float am = 0.5f + 0.5f * std::sin (kTwoPi * phStep (ph[1], 3.0f + D * 40.0f, sr));
                        l = r = (whistle * 0.4f + f1.bpOut * (0.4f + C)) * am;
                        break;
                    }
                    case 2: // ronzio elettrico 50 Hz + buzz
                    {
                        const float p = phStep (ph[0], 50.0f * semis (P.k[LK_Tune]) * sweepMul (P), sr);
                        float s = std::sin (kTwoPi * p) * 0.5f + sawOf (phStep (ph[1], 100.0f * semis (P.k[LK_Tune]), sr)) * T * 0.4f;
                        if (rng.uni() < D * 0.002f) held = 1.0f;
                        held *= 0.999f;
                        s += rng.bi() * held * C;
                        l = r = fastTanh (s * (1.0f + M * 6.0f)) * 0.6f;
                        break;
                    }
                    default: // modem: toni a salti rapidi
                    {
                        acc += (20.0f + R * 300.0f) / sr;
                        if (acc >= 1.0f) { acc -= 1.0f; static const float tones[] = { 1070, 1270, 2025, 2225, 980, 1180, 1650, 1850 }; held = tones[rng.next() % 8] * (0.5f + T); }
                        l = r = std::sin (kTwoPi * phStep (ph[0], held * sweepMul (P), sr)) * 0.45f + rng.bi() * M * 0.1f;
                        break;
                    }
                }
                x = l; y = r; stereo = true;
                break;
            }
        }
        // GRIT generale: saturazione + un pizzico di crush
        const float g = P.k[LK_Grit];
        if (g > 0.001f && fam != 5 && fam != 12)
        {
            x = lerp (x, fastTanh (x * (1.0f + g * 8.0f)), g);
            y = lerp (y, fastTanh (y * (1.0f + g * 8.0f)), g);
        }
        if (! stereo) y = x;
        if (! finite (x) || ! finite (y)) { x = y = 0; for (auto& p : ph) p = 0; f1.reset(); f2.reset(); f3.reset(); f1r.reset(); comb.clear(); comb2.clear(); combLp = combLp2 = 0; }
        outL = x; outR = y;
    }
};

// ======================================================================= TAIL: 9 code
constexpr int kNumTails = 10;
inline const char* tailName (int i)
{
    static const char* n[kNumTails] = { "Off", "Bell", "Metal", "String", "808 Boom", "Glass", "Tube", "Drone Pad", "Noise Wash", "Reverse Swell" };
    return n[std::min (std::max (i, 0), kNumTails - 1)];
}
struct TailGen
{
    float sr = 48000; Rng rng { 0x7A11u };
    int type = 0; float t = 0, env = 0; bool on = false;
    double ph[6] {}; float amp[6] {};
    DelayLine ks; float ksLp = 0, exc = 0; Svf f1, f2;
    void prepare (float s) { sr = s; ks.init ((int) (sr * 0.05f)); }
    void trigger (int ty)
    {
        type = ty; on = ty > 0; t = 0; env = 1;
        for (int i = 0; i < 6; ++i) { ph[i] = 0; amp[i] = 1.0f; }
        if (type == 3) { ks.clear(); ksLp = 0; exc = 1; }
    }
    // decay in secondi, tone 0..1
    void process (float f, float decay, float tone, float& L, float& R)
    {
        L = R = 0;
        if (! on) return;
        const float dt = 1.0f / sr; t += dt;
        const float d = std::max (0.01f, decay);
        float s = 0, s2 = 0;
        auto step = [&] (int i, float hz) { ph[i] += (double) std::min (hz, sr * 0.45f) / sr; ph[i] -= std::floor (ph[i]); return (float) ph[i]; };
        const float tf = semis ((tone - 0.5f) * 24.0f);
        switch (type)
        {
            case 1: { static const float r[4] = { 1.0f, 2.0f, 2.76f, 5.4f }; for (int i = 0; i < 4; ++i) s += std::sin (kTwoPi * step (i, f * 2.0f * tf * r[i])) * std::exp (-t / (d / (1.0f + i * 0.8f))) / (1.0f + i); s2 = s; break; }
            case 2: { static const float r[6] = { 1.0f, 1.42f, 2.08f, 2.79f, 3.41f, 4.27f }; for (int i = 0; i < 6; ++i) { const float v = std::sin (kTwoPi * step (i, f * 3.0f * tf * r[i])) * std::exp (-t / (d / (1.0f + i * 0.5f))) / (1.0f + i * 0.4f); if (i % 2) s2 += v; else s += v; } s *= 0.7f; s2 *= 0.7f; break; }
            case 3:
            {
                const float hz = std::max (30.0f, f * tf);
                const float r1 = ks.read (sr / hz);
                ksLp += (0.15f + tone * 0.8f) * (r1 - ksLp);
                const float fb = std::exp (-1.0f / (d * hz));
                float e = 0; if (exc > 0) { e = rng.bi() * exc; exc *= 0.99f; if (exc < 1e-3f) exc = 0; }
                ks.push (e + ksLp * fb); s = s2 = r1;
                break;
            }
            case 4: { const float hz = f * 0.5f * tf * (1.0f + 0.4f * std::exp (-t * 30.0f)); s = s2 = fastTanh (std::sin (kTwoPi * step (0, hz)) * 1.6f) * std::exp (-t / d * 4.6f); break; }
            case 5: { static const float r[4] = { 2.0f, 5.43f, 9.1f, 13.3f }; for (int i = 0; i < 4; ++i) s += std::sin (kTwoPi * step (i, f * 4.0f * tf * r[i])) * std::exp (-t / (d * 0.7f / (1.0f + i * 0.3f))) * 0.4f; s2 = s; break; }
            case 6: { f1.set (f * 2.0f * tf, 30.0f, sr); f2.set (f * 4.1f * tf, 30.0f, sr); const float n = rng.bi() * std::exp (-t / d * 4.6f); f1.process (n); f2.process (n); s = f1.bpOut * 6.0f; s2 = (f1.bpOut + f2.bpOut) * 3.0f; break; }
            case 7: { const float e = std::min (1.0f, t / 0.15f) * std::exp (-t / d * 4.6f); for (int i = 0; i < 4; ++i) { const float p = step (i, f * tf * (1.0f + (i - 1.5f) * 0.006f) * (i == 3 ? 2.0f : 1.0f)); const float v = (2.0f * p - 1.0f) * e; if (i % 2) s2 += v; else s += v; } f1.set (300.0f + tone * 3000.0f, 0.7f, sr); f1.process (s); f2.set (300.0f + tone * 3000.0f, 0.7f, sr); f2.process (s2); s = f1.lpOut * 0.5f; s2 = f2.lpOut * 0.5f; break; }
            case 8: { const float e = std::exp (-t / d * 4.6f); f1.set (400.0f + tone * 8000.0f, 0.6f, sr); f1.process (rng.bi()); f2.set (400.0f + tone * 8000.0f, 0.6f, sr); f2.process (rng.bi()); s = f1.lpOut * e * 0.6f; s2 = f2.lpOut * e * 0.6f; break; }
            case 9: { const float u = t / d; const float e = u < 1.0f ? u * u * u : 0.0f; if (u >= 1.0f) { on = false; break; } f1.set (300.0f + tone * 6000.0f * u, 2.0f, sr); f1.process (rng.bi()); const float ton = std::sin (kTwoPi * step (0, f * 2.0f * tf)); s = (f1.bpOut * 2.0f + ton * 0.5f) * e; s2 = s; break; }
            default: on = false; break;
        }
        if (t > d * 1.6f + 0.05f && type != 9) on = false;
        if (! finite (s) || ! finite (s2)) { s = s2 = 0; on = false; f1.reset(); f2.reset(); ks.clear(); }
        L = s; R = s2;
    }
};

// ======================================================================= BRK TAPZ
// Linea multi-tap: fino a 32 ripetizioni la cui distanza si accorcia (accelerando) o si allunga (rallentando),
// come una pallina che rimbalza. Le ripetizioni nascono dal segnale della voce.
struct BrkTapz
{
    static constexpr int kMaxTaps = 32;
    float sr = 48000; std::vector<float> bufL, bufR; int w = 0, size = 0;
    int nTaps = 0; int dly[kMaxTaps] {}; float gL[kMaxTaps] {}, gR[kMaxTaps] {};
    float tailHold = 0;        // campioni di coda ancora da far suonare
    void prepare (float s) { sr = s; size = (int) (sr * 6.0f); bufL.assign ((size_t) size, 0.0f); bufR.assign ((size_t) size, 0.0f); w = 0; }
    void reset() { std::fill (bufL.begin(), bufL.end(), 0.0f); std::fill (bufR.begin(), bufR.end(), 0.0f); tailHold = 0; }
    // taps 1..32, first = primo intervallo (s), shape -1..1 (<0 rallenta, >0 accelera), decay 0..1, spread 0..1
    void configure (int taps, float first, float shape, float decay, float spread)
    {
        nTaps = std::min (kMaxTaps, std::max (1, taps));
        const float ratio = std::pow (2.0f, -shape * 1.2f);   // accelera: ogni intervallo è più corto
        float gap = std::max (0.004f, first), acc = 0, g = 1.0f;
        const float fall = 0.35f + decay * 0.62f;              // guadagno per ripetizione
        int n = 0;
        for (int k = 0; k < nTaps; ++k)
        {
            acc += gap;
            const int d = (int) (acc * sr);
            if (d >= size - 2) break;
            dly[n] = d;
            const float pan = (k % 2 == 0 ? -1.0f : 1.0f) * spread;
            gL[n] = g * std::min (1.0f, 1.0f - pan); gR[n] = g * std::min (1.0f, 1.0f + pan);
            g *= fall; gap = std::max (0.0015f, gap * ratio);
            ++n;
        }
        nTaps = n;
    }
    int span() const { return nTaps > 0 ? dly[nTaps - 1] : 0; }
    void process (float inL, float inR, float mix, float& outL, float& outR)
    {
        bufL[(size_t) w] = inL; bufR[(size_t) w] = inR;
        float l = 0, r = 0;
        for (int k = 0; k < nTaps; ++k)
        {
            int rp = w - dly[k]; if (rp < 0) rp += size;
            l += bufL[(size_t) rp] * gL[k]; r += bufR[(size_t) rp] * gR[k];
        }
        if (++w >= size) w = 0;
        outL = inL + l * mix; outR = inR + r * mix;
        if (! finite (outL) || ! finite (outR)) { reset(); outL = inL; outR = inR; }
    }
};

// ======================================================================= BRK REPEAT
// Stutter: a ogni colpo registra una fetta (SLICE) e la ripete COUNT volte; ogni ripetizione può essere
// un buco (GAPS), al contrario (REVERSE) o ridotta di bit/frequenza (CRUSH). Spezza i colpi in fill glitch.
struct BrkRepeat
{
    float sr = 48000; Rng rng { 0x5E9EA7u };
    std::vector<float> sl, sr2; int sliceLen = 1, recPos = 0, playPos = 0, repLeft = 0;
    bool recording = false, playing = false;
    bool curGap = false, curRev = false; float curCrush = 0, crushAcc = 0, heldL = 0, heldR = 0, curGain = 1;
    void prepare (float s) { sr = s; sl.assign ((size_t) (sr * 2.0f), 0.0f); sr2.assign ((size_t) (sr * 2.0f), 0.0f); }
    void reset() { recording = playing = false; }
    void trigger (float sliceSec, int count)
    {
        sliceLen = std::max (16, std::min ((int) sl.size() - 1, (int) (sliceSec * sr)));
        recPos = 0; recording = true; playing = false; repLeft = std::max (0, count);
    }
    bool active() const { return recording || playing; }
    void newRepeat (float gaps, float rev, float crush)
    {
        playPos = 0;
        curGap = rng.uni() < gaps; curRev = rng.uni() < rev;
        curCrush = rng.uni() < crush ? 0.3f + 0.7f * rng.uni() * crush : 0.0f;
        curGain = 0.85f + 0.15f * rng.uni();
    }
    void process (float inL, float inR, float gaps, float rev, float crush, float mix, float& outL, float& outR)
    {
        outL = inL; outR = inR;
        if (recording)
        {
            sl[(size_t) recPos] = inL; sr2[(size_t) recPos] = inR;
            if (++recPos >= sliceLen) { recording = false; if (repLeft > 0) { playing = true; newRepeat (gaps, rev, crush); } }
            return;
        }
        if (! playing) return;
        const int idx = curRev ? sliceLen - 1 - playPos : playPos;
        float l = sl[(size_t) idx], r = sr2[(size_t) idx];
        // finestra corta ai bordi per evitare i click
        const int fadeN = std::min (sliceLen / 4, (int) (sr * 0.002f) + 1);
        float win = 1.0f;
        if (playPos < fadeN) win = (float) playPos / (float) fadeN;
        else if (playPos > sliceLen - fadeN) win = (float) (sliceLen - playPos) / (float) fadeN;
        if (curCrush > 0)
        {
            crushAcc += 1.0f - curCrush * 0.95f;
            if (crushAcc >= 1.0f) { crushAcc -= 1.0f; const float q = std::pow (2.0f, 2.0f + (1.0f - curCrush) * 10.0f); heldL = std::round (l * q) / q; heldR = std::round (r * q) / q; }
            l = heldL; r = heldR;
        }
        const float g = curGap ? 0.0f : win * curGain;
        outL = lerp (inL, l * g, mix); outR = lerp (inR, r * g, mix);
        if (++playPos >= sliceLen) { if (--repLeft <= 0) playing = false; else newRepeat (gaps, rev, crush); }
    }
};

// divisioni per la fetta di BRK REPEAT e il primo intervallo di BRK TAPZ (in battiti da 1/4)
constexpr int kNumBrkDivs = 9;
inline const char* brkDivName (int i) { static const char* n[] = { "1/128", "1/64", "1/32T", "1/32", "1/16T", "1/16", "1/8T", "1/8", "1/4" }; return n[std::min (std::max (i, 0), kNumBrkDivs - 1)]; }
inline float brkDivBeats (int i) { static const float b[] = { 1.0f / 32, 1.0f / 16, 1.0f / 12, 0.125f, 1.0f / 6, 0.25f, 1.0f / 3, 0.5f, 1.0f }; return b[std::min (std::max (i, 0), kNumBrkDivs - 1)]; }
} // namespace mp
