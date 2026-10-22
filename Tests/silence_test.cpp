// Silenzio improvviso: la macchina intera per molti minuti simulati, con kit casuali cambiati spesso, il trasporto
// del DAW in loop (il ppq torna indietro), cambi di tempo e blocchi di dimensione variabile. Segnala ogni tratto di
// più di 2 s in cui la macchina suona, le voci ricevono colpi ma l'uscita è a zero.
#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;

static void randomKit (DrumMachine& dm, Rng& r, bool extreme)
{
    for (int v = 0; v < kNumVoices; ++v)
    {
        auto& s = dm.vs[v];
        for (int t = 0; t < kNumTargets; ++t) s.base[t] = extreme && r.uni() < 0.3f ? (r.uni() < 0.5f ? 0.f : 1.f) : r.uni();
        s.base[T_Gain] = norm (T_Gain, -6.0f);
        s.engine = (int) (r.next() % kNumSynthEngines); s.filterMode = (int) (r.next() % kNumFilterModes);
        s.distMode = (int) (r.next() % kNumDistModes); s.stereo = r.uni() < 0.5f; s.post = r.uni() < 0.5f;
        s.synthEnv2 = r.uni() < 0.5f; s.variation = r.uni();
        s.l1Sync = r.uni() < 0.5f; s.l2Sync = r.uni() < 0.5f; s.l1Wave = (int) (r.next() % 4); s.l2Wave = (int) (r.next() % 4);
        s.l1Reset = (int) (r.next() % 5); s.l2Reset = (int) (r.next() % 5); s.l1Shape = r.uni(); s.l2Shape = r.uni();
        s.macro1 = r.uni(); s.macro2 = r.uni();
        for (int k = 0; k < kNumSources; ++k) for (int t = 0; t < kNumTargets; ++t)
            dm.vshared[v].depth[k][t] = r.uni() < 0.15f ? r.bi() : 0.0f;
        auto& q = dm.ss[v];
        q.steps = 16; q.fill = 2 + (int) (r.next() % 10); q.pattern = 1 + (int) (r.next() % 8); q.speedIdx = 4;
        q.density = r.bi() * 0.5f; q.swing = r.bi() * 0.5f;
        for (int i = 0; i < 16; ++i) { dm.sshared[v].lane[L_Ratch][i] = 1 + (float) (r.next() % 4); dm.sshared[v].lane[L_Prob][i] = r.bi() * 100; }
    }
    auto& g = dm.gs;
    g.dFeedback = r.uni(); g.dGlitch = r.uni(); g.dFbTilt = r.bi(); g.dDucking = r.uni(); g.dSRate = r.uni();
    g.dSync = r.uni() < 0.5f; g.dTimeMs = 1 + r.uni() * 1999; g.dDivIdx = (int) (r.next() % 13); g.dToRev = r.uni();
    g.dStereo = r.uni() < 0.5f; g.dPong = r.uni() < 0.5f; g.dGain = r.uni() * 2;
    g.rMode = (int) (r.next() % 4); g.rDecay = 0.1f + r.uni() * 19.9f; g.rPreMs = r.uni() * 1000; g.rSRate = r.uni();
    g.rInTilt = r.bi(); g.rOutTilt = r.bi(); g.rGain = r.uni() * 2; g.rSync = r.uni() < 0.5f; g.rDivIdx = (int) (r.next() % 13);
    g.maximize = r.uni(); g.eqLow = r.bi() * 18; g.eqMid = r.bi() * 18; g.eqHigh = r.bi() * 18; g.clip = (int) (r.next() % 4);
    g.humanize = r.uni();
}

int main (int argc, char** argv)
{
    const int sr = 48000;
    const int minutes = argc > 1 ? std::atoi (argv[1]) : 30;
    const uint32_t seed = argc > 2 ? (uint32_t) std::atoi (argv[2]) : 7u;
    DrumMachine dm; dm.prepare (sr, 512);
    Rng r (seed);
    randomKit (dm, r, false);
    Transport tp; tp.bpm = 120; tp.hostPlaying = true; tp.hostHasPosition = true;
    std::vector<float> L (4096), R (4096);
    long long t = 0, silentFrom = -1, nextKit = sr * 4; int found = 0;
    const long long total = (long long) minutes * 60 * sr;
    while (t < total)
    {
        int n = 32 + (int) (r.next() % 993);
        // il processore spezza in blocchi da <= 256
        for (int pos = 0; pos < n;)
        {
            const int len = std::min (256, n - pos);
            Transport t2 = tp; t2.ppq = tp.ppq + (double) pos / sr * tp.bpm / 60.0;
            dm.process (L.data() + pos, R.data() + pos, len, t2);
            pos += len;
        }
        float pk = 0; bool bad = false;
        for (int i = 0; i < n; ++i) { pk = std::max (pk, std::max (std::abs (L[i]), std::abs (R[i]))); bad |= ! std::isfinite (L[i]) || ! std::isfinite (R[i]); }
        if (bad) { std::printf ("NaN in uscita a %.1f s\n", (double) t / sr); ++found; }
        if (pk < 1e-6f) { if (silentFrom < 0) silentFrom = t; }
        else
        {
            if (silentFrom >= 0 && t - silentFrom > 2 * sr)
            {
                std::printf ("silenzio da %.1f s per %.1f s\n", (double) silentFrom / sr, (double) (t - silentFrom) / sr);
                ++found;
            }
            silentFrom = -1;
        }
        if (silentFrom >= 0 && t - silentFrom > 8 * sr)
        {
            std::printf ("SILENZIO PERSISTENTE da %.1f s (running=%d)\n", (double) silentFrom / sr, (int) dm.running.load());
            ++found;
            return 2;
        }
        tp.ppq += (double) n / sr * tp.bpm / 60.0;
        if (tp.ppq >= 16.0) tp.ppq -= 16.0;                 // loop di 4 battute come in Ableton
        t += n;
        if (t >= nextKit)
        {
            randomKit (dm, r, r.uni() < 0.3f);
            if (r.uni() < 0.2f) tp.bpm = 60 + r.uni() * 180;
            nextKit = t + sr * (2 + (long long) (r.next() % 6));
        }
    }
    std::printf ("%d min, seme %u: %s (ripristini: voci %d, effetti %d)\n", minutes, seed, found ? "PROBLEMI" : "ok", dm.voiceRecoveries(), dm.fxRecoveries.load());
    return found ? 1 : 0;
}
