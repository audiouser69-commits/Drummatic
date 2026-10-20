// Test offline del motore audio: renderizza un groove e verifica NaN/clip, scrive un WAV
#include "../Source/DSP/DrumMachine.h"
#include <cstdio>
#include <cstring>

using namespace mp;

static void writeWav (const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen (path, "wb"); if (! f) return;
    const uint32_t n = (uint32_t) L.size(), dataBytes = n * 4;
    auto w32 = [&] (uint32_t v) { std::fwrite (&v, 4, 1, f); };
    auto w16 = [&] (uint16_t v) { std::fwrite (&v, 2, 1, f); };
    std::fwrite ("RIFF", 1, 4, f); w32 (36 + dataBytes); std::fwrite ("WAVEfmt ", 1, 8, f);
    w32 (16); w16 (1); w16 (2); w32 ((uint32_t) sr); w32 ((uint32_t) sr * 4); w16 (4); w16 (16);
    std::fwrite ("data", 1, 4, f); w32 (dataBytes);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) (clampf (L[i], -1, 1) * 32767), b = (int16_t) (clampf (R[i], -1, 1) * 32767);
        std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f);
    }
    std::fclose (f);
}

static void defaults (VoiceSettings& v)
{
    for (int t = 0; t < kNumTargets; ++t) v.base[t] = norm (t, targetMap (t).def);
}

int main (int argc, char** argv)
{
    const int sr = 48000, block = 256;
    auto dm = std::make_unique<DrumMachine>();
    dm->prepare ((float) sr, block);
    for (int v = 0; v < kNumVoices; ++v) defaults (dm->vs[v]);

    const int engines[8] = { EngKick, EngSnare, EngHat, EngClap, EngFM, EngModal, EngKombucha, EngLPG };
    for (int v = 0; v < 8; ++v)
    {
        auto& s = dm->vs[v]; s.engine = engines[v];
        s.base[T_E1Decay] = norm (T_E1Decay, v == 2 ? 0.05f : 0.35f);
        s.base[T_Gain] = norm (T_Gain, -6.0f);
        s.base[T_RevSend] = 0.2f; s.base[T_DelaySend] = v == 4 ? 0.4f : 0.0f;
        s.variation = 0.2f;
        dm->ss[v].steps = 16;
    }
    dm->ss[0].fill = 4; dm->ss[1].fill = 2; dm->ss[1].rotate = 4; dm->ss[2].fill = 11; dm->ss[3].fill = 3; dm->ss[3].pattern = 3;
    dm->ss[4].steps = 7; dm->ss[4].fill = 3; dm->ss[4].snap = false; dm->ss[5].fill = 5; dm->ss[5].shape = 0.5f;
    dm->ss[6].fill = 2; dm->ss[6].speedIdx = 3; dm->ss[7].fill = 6; dm->ss[7].density = -0.3f; dm->ss[2].swing = 0.3f;
    dm->sshared[0].lane[L_Ratch][3] = 3;
    dm->vshared[5].depth[S_Lfo1][T_Cutoff] = 0.3f; dm->vs[5].base[T_Cutoff] = 0.6f;
    dm->vs[1].distMode = Wavefold; dm->vs[1].base[T_Distort] = 0.4f;
    dm->gs.humanize = 0.2f; dm->gs.maximize = 0.3f; dm->gs.choke[2] = 0;
    if (argc > 2) { const int eng = std::atoi (argv[2]); for (int v = 0; v < 8; ++v) dm->vs[v].engine = (eng + v) % kNumSynthEngines; }

    dm->setTransportMode (DrumMachine::AlwaysPlay);
    Transport tp; tp.bpm = 128;
    const int total = sr * 8;
    std::vector<float> L (total), R (total);
    float peak = 0; int nans = 0;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        dm->process (L.data() + pos, R.data() + pos, n, tp);
        for (int i = pos; i < pos + n; ++i) { if (! std::isfinite (L[i])) ++nans; peak = std::max (peak, std::abs (L[i])); }
    }
    double rms = 0; for (float x : L) rms += x * x; rms = std::sqrt (rms / total);
    std::printf ("peak=%.3f rms=%.4f nans=%d\n", peak, rms, nans);
    writeWav (argc > 1 ? argv[1] : "test.wav", L, R, sr);
    return nans == 0 && rms > 0.005 ? 0 : 1;
}
