#pragma once
#include "Voice.h"
#include "Sequencer.h"
#include "Effects.h"
#include <atomic>
#include <algorithm>

namespace mp
{
constexpr int kNumVoices = 8;

struct GlobalSettings
{
    // Shatter Delay
    bool dSync = true, dStereo = true, dPong = false;
    int dDivIdx = 6;            // indice divisioni (lfoDiv*)
    float dTimeMs = 250, dFeedback = 0.3f, dGlitch = 0, dFbTilt = 0, dDucking = 0, dSRate = 1, dGain = 1, dToRev = 0;
    // Headspace Reverb
    bool rSync = false; int rDivIdx = 8; int rMode = 0;
    float rPreMs = 1, rDecay = 4, rSRate = 1, rInTilt = 0, rOutTilt = 0, rGain = 1;
    // Master
    float maximize = 0, eqLow = 0, eqMid = 0, eqHigh = 0, masterDb = 0; int clip = SoftClip;
    float humanize = 0;
    int choke[kNumVoices] {}, link[kNumVoices] {};     // 0 = nessuno, 1..8
};

struct Transport { bool hostPlaying = false; double ppq = 0; double bpm = 120; bool hostHasPosition = false; };

// Azioni richieste dal MIDI che devono girare sul thread dei messaggi (preset, random…)
struct ActionQueue
{
    static constexpr int N = 64;
    std::atomic<int> data[N]; std::atomic<int> w { 0 }, r { 0 };
    ActionQueue() { for (auto& d : data) d = 0; }
    void push (int a) { const int ww = w.load(); const int nx = (ww + 1) % N; if (nx == r.load()) return; data[ww] = a; w = nx; }
    bool pop (int& a) { const int rr = r.load(); if (rr == w.load()) return false; a = data[rr].load(); r = (rr + 1) % N; return true; }
};
enum Action { A_PrevPreset = 1, A_NextPreset, A_RandomPreset, A_RandomizeAll, A_PerfRandom, A_PerfReset, A_PerfCommit, A_RandomizeVoice0 = 100 };

class DrumMachine
{
public:
    enum TransportMode { FollowDaw, AlwaysPlay, Stopped };

    VoiceShared vshared[kNumVoices];
    SeqShared sshared[kNumVoices];
    VoiceSettings vs[kNumVoices];
    SeqSettings ss[kNumVoices];
    GlobalSettings gs;
    std::atomic<bool> mute[kNumVoices], solo[kNumVoices], momMute[kNumVoices], momSolo[kNumVoices];
    std::atomic<int> transportMode { FollowDaw };
    std::atomic<bool> running { false };           // il sequencer sta suonando
    std::atomic<double> displayBeat { 0 };
    ActionQueue actions;
    const SampleData* samples[kNumVoices] {};

    DrumMachine()
    {
        for (int v = 0; v < kNumVoices; ++v)
        {
            voices[v].shared = &vshared[v]; seqs[v].shared = &sshared[v]; seqs[v].voiceIndex = v;
            mute[v] = solo[v] = momMute[v] = momSolo[v] = false;
        }
    }

    void prepare (float sampleRate, int maxBlock)
    {
        sr = sampleRate;
        for (auto& v : voices) v.prepare (sr);
        for (auto& s : seqs) s.reset();
        delay.prepare (sr); reverb.prepare (sr); master.prepare (sr);
        maxN = std::max (maxBlock, 1024);           // margine: alcuni host mandano blocchi più grandi di quelli annunciati
        scratch.assign ((size_t) maxN * 8, 0.0f);
        events.reserve (1024);
    }

    // MIDI in ingresso (offset in campioni nel blocco corrente)
    void noteOn (int channel, int note, float vel, int offset)
    {
        if (channel == 1)
        {
            const int v = note % 12;
            if (v < kNumVoices) { TriggerInfo ti; ti.velocity = vel; addEvent (offset, v, ti); }
        }
        else if (channel >= 2 && channel <= 9)
        {
            TriggerInfo ti; ti.velocity = vel; ti.pitchSt = (float) (note - 60);
            addEvent (offset, channel - 2, ti);
        }
        else if (channel == 10)
        {
            if (note == 0) transportMode = (transportMode.load() == Stopped || (! running.load())) ? AlwaysPlay : Stopped;
            else if (note == 1) transportMode = AlwaysPlay;
            else if (note == 2) transportMode = Stopped;
            else if (note >= 12 && note < 20) mute[note - 12] = ! mute[note - 12].load();
            else if (note >= 24 && note < 32) solo[note - 24] = ! solo[note - 24].load();
            else if (note >= 36 && note < 44) momMute[note - 36] = true;
            else if (note >= 48 && note < 56) momSolo[note - 48] = true;
            else if (note >= 60 && note < 68) { Ev e; e.offset = offset; e.voice = note - 60; e.choke = true; events.push_back (e); }
        }
        else if (channel == 11)
        {
            if (note == 0) actions.push (A_PrevPreset);
            else if (note == 1) actions.push (A_NextPreset);
            else if (note == 2) actions.push (A_RandomPreset);
            else if (note == 3) actions.push (A_RandomizeAll);
            else if (note == 12) actions.push (A_PerfRandom);
            else if (note == 13) actions.push (A_PerfReset);
            else if (note == 14) actions.push (A_PerfCommit);
            else if (note >= 24 && note < 32) actions.push (A_RandomizeVoice0 + note - 24);
        }
    }
    void noteOff (int channel, int note)
    {
        if (channel == 10)
        {
            if (note >= 36 && note < 44) momMute[note - 36] = false;
            else if (note >= 48 && note < 56) momSolo[note - 48] = false;
        }
    }
    // trigger manuale dalla UI (tasto Q / click sulla voce)
    void auditionVoice (int v) { pendingAudition.store (v); }

    static constexpr int kNumOutPairs = 8;
    // auxL/auxR: puntatori alle coppie d'uscita 3-4 … 15-16 (nullptr = non attive → finisce sul Main)
    int maxBlock() const { return maxN; }
    void process (float* outL, float* outR, int n, const Transport& tp, float* const* auxL = nullptr, float* const* auxR = nullptr)
    {
        if (n > maxN) n = maxN;   // sicurezza: il chiamante spezza comunque in blocchi più piccoli
        std::fill (outL, outL + n, 0.0f); std::fill (outR, outR + n, 0.0f);
        if (auxL != nullptr)
            for (int k = 0; k < kNumOutPairs - 1; ++k)
                if (auxL[k] != nullptr) { std::fill (auxL[k], auxL[k] + n, 0.0f); std::fill (auxR[k], auxR[k] + n, 0.0f); }
        for (int v = 0; v < kNumVoices; ++v) voices[v].sample = samples[v];
        const int aud = pendingAudition.exchange (-1);
        if (aud >= 0 && aud < kNumVoices) { TriggerInfo ti; ti.velocity = 0.8f; addEvent (0, aud, ti); }

        // --- trasporto ---
        int mode = transportMode.load();
        bool play = false; double beat0 = internalBeat; double bpm = tp.bpm > 0 ? tp.bpm : 120.0;
        // il DAW che parte riarma sempre il sequencer, anche dopo uno STOP del plugin
        const bool hostStarted = tp.hostPlaying && ! hostWasPlaying;
        hostWasPlaying = tp.hostPlaying;
        if (hostStarted && mode == Stopped) { transportMode = FollowDaw; mode = FollowDaw; }
        if (mode == FollowDaw && tp.hostPlaying && tp.hostHasPosition) { play = true; beat0 = tp.ppq; }
        else if (mode == AlwaysPlay)
        {
            // PLAY del plugin: suona finché non si preme STOP. Col DAW in riproduzione segue la sua posizione;
            // quando il DAW si ferma continua col clock interno da dove era (prima passava a "segui il DAW" e al primo
            // stop del DAW, anche solo un attimo di play riportato dall'host, restava muto fino a un nuovo PLAY)
            play = true;
            if (tp.hostPlaying && tp.hostHasPosition) beat0 = tp.ppq;
        }
        const double beatsPerSample = bpm / 60.0 / sr;
        const double beat1 = beat0 + beatsPerSample * n;
        if (play && ! wasPlaying)
        {
            seed = hash32 (seed + 0x51u);
            for (auto& s : seqs) s.reset();
            for (auto& v : voices) v.onTransportStart();
            if (mode != FollowDaw || ! tp.hostPlaying) beat0 = internalBeat = 0;
        }
        wasPlaying = play; running = play;
        for (auto& v : vs) v.bpm = (float) bpm;

        if (play)
        {
            for (int v = 0; v < kNumVoices; ++v)
            {
                seqs[v].collect (ss[v], beat0, beat0 + beatsPerSample * n, gs.humanize, seed, [&] (const SeqEvent& e)
                {
                    const int off = (int) std::floor ((e.beat - beat0) / beatsPerSample);
                    addEvent (std::max (0, std::min (n - 1, off)), v, e.info);
                });
            }
            internalBeat = beat0 + beatsPerSample * n;
            displayBeat = internalBeat;
        }
        else for (auto& s : sshared) s.playStep = -1;
        (void) beat1;

        std::stable_sort (events.begin(), events.end(), [] (const Ev& a, const Ev& b) { return a.offset < b.offset; });

        // --- rendering a segmenti fra un evento e l'altro ---
        float* dL = scratch.data(); float* dR = dL + maxN; float* rL = dR + maxN; float* rR = rL + maxN;
        float* xL = rR + maxN; float* xR = xL + maxN; float* xd = xR + maxN; float* xr = xd + maxN;
        std::fill (dL, dL + n, 0.0f); std::fill (dR, dR + n, 0.0f); std::fill (rL, rL + n, 0.0f); std::fill (rR, rR + n, 0.0f);
        bool anySolo = false;
        for (int v = 0; v < kNumVoices; ++v) anySolo |= solo[v].load() || momSolo[v].load();

        size_t ei = 0; int pos = 0;
        while (pos < n)
        {
            while (ei < events.size() && events[ei].offset <= pos) { fire (events[ei], 0); ++ei; }
            const int next = ei < events.size() ? std::min (n, events[ei].offset) : n;
            const int len = next - pos;
            if (len > 0)
            {
                for (int v = 0; v < kNumVoices; ++v)
                {
                    const bool audible = ! (mute[v].load() || momMute[v].load()) && (! anySolo || solo[v].load() || momSolo[v].load());
                    float* oL = outL; float* oR = outR;
                    const int rt = vs[v].route;
                    if (rt > 0 && auxL != nullptr && rt - 1 < kNumOutPairs - 1 && auxL[rt - 1] != nullptr) { oL = auxL[rt - 1]; oR = auxR[rt - 1]; }
                    if (audible) voices[v].render (vs[v], oL + pos, oR + pos, dL + pos, dR + pos, rL + pos, rR + pos, len);
                    else
                    {
                        std::fill (xL, xL + len, 0.0f); std::fill (xR, xR + len, 0.0f);
                        voices[v].render (vs[v], xL, xR, xd, xr, xd, xr, len);
                    }
                }
            }
            pos = next;
        }
        events.clear();

        // --- effetti in mandata + master ---
        const float dTime = gs.dSync ? (float) (lfoDivBeats (gs.dDivIdx) * 60.0 / bpm) : gs.dTimeMs * 0.001f;
        const float rPre = gs.rSync ? (float) (lfoDivBeats (gs.rDivIdx) * 60.0 / bpm) : gs.rPreMs * 0.001f;
        for (int i = 0; i < n; ++i)
        {
            float dl, dr, rl, rr;
            delay.process (dL[i], dR[i], dl, dr, dTime, gs.dFeedback, gs.dGlitch, gs.dFbTilt, gs.dDucking, gs.dSRate, gs.dGain, gs.dStereo, gs.dPong);
            reverb.process (rL[i] + dl * gs.dToRev, rR[i] + dr * gs.dToRev, rl, rr, rPre, gs.rDecay, gs.rSRate, gs.rInTilt, gs.rOutTilt, gs.rGain, gs.rMode);
            float L = outL[i] + dl + rl, R = outR[i] + dr + rr;
            master.process (L, R, gs.maximize, gs.eqLow, gs.eqMid, gs.eqHigh, gs.masterDb, gs.clip);
            outL[i] = finite (L) ? L : 0.0f; outR[i] = finite (R) ? R : 0.0f;
        }
        (void) xd;
    }

    void setTransportMode (int m) { transportMode = m; }

private:
    struct Ev { int offset = 0; int voice = 0; TriggerInfo info; bool choke = false; };
    Voice voices[kNumVoices];
    Sequencer seqs[kNumVoices];
    ShatterDelay delay; HeadspaceReverb reverb; MasterBus master;
    std::vector<Ev> events; std::vector<float> scratch;
    float sr = 48000; int maxN = 512;
    double internalBeat = 0; bool wasPlaying = false, hostWasPlaying = false; uint32_t seed = 0x1234;
    std::atomic<int> pendingAudition { -1 };

    void addEvent (int offset, int v, const TriggerInfo& ti) { Ev e; e.offset = offset; e.voice = v; e.info = ti; events.push_back (e); }

    void fire (const Ev& e, int depth)
    {
        if (e.voice < 0 || e.voice >= kNumVoices || depth > 7) return;
        if (e.choke) { voices[e.voice].choke(); return; }
        voices[e.voice].sample = samples[e.voice];
        const int ck = gs.choke[e.voice];
        if (ck > 0 && ck - 1 != e.voice) voices[ck - 1].choke();
        voices[e.voice].trigger (vs[e.voice], e.info);
        const int lk = gs.link[e.voice];
        if (lk > 0 && lk - 1 != e.voice) { Ev l = e; l.voice = lk - 1; fire (l, depth + 1); }
    }
};
} // namespace mp
