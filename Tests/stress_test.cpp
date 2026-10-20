// Simula una lunga sessione DAW: loop, blocchi variabili, cambi parametri continui, choke, MIDI.
// Segnala quando l'uscita resta muta mentre il sequencer sta generando colpi.
#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
using namespace mp;
int main (int argc, char** argv)
{
    const int sr = 48000; Rng r (argc > 1 ? (uint32_t) atoi (argv[1]) : 7u);
    auto dm = std::make_unique<DrumMachine>(); dm->prepare ((float) sr, 512);
    for (int v = 0; v < 8; ++v) { for (int t = 0; t < kNumTargets; ++t) dm->vs[v].base[t] = norm (t, targetMap (t).def);
        dm->vs[v].engine = (v * 3) % kNumSynthEngines; dm->ss[v].fill = 4 + v % 5; dm->vs[v].base[T_Gain] = norm (T_Gain, -6); }
    dm->gs.choke[2] = 4; dm->gs.choke[3] = 3; dm->gs.link[0] = 6;
    std::vector<float> L (4096), R (4096);
    double ppq = 0; const double loopStart = 4, loopEnd = 20; long long total = 0; int silentBlocks = 0, worstSilent = 0, nans = 0;
    double silentSince = -1; int incidents = 0;
    for (int b = 0; b < 200000; ++b)
    {
        const int n = 32 + (int) (r.uni() * 1000);
        Transport tp; tp.hostPlaying = true; tp.hostHasPosition = true; tp.bpm = 90 + (b / 5000) % 80; tp.ppq = ppq;
        // cambi di parametri casuali come un'automazione/dado
        if (r.uni() < 0.05f)
        {
            const int v = r.next() % 8;
            const int t = r.next() % kNumTargets; dm->vs[v].base[t] = r.uni();
            if (r.uni() < 0.2f) dm->vs[v].engine = r.next() % kNumSynthEngines;
            if (r.uni() < 0.2f) dm->vs[v].filterMode = r.next() % kNumFilterModes;
            if (r.uni() < 0.2f) dm->vs[v].distMode = r.next() % kNumDistModes;
            if (r.uni() < 0.1f) dm->vs[v].base[T_Gain] = norm (T_Gain, -6);
            if (r.uni() < 0.1f) dm->ss[v].speedIdx = r.next() % kNumSpeeds;
            if (r.uni() < 0.1f) dm->vs[v].variation = r.uni();
            if (r.uni() < 0.1f) dm->vshared[v].depth[r.next() % kNumSources][r.next() % kNumTargets] = r.bi();
        }
        if (r.uni() < 0.01f) dm->noteOn (1 + r.next() % 9, r.next() % 128, r.uni(), 0);
        dm->process (L.data(), R.data(), n, tp);
        float pk = 0; for (int i = 0; i < n; ++i) { if (! std::isfinite (L[i])) ++nans; pk = std::max (pk, std::abs (L[i])); }
        // tutte le voci a gain non zero con fill>0 → deve sempre esserci suono
        bool anyAudible = false;
        for (int v = 0; v < 8; ++v) anyAudible |= denorm (T_Gain, dm->vs[v].base[T_Gain]) > -40;
        if (pk < 1e-5f && anyAudible) { if (silentSince < 0) silentSince = total / (double) sr; }
        else if (silentSince >= 0)
        {
            const double dur = total / (double) sr - silentSince;
            if (dur > 3.0) { ++incidents; std::printf ("silenzio %.1fs a t=%.1fs\n", dur, silentSince); }
            silentSince = -1;
        }
        total += n;
        ppq += n / (double) sr * tp.bpm / 60.0;
        if (ppq >= loopEnd) ppq = loopStart + (ppq - loopEnd);
    }
    if (silentSince >= 0) { std::printf ("silenzio FINO ALLA FINE da t=%.1fs (%.1fs)\n", silentSince, total / (double) sr - silentSince); ++incidents; }
    std::printf ("simulati %.0f s, incidenti=%d, nans=%d\n", total / (double) sr, incidents, nans);
    return incidents ? 1 : 0;
}
