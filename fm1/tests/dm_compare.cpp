// Drummatic FM-1: the port against the original (Source/DSP of Drummatic 0.2.3, pure C++).
// Per engine: the same hit rendered by both, compared (level, correlation, spectral centroid).
// Then the power-on kit, two bars at 120 BPM through both machines (level, centroid, per voice hits).
#include "../../Source/DSP/DrumMachine.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <string>
extern "C" {
#include "../firmware/src/dm/dm_machine.h"
#ifdef DM_TEST_HOOK
extern void (*dm_test_event)(uint64_t, uint32_t, const dm_trig_t*);
#endif
}
#ifdef DM_TEST_HOOK
static void evp (uint64_t s, uint32_t v, const dm_trig_t* t) { if (std::getenv ("DMEV") && (int) v == std::atoi (std::getenv ("DMEV")) && s < 44100) std::printf ("P %7llu v%u vel %.3f\n", (unsigned long long) s, v, t->vel); }
#endif

static double rms(const std::vector<float>& x) { double s = 0; for (float v : x) s += (double) v * v; return std::sqrt (s / std::max<size_t> (1, x.size())); }
static double peak(const std::vector<float>& x) { double p = 0; for (float v : x) p = std::max (p, (double) std::fabs (v)); return p; }
static double corr(const std::vector<float>& a, const std::vector<float>& b)
{
    double ab = 0, aa = 0, bb = 0; size_t n = std::min (a.size(), b.size());
    for (size_t i = 0; i < n; ++i) { ab += (double) a[i] * b[i]; aa += (double) a[i] * a[i]; bb += (double) b[i] * b[i]; }
    return aa > 0 && bb > 0 ? ab / std::sqrt (aa * bb) : (aa == bb ? 1.0 : 0.0);
}
// spectral centroid by a crude DFT over a few frames (Hz)
static double centroid(const std::vector<float>& x)
{
    const int N = 1024; double num = 0, den = 0;
    for (size_t f0 = 0; f0 + N <= x.size() && f0 < 44100; f0 += N)
        for (int k = 1; k < N / 2; k += 2)
        {
            double re = 0, im = 0;
            for (int i = 0; i < N; ++i) { double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / N); re += x[f0 + i] * w * std::cos (2 * M_PI * k * i / N); im -= x[f0 + i] * w * std::sin (2 * M_PI * k * i / N); }
            double m = std::sqrt (re * re + im * im); num += m * k * 44100.0 / N; den += m;
        }
    return den > 0 ? num / den : 0;
}
static void wav(const char* path, const std::vector<float>& l, const std::vector<float>& r)
{
    FILE* f = std::fopen (path, "wb"); if (! f) return;
    uint32_t n = (uint32_t) l.size(), bytes = n * 4, v; uint16_t s;
    std::fwrite ("RIFF", 1, 4, f); v = 36 + bytes; std::fwrite (&v, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f);
    v = 16; std::fwrite (&v, 4, 1, f); s = 1; std::fwrite (&s, 2, 1, f); s = 2; std::fwrite (&s, 2, 1, f);
    v = 44100; std::fwrite (&v, 4, 1, f); v = 44100 * 4; std::fwrite (&v, 4, 1, f); s = 4; std::fwrite (&s, 2, 1, f); s = 16; std::fwrite (&s, 2, 1, f);
    std::fwrite ("data", 1, 4, f); std::fwrite (&bytes, 4, 1, f);
    for (uint32_t i = 0; i < n; ++i) { int16_t a = (int16_t) std::lrint (std::max (-1.f, std::min (1.f, l[i])) * 32767), b = (int16_t) std::lrint (std::max (-1.f, std::min (1.f, r[i])) * 32767); std::fwrite (&a, 2, 1, f); std::fwrite (&b, 2, 1, f); }
    std::fclose (f);
}

int main (int argc, char** argv)
{
    const char* outdir = argc > 1 ? argv[1] : nullptr;
    dm_math_init();
    int fails = 0;
    std::printf ("%-10s %8s %8s %7s %8s %8s %6s\n", "engine", "rms orig", "rms port", "corr", "cent.o", "cent.p", "ok");
    const float knobs[3][4] = { { 0.5f, 0.5f, 0.5f, 0.5f }, { 0.2f, 0.8f, 0.3f, 0.7f }, { 0.9f, 0.1f, 0.8f, 0.2f } };
    static int16_t line[2048];
    for (int id = 0; id < ENG_COUNT; ++id)
        for (int kset = 0; kset < 3; ++kset)
        {
            auto eng = mp::makeEngine (id); eng->prepare (44100); eng->rng = mp::Rng (0x1000u + (uint32_t) (id * 7));
            dm_eng_t e; dm_eng_reset (&e, (uint32_t) id, 0x1000u + (uint32_t) id * 7u, line, 2048);
            const float* p = knobs[kset];
            float f = mp::Voice::baseFreq (id);
            std::vector<float> a, b;
            eng->trigger (1.0f); dm_eng_trigger (&e, 1.0f);
            const int N = 22050;
            for (int i = 0; i < N; ++i)
            {
                if (i % 32 == 0) dm_eng_ctrl (&e, f, p);
                a.push_back (eng->process (f, p));
                b.push_back (dm_eng_run (&e));
            }
            double ra = rms (a), rb = rms (b), c = corr (a, b), ca = centroid (a), cb = centroid (b);
            double lv = ra > 1e-6 ? 20 * std::log10 (rb / ra) : 0;
            bool ok = id == ENG_CHAOS ? std::fabs (lv) < 3.0   /* (chaotic: any rounding sends it elsewhere) */
                                      : std::fabs (lv) < 1.5 && (c > 0.5 || std::fabs (ca - cb) < 0.15 * std::max (ca, cb) + 200);
            if (! ok) ++fails;
            std::printf ("%-10s %8.4f %8.4f %7.3f %8.0f %8.0f %6s  (k%d, %+.1f dB)\n", DM_ENG_INFO[id].name, ra, rb, c, ca, cb, ok ? "ok" : "DIFF", kset, lv);
        }
    // ---- the whole machine: the power-on kit, 2 bars at 120 BPM ----
    {
        mp::DrumMachine dm; dm.prepare (44100, 128);
        dm.setTransportMode (mp::DrumMachine::AlwaysPlay);
        // the parameter defaults of Params.cpp for the original
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            auto& s = dm.vs[v]; auto& q = dm.ss[v];
            static const struct { int eng; float decay; int fill, rotate; float gain, pan; } d[8] = {
                { 0, 0.35f, 4, 0, -2, 0 }, { 1, 0.22f, 2, 4, -4, 0 }, { 2, 0.05f, 8, 2, -9, 0.15f }, { 2, 0.35f, 0, 0, -10, -0.15f },
                { 3, 0.3f, 0, 0, -6, 0 }, { 4, 0.25f, 0, 0, -8, 0 }, { 5, 0.6f, 0, 0, -10, 0.2f }, { 6, 0.2f, 0, 0, -12, -0.2f } };
            for (int t = 0; t < mp::kNumTargets; ++t) s.base[t] = mp::norm (t, mp::targetMap (t).def);
            s.base[mp::T_E1Decay] = mp::norm (mp::T_E1Decay, d[v].decay); s.base[mp::T_Gain] = mp::norm (mp::T_Gain, d[v].gain);
            s.base[mp::T_Pan] = mp::norm (mp::T_Pan, d[v].pan); s.base[mp::T_Cutoff] = 1.0f;
            s.engine = d[v].eng; s.synthOn = true; s.sampOn = false;
            q.steps = 16; q.fill = d[v].fill; q.rotate = d[v].rotate; q.pattern = 1;
        }
        dm_machine_init(); dm_kit_defaults (&dm_kit); dm_m.want_play = 1;
        const int N = 44100 * 4;   // 2 bars at 120
        std::vector<float> ol, orr, pl, pr; std::vector<float> tmpL (128), tmpR (128); std::vector<float> o (256);
        mp::Transport tp; tp.bpm = 120;
        for (int i = 0; i < N; i += 128)
        {
            dm.process (tmpL.data(), tmpR.data(), 128, tp);
            for (int k = 0; k < 128; ++k) { ol.push_back (tmpL[k]); orr.push_back (tmpR[k]); }
            dm_machine_render (o.data());
            for (int k = 0; k < 128; ++k) { pl.push_back (o[2 * k]); pr.push_back (o[2 * k + 1]); }
        }
        double ra = rms (ol), rb = rms (pl), c = corr (ol, pl);
        std::printf ("\nmachine: power-on kit 2 bars: rms %.4f / %.4f (%+.2f dB), peak %.3f / %.3f, corr %.3f, centroid %.0f / %.0f\n",
                     ra, rb, 20 * std::log10 (rb / ra), peak (ol), peak (pl), c, centroid (ol), centroid (pl));
        if (std::fabs (20 * std::log10 (rb / ra)) > 1.5) ++fails;
        if (outdir) { wav ((std::string (outdir) + "/kit_orig.wav").c_str(), ol, orr); wav ((std::string (outdir) + "/kit_port.wav").c_str(), pl, pr); }
    }

    // ---- scenario 2: other engines, TONE modes, modulation, lanes, swing / shape, FX and master ----
    {
        static const int TMAP[DT_N] = { mp::T_Pitch, mp::T_P1, mp::T_P2, mp::T_P3, mp::T_P4, mp::T_Cutoff, mp::T_Res, mp::T_Distort,
            mp::T_E1Attack, mp::T_E1Hold, mp::T_E1Decay, mp::T_E2Attack, mp::T_E2Hold, mp::T_E2Decay,
            mp::T_L1Speed, mp::T_L1Amp, mp::T_L2Speed, mp::T_L2Amp, mp::T_Pan, mp::T_Gain, mp::T_DelaySend, mp::T_RevSend };
        mp::DrumMachine dm; dm.prepare (44100, 128); dm.setTransportMode (mp::DrumMachine::AlwaysPlay);
        dm_machine_init(); dm_kit_defaults (&dm_kit);
        const int ENG[8] = { ENG_TRIPWIRE, ENG_HAYWIRE, ENG_COWBELL, ENG_SCRAPYARD, ENG_ZAP, ENG_SUMSYN, ENG_RESON, ENG_VOSIM };
        const int FMODE[8] = { FM_DARK_LP, FM_SCREAM_BP, FM_SCREAM_HP, FM_COMB_FBP, FM_DARK_BP, FM_COMB_AP, FM_SCREAM_LP, FM_DARK_HP };
        const int DMODE[8] = { DM_TUBE, DM_BITCRUSH, DM_WAVEFOLD, DM_TAPE, DM_SQUARE, DM_CONTRAST, DM_RATECRUSH, DM_PHASE };
        for (int v = 0; v < 8; ++v)
        {
            dm_vkit_t* k = &dm_kit.v[v];
            k->engine = (uint8_t) ENG[v]; k->fmode = (uint8_t) FMODE[v]; k->dmode = (uint8_t) DMODE[v]; k->post = (uint8_t) (v & 1);
            k->base[T_CUTOFF] = 0.55f + 0.05f * v; k->base[T_RES] = 0.3f; k->base[T_DIST] = 0.25f;
            k->base[T_P1] = 0.3f + 0.05f * v; k->base[T_P3] = 0.7f - 0.04f * v;
            k->base[T_E1D] = dm_norm (T_E1D, 0.12f + 0.03f * v); k->base[T_GAIN] = dm_norm (T_GAIN, -9.0f);
            k->base[T_DSEND] = v < 4 ? 0.5f : 0.0f; k->base[T_RSEND] = v >= 3 ? 0.4f : 0.0f;
            k->depth[S_LFO1][T_CUTOFF] = 0.2f; k->depth[S_ENV2][T_PITCH] = 0.05f; k->depth[S_VELOCITY][T_P2] = 0.3f;
            k->l1sync = 1; k->base[T_L1SPD] = 0.55f;
            k->steps = (int8_t) (8 + v); k->fill = (int8_t) (3 + (v % 4)); k->pattern = (int8_t) (1 + v * 3); k->rotate = (int8_t) v;
            k->swing = 0.3f; k->shape = v == 2 ? 0.4f : 0.0f;
            for (int i = 0; i < 16; ++i) { k->lane[L_VEL][i] = (int8_t) (70 + (i * 13) % 58); k->lane[L_RATCH][i] = (int8_t) (i % 5 == 0 ? 3 : 1); k->lane[L_PITCH][i] = (int8_t) ((i * 7) % 5 - 2); k->lane[L_PAN][i] = (int8_t) ((i * 37) % 120 - 60); }
            k->lane_sync[L_PITCH] = LS_TRIG; k->lane_len[L_PITCH] = 5;
        }
        if (const char* st = std::getenv ("DMSTRIP"))
        {
            int m = std::atoi (st);
            for (int v = 0; v < 8; ++v)
            {
                dm_vkit_t* k = &dm_kit.v[v];
                if (m & 1) { k->swing = 0; k->shape = 0; }
                if (m & 2) for (int l = 0; l < DM_NLANE; ++l) { dm_lane_clear (k, l); k->lane_sync[l] = LS_SYNC; k->lane_len[l] = 16; }
                if (m & 4) for (int a = 0; a < DS_N; ++a) for (int t = 0; t < DT_N; ++t) k->depth[a][t] = 0;
                if (m & 8) { k->fmode = FM_SCREAM_LP; k->base[T_CUTOFF] = 1; k->base[T_RES] = 0; }
                if (m & 16) { k->dmode = DM_TAPE; k->base[T_DIST] = 0; }
                if (m & 32) k->depth[S_LFO1][T_CUTOFF] = 0;
                if (m & 64) k->depth[S_ENV2][T_PITCH] = 0;
                if (m & 128) k->depth[S_VELOCITY][T_P2] = 0;
            }
        }
        dm_kit.g.d_sync = 1; dm_kit.g.d_div = 6; dm_kit.g.d_fb = 0.5f; dm_kit.g.d_fbtilt = -0.3f; dm_kit.g.r_decay = 2.0f; dm_kit.g.r_mode = 2;
        dm_kit.g.m_low = 3.0f; dm_kit.g.m_high = -2.0f; dm_kit.g.bpm = 132.0f;
        for (int v = 0; v < 8; ++v)
        {
            const dm_vkit_t* k = &dm_kit.v[v]; auto& s = dm.vs[v]; auto& q = dm.ss[v]; auto& sh = dm.vshared[v]; auto& sq = dm.sshared[v];
            for (int t = 0; t < mp::kNumTargets; ++t) s.base[t] = mp::norm (t, mp::targetMap (t).def);
            for (int t = 0; t < DT_N; ++t) s.base[TMAP[t]] = k->base[t];
            for (int a = 0; a < DS_N; ++a) for (int t = 0; t < DT_N; ++t) sh.depth[a][TMAP[t]] = k->depth[a][t];
            s.engine = k->engine; s.filterMode = k->fmode; s.distMode = k->dmode; s.post = k->post; s.synthOn = true; s.sampOn = false;
            s.l1Sync = k->l1sync;
            q.steps = k->steps; q.fill = k->fill; q.pattern = k->pattern; q.rotate = k->rotate; q.swing = k->swing; q.shape = k->shape;
            for (int l = 0; l < DM_NLANE; ++l) { for (int i = 0; i < DM_MAXSTEPS; ++i) sq.lane[l][i] = k->lane[l][i]; sq.laneLen[l] = k->lane_len[l]; sq.laneSync[l] = k->lane_sync[l]; }
        }
        auto& G = dm.gs; G.dSync = true; G.dDivIdx = 6; G.dFeedback = 0.5f; G.dFbTilt = -0.3f; G.rDecay = 2.0f; G.rMode = 2; G.eqLow = 3; G.eqHigh = -2;
        dm_m.want_play = 1;
#ifdef DM_TEST_HOOK
        dm_test_event = evp;
#endif
        if (const char* so = std::getenv ("DMSOLO"))
            for (int v = 0; v < 8; ++v) { bool m = v != std::atoi (so); dm.mute[v] = m; dm_kit.v[v].mute = (uint8_t) m; }
        if (std::getenv ("DMDRY"))
        {
            for (int v = 0; v < 8; ++v) { dm_kit.v[v].base[T_DSEND] = dm_kit.v[v].base[T_RSEND] = 0; dm.vs[v].base[mp::T_DelaySend] = dm.vs[v].base[mp::T_RevSend] = 0; }
        }
        const int N = 44100 * 4;
        std::vector<float> ol, orr, pl, pr; std::vector<float> tmpL (128), tmpR (128); std::vector<float> o (256);
        mp::Transport tp; tp.bpm = 132;
        for (int i = 0; i < N; i += 128)
        {
            dm.process (tmpL.data(), tmpR.data(), 128, tp);
            for (int k = 0; k < 128; ++k) { ol.push_back (tmpL[k]); orr.push_back (tmpR[k]); }
            dm_machine_render (o.data());
            for (int k = 0; k < 128; ++k) { pl.push_back (o[2 * k]); pr.push_back (o[2 * k + 1]); }
        }
        double ra = rms (ol), rb = rms (pl), c = corr (ol, pl);
        std::printf ("machine: scenario 2: rms %.4f / %.4f (%+.2f dB), peak %.3f / %.3f, corr %.3f, centroid %.0f / %.0f\n",
                     ra, rb, 20 * std::log10 (rb / ra), peak (ol), peak (pl), c, centroid (ol), centroid (pl));
        if (std::fabs (20 * std::log10 (rb / ra)) > 1.5) ++fails;
        if (outdir) { wav ((std::string (outdir) + "/s2_orig.wav").c_str(), ol, orr); wav ((std::string (outdir) + "/s2_port.wav").c_str(), pl, pr); }
    }
    std::printf ("\n%d differences\n", fails);
    return fails ? 1 : 0;
}
