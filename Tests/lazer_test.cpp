// Test dello strato LAZER: ogni motore suona, resta finito e non esplode; code, TAPZ e REPEAT.
#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
static void init (VoiceSettings& s) { for (int t = 0; t < kNumTargets; ++t) s.base[t] = norm (t, targetMap (t).def); }
int main()
{
    const int sr = 48000; int bad = 0;
    for (int e = 0; e < kNumLzEngines; ++e)
    {
        VoiceShared sh; Voice v; v.shared = &sh; v.prepare (sr); VoiceSettings s; init (s);
        s.synthOn = false; s.lzOn = true; s.lzEngine = e; s.base[T_LzDecay] = norm (T_LzDecay, 0.6f);
        TriggerInfo ti; v.trigger (s, ti);
        std::vector<float> L (sr), R (sr), a (sr), b (sr), c (sr), d (sr);
        v.render (s, L.data(), R.data(), a.data(), b.data(), c.data(), d.data(), sr);
        float pk = 0; double rms = 0, side = 0; bool fin = true;
        for (int i = 0; i < sr; ++i) { fin &= std::isfinite (L[i]) && std::isfinite (R[i]); pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i]))); rms += L[i] * L[i]; side += (L[i] - R[i]) * (L[i] - R[i]); }
        rms = std::sqrt (rms / sr);
        const bool ok = fin && pk > 0.01f && pk < 4.0f;
        if (! ok) ++bad;
        std::printf ("%2d %-15s %-14s peak=%.3f rms=%.4f side=%.4f %s\n", e, lzFamilyName (e / 4), lzEngineName (e), pk, rms, std::sqrt (side / sr), ok ? "" : "<-- PROBLEMA");
    }
    // estremi casuali per 200 configurazioni
    Rng r (77);
    for (int it = 0; it < 200; ++it)
    {
        VoiceShared sh; Voice v; v.shared = &sh; v.prepare (sr); VoiceSettings s; init (s);
        for (int t = 0; t < kNumTargets; ++t) s.base[t] = r.uni() < 0.3f ? (r.uni() < 0.5f ? 0.f : 1.f) : r.uni();
        s.base[T_Gain] = norm (T_Gain, 0);
        s.lzOn = true; s.lzEngine = (int) (r.next() % kNumLzEngines); s.tail = (int) (r.next() % kNumTails);
        s.tzOn = r.uni() < 0.5f; s.tzTaps = 1 + (int) (r.next() % 32); s.tzShape = r.bi(); s.tzFirst = 0.004f + r.uni() * 0.5f; s.tzDecay = r.uni();
        s.rpOn = r.uni() < 0.5f; s.rpSlice = 0.005f + r.uni() * 0.3f; s.rpCount = (int) (r.next() % 17); s.rpGaps = r.uni(); s.rpRev = r.uni(); s.rpCrush = r.uni();
        s.lzEnvSrc = (int) (r.next() % 3);
        std::vector<float> L (sr / 2), R (sr / 2), a (sr / 2), b (sr / 2), c (sr / 2), d (sr / 2);
        for (int k = 0; k < 4; ++k)
        {
            TriggerInfo ti; ti.velocity = r.uni(); ti.pitchSt = r.bi() * 24; v.trigger (s, ti);
            std::fill (L.begin(), L.end(), 0.f); std::fill (R.begin(), R.end(), 0.f);
            v.render (s, L.data(), R.data(), a.data(), b.data(), c.data(), d.data(), sr / 2);
            for (int i = 0; i < sr / 2; ++i) if (! std::isfinite (L[i]) || std::abs (L[i]) > 20.0f) { std::printf ("estremo %d: valore %f motore %s\n", it, L[i], lzEngineName (s.lzEngine)); ++bad; break; }
        }
    }
    // TAPZ: un colpo corto deve produrre ripetizioni che si avvicinano
    {
        VoiceShared sh; Voice v; v.shared = &sh; v.prepare (sr); VoiceSettings s; init (s);
        s.synthOn = true; s.engine = EngKick; s.base[T_E1Decay] = norm (T_E1Decay, 0.02f);
        s.tzOn = true; s.tzTaps = 12; s.tzFirst = 0.2f; s.tzShape = 0.8f; s.tzMix = 1.0f;
        TriggerInfo ti; v.trigger (s, ti);
        const int n = sr * 2; std::vector<float> L (n), R (n), a (n), b (n), c (n), d (n);
        v.render (s, L.data(), R.data(), a.data(), b.data(), c.data(), d.data(), n);
        int bursts = 0; bool in = false; int lastOn = -100000; std::printf ("TAPZ colpi a (ms):");
        for (int i = 0; i < n; ++i) { const bool loud = std::abs (L[i]) > 0.02f; if (loud && ! in && i - lastOn > sr / 200) { ++bursts; std::printf (" %d", i * 1000 / sr); lastOn = i; } in = loud; if (loud) lastOn = i; }
        std::printf ("  (%d)\n", bursts);
        if (bursts < 6) { std::printf ("TAPZ: troppo poche ripetizioni\n"); ++bad; }
    }
    std::printf (bad ? "FALLITO: %d problemi\n" : "OK\n", bad);
    return bad ? 1 : 0;
}
