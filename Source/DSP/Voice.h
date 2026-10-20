#pragma once
#include "Common.h"
#include "Mappings.h"
#include "ModSources.h"
#include "Tone.h"
#include "SynthEngines.h"
#include "Sampler.h"
#include <atomic>
#include <memory>

namespace mp
{
// Stato condiviso UI <-> audio per una voce (matrice di modulazione e lucchetti)
struct VoiceShared
{
    std::atomic<float> depth[kNumSources][kNumTargets];
    std::atomic<bool> locked[kNumTargets];
    // valori visualizzati dalla UI (ultimi trigger, valore modulato corrente)
    std::atomic<float> lastRandom[5], lastVelocity[5];
    std::atomic<int> histPos { 0 };
    std::atomic<float> display[kNumTargets];
    std::atomic<float> level { 0 };
    VoiceShared()
    {
        for (auto& s : depth) for (auto& d : s) d = 0.0f;
        for (auto& l : locked) l = false;
        for (auto& r : lastRandom) r = 0;
        for (auto& v : lastVelocity) v = 0;
        for (int t = 0; t < kNumTargets; ++t) display[t] = 0;
    }
};

// Impostazioni non modulabili lette dai parametri a ogni blocco
struct VoiceSettings
{
    bool synthOn = true, sampOn = false, synthEnv2 = false, stereo = false, post = false;
    int engine = EngKick, sampEngine = SmpClassic, sampEnvSrc = 2 /*0 off,1 env1,2 env2*/, filterMode = ScreamLP, distMode = Tape;
    float e1Curve = 1, e1Vel = 0.5f, e2Curve = 1, e2Vel = 0.5f;
    bool l1Sync = true, l2Sync = true, l1Bip = true, l2Bip = true;
    int l1Wave = 0, l2Wave = 0, l1Reset = 0, l2Reset = 0;
    float l1Shape = 0.5f, l2Shape = 0.5f;
    float macro1 = 0, macro2 = 0, variation = 0;
    float modAmp = 1.0f;              // MOD AMP della pagina Performance (0 = nessuna modulazione, 2 = doppia)
    int route = 0;                    // coppia d'uscita (0 = Main 1-2)
    float base[kNumTargets] {};       // valori base normalizzati 0..1
    float bpm = 120;
};

// Informazioni passate a ogni colpo (dal sequencer o dal MIDI)
struct TriggerInfo
{
    float velocity = 1.0f;     // 0..1
    float pitchSt = 0;         // trasposizione extra
    float envScale = 1.0f;     // ENV lane
    float pan = 0;             // offset pan -1..1
    float varAmount = 0;       // VAR lane (si somma a VARIATION)
    float sendDelay = 0, sendReverb = 0;
    float macro2 = -1;         // <0 = nessun override
    bool accent = false;       // VEL al massimo nel sequencer: spinta extra di distorsione
};

class Voice
{
public:
    VoiceShared* shared = nullptr;
    const SampleData* sample = nullptr;          // impostato dal processore (con lock)
    float sr = 48000;

    void prepare (float sampleRate)
    {
        sr = sampleRate;
        for (int i = 0; i < kNumSynthEngines; ++i)
            for (int c = 0; c < 2; ++c) { engines[c][i] = makeEngine (i); engines[c][i]->prepare (sr); engines[c][i]->rng = Rng (0x1000u + (uint32_t) (i * 7 + c * 131)); }
        for (auto& f : filt) f.prepare (sr);
        for (int c = 0; c < 2; ++c) dist[c].prepare (sr, c);
        sampler.prepare (sr);
        ctrlCounter = 0;
    }

    void trigger (const VoiceSettings& s, const TriggerInfo& ti)
    {
        trig = ti;
        const bool wasSilent = ! env1.active() && ! env2.active();
        // sorgenti per-trigger
        randVal = rng.uni();
        velVal = clamp01 (ti.velocity);
        const int hp = (shared->histPos.load() + 1) % 5;
        shared->lastRandom[hp] = randVal; shared->lastVelocity[hp] = velVal; shared->histPos = hp;
        // VARIATION: offset casuale per ogni controllo non bloccato
        const float var = clamp01 (s.variation + ti.varAmount);
        for (int t = 0; t < kNumTargets; ++t)
        {
            float range = 0.18f;
            if (t == T_Gain || t == T_DelaySend || t == T_RevSend || t == T_Blend) range = 0.08f;
            if (t == T_Pitch || t == T_SPitch) range = 0.04f;
            varOff[t] = shared->locked[t].load() ? 0.0f : rng.bi() * var * var * range * 2.0f;
        }
        modMacro2 = ti.macro2;
        computeTargets (s);   // valori aggiornati prima del colpo
        env1.attack = cur[T_E1Attack]; env1.hold = cur[T_E1Hold]; env1.decay = cur[T_E1Decay]; env1.curve = s.e1Curve;
        env2.attack = cur[T_E2Attack]; env2.hold = cur[T_E2Hold]; env2.decay = cur[T_E2Decay]; env2.curve = s.e2Curve;
        env1.timeScale = env2.timeScale = ti.envScale;
        env1.trigger (velVal, s.e1Vel); env2.trigger (velVal, s.e2Vel);
        lfo[0].onTrigger (wasSilent); lfo[1].onTrigger (wasSilent);
        for (int c = 0; c < 2; ++c) engines[c][s.engine]->trigger (velVal);
        { const float sp[4] = { cur[T_SP1], cur[T_SP2], cur[T_SP3], cur[T_SP4] }; sampler.trigger (sample, cur[T_Start], cur[T_End], s.sampEngine, sp); }
        if (wasSilent) { for (auto& f : filt) f.reset(); for (auto& d : dist) d.reset(); }
        choking = false; chokeFade = 1.0f;   // un nuovo colpo annulla un choke in corso
        ctrlCounter = 0;
    }

    void choke() { env1.choke(); env2.choke(); sampler.playing = false; chokeFade = 1.0f; choking = true; }
    void onTransportStart() { lfo[0].onTransportStart(); lfo[1].onTransportStart(); }
    bool isActive() const { return env1.active() || env2.active() || sampler.playing; }

    // Genera nSamples in outL/outR (sommati), più mandate effetto
    void render (const VoiceSettings& s, float* outL, float* outR, float* dlyL, float* dlyR, float* revL, float* revR, int n)
    {
        float peak = 0;
        for (int i = 0; i < n; ++i)
        {
            if (ctrlCounter-- <= 0) { ctrlCounter = 16; controlTick (s, 16.0f / sr); }
            if (! isActive() && ! choking) { continue; }

            const float a1 = env1.advance (1.0f / sr), a2 = env2.advance (1.0f / sr);
            // se l'inviluppo che controlla il sampler è finito, il campione non serve più
            if (sampler.playing && s.sampEnvSrc != 0 && ! (s.sampEnvSrc == 1 ? env1.active() : env2.active())) sampler.playing = false;
            float L = 0, R = 0;
            const float blend = cur[T_Blend];
            const float synthG = std::min (1.0f, 2.0f * (1.0f - blend)), sampG = std::min (1.0f, 2.0f * blend);

            if (s.synthOn && synthG > 0)
            {
                const float amp = s.synthEnv2 ? a2 : a1;
                const float f = baseFreq (s.engine) * semis (cur[T_Pitch] + trig.pitchSt);
                const float synthP[4] = { cur[T_P1], cur[T_P2], cur[T_P3], cur[T_P4] };
                const float x = engines[0][s.engine]->process (f, synthP);
                float y = x;
                if (s.stereo) y = engines[1][s.engine]->process (f * 1.0045f, synthP);
                L += x * amp * synthG; R += y * amp * synthG;
            }
            if (s.sampOn && sampG > 0)
            {
                float sl, srr;
                const float sp[4] = { cur[T_SP1], cur[T_SP2], cur[T_SP3], cur[T_SP4] };
                sampler.process (sample, cur[T_Start], cur[T_End], cur[T_SPitch] + trig.pitchSt, sp, sl, srr);
                const float amp = s.sampEnvSrc == 0 ? 1.0f : (s.sampEnvSrc == 1 ? a1 : a2);
                L += sl * amp * sampG; R += srr * amp * sampG;
            }
            // blocco della componente continua (alcuni motori a retroazione la generano)
            dcBlock[0].setCutoff (12.0f, sr); dcBlock[1].a = dcBlock[0].a;
            L = dcBlock[0].hp (L); R = dcBlock[1].hp (R);
            // TONE: filtro + distorsione (ordine selezionabile)
            float* ch[2] = { &L, &R };
            for (int c = 0; c < 2; ++c)
            {
                float v = *ch[c];
                if (s.post) { v = filt[c].process (v, s.filterMode, cur[T_Cutoff], cur[T_Res]); v = dist[c].process (v, s.distMode, curDist); }
                else { v = dist[c].process (v, s.distMode, curDist); v = filt[c].process (v, s.filterMode, cur[T_Cutoff], cur[T_Res]); }
                *ch[c] = finite (v) ? v : 0.0f;
            }
            if (choking) { chokeFade *= 0.997f; L *= chokeFade; R *= chokeFade; if (chokeFade < 0.001f) { choking = false; for (auto& f : filt) f.reset(); } }

            const float g = dbToGain (cur[T_Gain]);
            const float pan = clampf (cur[T_Pan] + trig.pan, -1, 1);
            const float pl = std::cos ((pan + 1.0f) * kPi * 0.25f) * 1.4142f, pr = std::sin ((pan + 1.0f) * kPi * 0.25f) * 1.4142f;
            const float oL = L * g * std::min (1.0f, pl), oR = R * g * std::min (1.0f, pr);
            outL[i] += oL; outR[i] += oR;
            const float ds = clamp01 (cur[T_DelaySend] + trig.sendDelay), rs = clamp01 (cur[T_RevSend] + trig.sendReverb);
            dlyL[i] += oL * ds * ds; dlyR[i] += oR * ds * ds;
            revL[i] += oL * rs * rs; revR[i] += oR * rs * rs;
            peak = std::max (peak, std::max (std::abs (oL), std::abs (oR)));
        }
        const float old = shared->level.load();
        shared->level = std::max (peak, old * 0.85f);
    }

    static float baseFreq (int engine) { return engineInfo (engine).octaveDown ? 65.41f : 130.81f; }

private:
    std::unique_ptr<Engine> engines[2][kNumSynthEngines];
    SamplerVoice sampler;
    ToneFilter filt[2]; Distortion dist[2]; OnePole dcBlock[2];
    AhdEnvelope env1, env2; Lfo lfo[2];
    TriggerInfo trig;
    Rng rng { 0xC0FFEE };
    float randVal = 0, velVal = 0, lfoVal[2] {}, modMacro2 = -1;
    float varOff[kNumTargets] {}, cur[kNumTargets] {}, curDist = 0;
    int ctrlCounter = 0; bool choking = false; float chokeFade = 1;

    void computeTargets (const VoiceSettings& s)
    {
        float src[kNumSources];
        src[S_Random] = randVal; src[S_Velocity] = velVal;
        src[S_Macro1] = s.macro1; src[S_Macro2] = modMacro2 >= 0 ? modMacro2 : s.macro2;
        src[S_Env1] = env1.value; src[S_Env2] = env2.value;
        src[S_Lfo1] = lfoVal[0]; src[S_Lfo2] = lfoVal[1];
        for (int t = 0; t < kNumTargets; ++t)
        {
            float n = s.base[t] + varOff[t];
            for (int k = 0; k < kNumSources; ++k)
            {
                const float d = shared->depth[k][t].load (std::memory_order_relaxed);
                if (d != 0.0f) n += d * src[k] * s.modAmp;
            }
            n = clamp01 (n);
            shared->display[t].store (n, std::memory_order_relaxed);
            cur[t] = denorm (t, n);
        }
        curDist = clamp01 (cur[T_Distort] + (trig.accent ? 0.3f : 0.0f));
    }

    void controlTick (const VoiceSettings& s, float dt)
    {
        // LFO
        for (int k = 0; k < 2; ++k)
        {
            Lfo& l = lfo[k];
            l.shape = k == 0 ? s.l1Shape : s.l2Shape;
            l.bipolar = k == 0 ? s.l1Bip : s.l2Bip;
            l.wave = k == 0 ? s.l1Wave : s.l2Wave;
            l.resetMode = k == 0 ? s.l1Reset : s.l2Reset;
            l.amp = cur[k == 0 ? T_L1Amp : T_L2Amp];
            const float spN = cur[k == 0 ? T_L1Speed : T_L2Speed];
            const bool sync = k == 0 ? s.l1Sync : s.l2Sync;
            const float hz = sync ? (s.bpm / 60.0f) / lfoDivBeats (lfoDivIndex (spN)) : lfoHz (spN);
            lfoVal[k] = l.advance (dt, hz);
        }
        computeTargets (s);
    }
};
} // namespace mp
