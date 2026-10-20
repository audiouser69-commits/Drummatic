#pragma once
// Drummatic hardware: logica del pannello (pagine, encoder, tasti, LED).
// Il circuito di controllo parla USB-MIDI sul canale 16 (vedi firmware Pico 2):
//   tasti   -> note 0..45 (on/off)        pulsanti encoder -> note 64..71, DATA 72
//   encoder -> CC 16..23, DATA CC 24, relativi (1..63 +, 65..127 -)    volume -> CC 7
//   LED     <- note on n, velocity = colore (0 spento, 1..15 tavolozza, +16 meta luce)
#include "../PluginProcessor.h"

namespace hw
{
// indici dei tasti (stesso ordine del firmware / keymap.json)
enum Key
{
    K_VOICE, K_SMPL, K_TONE, K_MOD, K_SEQ, K_LANE, K_MIX, K_PERF,
    K_FUNC, K_MAP, K_LOCK, K_DADO, K_COMMIT, K_KIT, K_COPY,
    K_V1, K_V2, K_V3, K_V4, K_V5, K_V6, K_V7, K_V8,
    K_REC, K_PLAY, K_STOP, K_LEFT, K_UP, K_DOWN, K_RIGHT,
    K_T1, K_T16 = K_T1 + 15,
    kNumKeys
};
enum Page { P_Voice, P_Smpl, P_Tone, P_Mod, P_Seq, P_Lane, P_Mix, P_Perf, kNumPages };
inline const char* pageName (int p) { static const char* n[] = { "VOICE", "SAMPLER", "TONE", "MOD", "SEQ", "LANE", "MIX", "PERF" }; return n[p]; }

// colori della tavolozza del firmware
enum Led { L_Off = 0, L_White = 1, L_Red = 2, L_Orange = 3, L_Amber = 4, L_Yellow = 5, L_Lime = 6, L_Green = 7,
           L_Cyan = 9, L_Blue = 11, L_Violet = 12, L_Magenta = 13, L_Half = 16 };

constexpr int kMidiCh = 16;
constexpr int kEncCC0 = 16, kDataCC = 24, kVolumeCC = 7, kEncNote0 = 64, kDataNote = 72;

// ---------------------------------------------------------------- slot di un encoder
struct Slot
{
    enum Kind { None, Param, VoiceParam, LaneValue, LaneLength, LaneSync, SampleFile };
    Kind kind = None;
    juce::String id;          // id del parametro (per VoiceParam: nome senza prefisso voce)
    juce::String label;
    int voice = -1;           // VoiceParam: -1 = voce selezionata, altrimenti voce fissa (pagina MIX)
    int target = -1;          // indice mp::Target se modulabile (per MAP e LOCK)
    int perf = -1;            // indice controllo Performance (per LOCK)
    static Slot vp (const char* id, const char* label, int target = -1) { Slot s; s.kind = VoiceParam; s.id = id; s.label = label; s.target = target; return s; }
    static Slot vt (int t, const char* label) { return vp (mpp::targetId (t), label, t); }
    static Slot fixed (int v, int t, const char* label) { Slot s = vt (t, label); s.voice = v; return s; }
    static Slot gp (const char* id, const char* label) { Slot s; s.kind = Param; s.id = id; s.label = label; return s; }
    static Slot pf (int i, const char* label) { Slot s; s.kind = Param; s.id = mpp::perfId (i); s.label = label; s.perf = i; return s; }
    static Slot special (Kind k, const char* label) { Slot s; s.kind = k; s.label = label; return s; }
};
using Bank = std::array<Slot, 8>;

struct PageDef { juce::String bankNames[8]; std::vector<Bank> banks; };

inline std::vector<PageDef> makePages()
{
    using S = Slot;
    using namespace mp;
    std::vector<PageDef> P (kNumPages);
    P[P_Voice].banks = {
        Bank { S::vt (T_Pitch, "PITCH"), S::vt (T_P1, "SYNTH 1"), S::vt (T_P2, "SYNTH 2"), S::vt (T_P3, "SYNTH 3"),
               S::vt (T_P4, "SYNTH 4"), S::vt (T_Blend, "BLEND"), S::vt (T_Gain, "GAIN"), S::vt (T_Pan, "PAN") },
        Bank { S::vp ("synthOn", "SYNTH ON"), S::vp ("env2", "AMP ENV2"), S::vp ("stereo", "STEREO"), S::vp ("variation", "VARIATION"),
               S::vp ("macro1", "MACRO 1"), S::vp ("macro2", "MACRO 2"), S::vp ("choke", "CHOKE"), S::vp ("link", "TRIGGER") } };
    P[P_Voice].bankNames[0] = "SINTESI"; P[P_Voice].bankNames[1] = "VOCE";
    P[P_Smpl].banks = {
        Bank { S::vt (T_SPitch, "PITCH"), S::vt (T_SP1, "SAMPLER 1"), S::vt (T_SP2, "SAMPLER 2"), S::vt (T_SP3, "SAMPLER 3"),
               S::vt (T_SP4, "SAMPLER 4"), S::vt (T_Start, "START"), S::vt (T_End, "END"), S::vt (T_Blend, "BLEND") },
        Bank { S::special (S::SampleFile, "FILE"), S::vp ("sampOn", "SAMPLER ON"), S::vp ("sampEng", "MOTORE"), S::vp ("sampEnv", "INVILUPPO"),
               S::vt (T_Gain, "GAIN"), S::vt (T_Pan, "PAN"), S::vt (T_DelaySend, "DELAY"), S::vt (T_RevSend, "REVERB") } };
    P[P_Smpl].bankNames[0] = "SUONO"; P[P_Smpl].bankNames[1] = "FILE";
    P[P_Tone].banks = {
        Bank { S::vt (T_Cutoff, "CUTOFF"), S::vt (T_Res, "RESONANCE"), S::vp ("fmode", "FILTRO"), S::vt (T_Distort, "DISTORT"),
               S::vp ("dmode", "DISTORSIONE"), S::vp ("post", "POST"), S::vt (T_DelaySend, "DELAY"), S::vt (T_RevSend, "REVERB") } };
    P[P_Tone].bankNames[0] = "FILTRO E DIST.";
    P[P_Mod].banks = {
        Bank { S::vt (T_E1Attack, "ENV1 ATK"), S::vt (T_E1Hold, "ENV1 HOLD"), S::vt (T_E1Decay, "ENV1 DECAY"), S::vp ("e1curve", "ENV1 CURVA"),
               S::vt (T_E2Attack, "ENV2 ATK"), S::vt (T_E2Hold, "ENV2 HOLD"), S::vt (T_E2Decay, "ENV2 DECAY"), S::vp ("e2curve", "ENV2 CURVA") },
        Bank { S::vt (T_L1Speed, "LFO1 SPEED"), S::vt (T_L1Amp, "LFO1 AMP"), S::vp ("l1wave", "LFO1 FORMA"), S::vp ("l1shape", "LFO1 SHAPE"),
               S::vt (T_L2Speed, "LFO2 SPEED"), S::vt (T_L2Amp, "LFO2 AMP"), S::vp ("l2wave", "LFO2 FORMA"), S::vp ("l2shape", "LFO2 SHAPE") },
        Bank { S::vp ("e1vel", "ENV1 VEL"), S::vp ("e2vel", "ENV2 VEL"), S::vp ("l1sync", "LFO1 SYNC"), S::vp ("l1reset", "LFO1 RESET"),
               S::vp ("l2sync", "LFO2 SYNC"), S::vp ("l2reset", "LFO2 RESET"), S::vp ("l1bip", "LFO1 BIPOL."), S::vp ("l2bip", "LFO2 BIPOL.") } };
    P[P_Mod].bankNames[0] = "INVILUPPI"; P[P_Mod].bankNames[1] = "LFO"; P[P_Mod].bankNames[2] = "OPZIONI";
    P[P_Seq].banks = {
        Bank { S::vp ("steps", "STEPS"), S::vp ("fill", "FILL"), S::vp ("pattern", "PATTERN"), S::vp ("rotate", "ROTATE"),
               S::vp ("density", "DENSITY"), S::vp ("swing", "SWING"), S::vp ("sshift", "SHIFT"), S::vp ("shape", "SHAPE") },
        Bank { S::vp ("speed", "SPEED"), S::vp ("mult", "MULT"), S::vp ("length", "LENGTH"), S::vp ("invert", "INVERT"),
               S::vp ("snap", "SNAP"), S::vp ("resetOn", "RESET"), S::vp ("resetLen", "RESET LEN"), S::gp ("humanize", "HUMANIZE") } };
    P[P_Seq].bankNames[0] = "EUCLIDEO"; P[P_Seq].bankNames[1] = "TEMPO";
    P[P_Lane].banks = {
        Bank { S::special (S::LaneValue, "VALORE"), S::special (S::LaneLength, "LUNGHEZZA"), S::special (S::LaneSync, "SYNC"), S{}, S{}, S{}, S{}, S{} } };
    P[P_Lane].bankNames[0] = "LANE";
    Bank gains, pans, dsends, rsends;
    for (int v = 0; v < kNumVoices; ++v)
    {
        static const char* nums[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
        gains[(size_t) v] = S::fixed (v, T_Gain, nums[v]); pans[(size_t) v] = S::fixed (v, T_Pan, nums[v]);
        dsends[(size_t) v] = S::fixed (v, T_DelaySend, nums[v]); rsends[(size_t) v] = S::fixed (v, T_RevSend, nums[v]);
    }
    P[P_Mix].banks = { gains, pans, dsends, rsends,
        Bank { S::gp ("d_time", "DLY TIME"), S::gp ("d_fb", "FEEDBACK"), S::gp ("d_glitch", "GLITCH"), S::gp ("d_fbtilt", "FB TILT"),
               S::gp ("d_duck", "DUCKING"), S::gp ("d_srate", "S.RATE"), S::gp ("d_torev", "> REVERB"), S::gp ("d_gain", "DLY GAIN") },
        Bank { S::gp ("r_pre", "PREDELAY"), S::gp ("r_decay", "DECAY"), S::gp ("r_srate", "S.RATE"), S::gp ("r_intilt", "IN TILT"),
               S::gp ("r_outtilt", "OUT TILT"), S::gp ("r_mode", "MODO"), S::gp ("r_gain", "REV GAIN"), S::gp ("d_sync", "DLY SYNC") },
        Bank { S::gp ("m_max", "MAXIMIZE"), S::gp ("m_low", "EQ LOW"), S::gp ("m_mid", "EQ MID"), S::gp ("m_high", "EQ HIGH"),
               S::gp ("m_clip", "CLIP"), S::gp ("m_gain", "MASTER"), S::gp ("bpm", "TEMPO"), S::gp ("humanize", "HUMANIZE") } };
    const char* mixNames[] = { "VOLUMI", "PAN", "MANDATA DELAY", "MANDATA REVERB", "DELAY", "REVERB", "MASTER" };
    for (int i = 0; i < 7; ++i) P[P_Mix].bankNames[i] = mixNames[i];
    using namespace mpp;
    P[P_Perf].banks = {
        Bank { S::pf (PF_EnvA, "ATTACK"), S::pf (PF_EnvH, "HOLD"), S::pf (PF_EnvD, "DECAY"), S::pf (PF_EnvC, "CURVA"),
               S::pf (PF_Macro1, "MACRO 1"), S::pf (PF_Macro2, "MACRO 2"), S::pf (PF_Var, "VARIATION"), S::pf (PF_ModAmp, "MOD AMP") },
        Bank { S::pf (PF_SynPitch, "SYN PITCH"), S::pf (PF_Syn1, "SYN 1"), S::pf (PF_Syn2, "SYN 2"), S::pf (PF_Syn3, "SYN 3"),
               S::pf (PF_Syn4, "SYN 4"), S::pf (PF_Distort, "DISTORT"), S::pf (PF_Cutoff, "CUTOFF"), S::pf (PF_Res, "RES") },
        Bank { S::pf (PF_SmpPitch, "SMP PITCH"), S::pf (PF_Smp1, "SMP 1"), S::pf (PF_Smp2, "SMP 2"), S::pf (PF_Smp3, "SMP 3"),
               S::pf (PF_Smp4, "SMP 4"), S::gp ("pf_depth", "DEPTH"), S::gp ("pf_rnd", "DADO DEPTH"), S::gp ("pf_enabled", "ATTIVA") },
        Bank { S::pf (PF_Steps, "STEPS"), S::pf (PF_Fill, "FILL"), S::pf (PF_Pattern, "PATTERN"), S::pf (PF_Density, "DENSITY"),
               S::pf (PF_Prob, "PROB"), S::pf (PF_Ratchet, "RATCHET"), S::gp ("rnd_depth", "RND DEPTH"), S::gp ("pf_depth", "DEPTH") } };
    const char* pfNames[] = { "INVILUPPO E MACRO", "SINTESI", "SAMPLER", "SEQUENCER" };
    for (int i = 0; i < 4; ++i) P[P_Perf].bankNames[i] = pfNames[i];
    return P;
}

inline mp::SeqSettings seqSettings (ManipoloProcessor& P, int v)
{
    auto iv = [&] (const char* n) { return juce::roundToInt (P.param (v, n)->convertFrom0to1 (P.param (v, n)->getValue())); };
    auto bv = [&] (const char* n) { return P.param (v, n)->getValue() > 0.5f; };
    mp::SeqSettings s;
    s.steps = iv ("steps"); s.fill = iv ("fill"); s.pattern = iv ("pattern"); s.rotate = iv ("rotate"); s.invert = bv ("invert");
    s.length = iv ("length"); s.snap = bv ("snap"); s.resetOn = bv ("resetOn"); s.resetLen = iv ("resetLen");
    return s;
}

// ---------------------------------------------------------------- controller
class Controller
{
public:
    explicit Controller (ManipoloProcessor& p) : proc (p), pages (makePages())
    {
        bank.fill (0);
        rescanSamples();
    }

    ManipoloProcessor& proc;
    const std::vector<PageDef> pages;

    // stato visibile dallo schermo
    int page = P_Voice, voice = 0, stepPage = 0, lane = mp::L_Vel, laneStep = 0, mapSource = mp::S_Macro1;
    std::array<int, kNumPages> bank;
    bool func = false, mapMode = false, recArmed = false;
    int lastSlot = -1; juce::uint32 lastTouchMs = 0;
    juce::String toast; juce::uint32 toastMs = 0;
    std::atomic<float> volume { 0.8f };          // dal potenziometro (applicato dall'app all'uscita)
    juce::Array<juce::File> sampleList; int sampleIdx = -1;
    std::function<void (const juce::MidiMessage&)> sendToPanel;     // verso il circuito (LED)

    const Bank& currentBank() const { return pages[(size_t) page].banks[(size_t) bank[(size_t) page]]; }
    int numBanks() const { return (int) pages[(size_t) page].banks.size(); }
    juce::String bankName() const { return pages[(size_t) page].bankNames[bank[(size_t) page]]; }

    // ------------------------------------------------ ingresso dal pannello
    void handle (const juce::MidiMessage& m)
    {
        if (m.getChannel() != kMidiCh) return;
        if (m.isNoteOn()) noteOn (m.getNoteNumber());
        else if (m.isNoteOff()) noteOff (m.getNoteNumber());
        else if (m.isController())
        {
            const int cc = m.getControllerNumber(), v = m.getControllerValue();
            if (cc == kVolumeCC) { volume = (float) v / 127.0f; return; }
            const int d = v < 64 ? v : v - 128;
            if (cc >= kEncCC0 && cc < kEncCC0 + 8) turn (cc - kEncCC0, d);
            else if (cc == kDataCC) turnData (d);
        }
    }

    void noteOn (int n)
    {
        if (n >= kEncNote0 && n < kEncNote0 + 8) { resetSlot (n - kEncNote0); return; }
        if (n == kDataNote) { proc.dm.auditionVoice (voice); return; }
        if (n < 0 || n >= kNumKeys) return;
        held[(size_t) n] = true;
        if (n <= K_PERF) { page = n; mapMode = mapMode && page != P_Lane && page != P_Mix; return; }
        if (n >= K_V1 && n <= K_V8) { voiceKey (n - K_V1); return; }
        if (n >= K_T1 && n <= K_T16) { trigKey (n - K_T1); return; }
        switch (n)
        {
            case K_FUNC: func = true; break;
            case K_MAP: mapMode = ! mapMode; say (mapMode ? juce::String ("MAP: ") + mp::sourceName (mapSource) : "MAP spento"); break;
            case K_LOCK: toggleLock(); break;
            case K_DADO:
                if (page == P_Perf) { proc.perfRandomize(); say ("Performance: dado"); }
                else if (func) { proc.randomizeAll(); say ("Dado su tutte le voci"); }
                else { proc.randomizeVoice (voice); say ("Dado voce " + juce::String (voice + 1)); }
                break;
            case K_COMMIT:
                if (func) { proc.perfReset(); say ("Performance azzerata"); } else { proc.perfCommit(); say ("Performance fissata"); }
                break;
            case K_KIT:
                proc.stepKit (func ? -1 : 1);
                say ("Kit: " + (proc.currentKit.existsAsFile() ? proc.currentKit.getFileNameWithoutExtension() : juce::String ("-")));
                break;
            case K_COPY:
                if (func) { if (clip.isValid()) { proc.voiceFromTree (voice, clip); say ("Incollato su voce " + juce::String (voice + 1)); } }
                else { clip = proc.voiceToTree (voice); say ("Copiata voce " + juce::String (voice + 1)); }
                break;
            case K_REC:
                if (func) saveKit(); else { recArmed = ! recArmed; say (recArmed ? "REC: suona le voci" : "REC spento"); }
                break;
            case K_PLAY:
                proc.dm.setTransportMode (proc.dm.transportMode.load() == mp::DrumMachine::AlwaysPlay ? mp::DrumMachine::Stopped : mp::DrumMachine::AlwaysPlay); break;
            case K_STOP: proc.dm.setTransportMode (mp::DrumMachine::Stopped); recArmed = false; break;
            case K_LEFT:
                if (func) stepPage = std::max (0, stepPage - 1); else bank[(size_t) page] = std::max (0, bank[(size_t) page] - 1);
                break;
            case K_RIGHT:
                if (func) stepPage = std::min (numStepPages() - 1, stepPage + 1); else bank[(size_t) page] = std::min (numBanks() - 1, bank[(size_t) page] + 1);
                break;
            case K_UP: selectVoice (voice - 1); break;
            case K_DOWN: selectVoice (voice + 1); break;
            default: break;
        }
    }
    void noteOff (int n)
    {
        if (n < 0 || n >= kNumKeys) return;
        held[(size_t) n] = false;
        if (n == K_FUNC) func = false;
    }

    void selectVoice (int v)
    {
        voice = juce::jlimit (0, mp::kNumVoices - 1, v);
        stepPage = std::min (stepPage, numStepPages() - 1);
        rescanSampleIndex();
    }

    void voiceKey (int v)
    {
        if (func) { proc.dm.mute[v] = ! proc.dm.mute[v].load(); return; }
        if (page == P_Mix) { proc.dm.mute[v] = ! proc.dm.mute[v].load(); return; }
        if (page == P_Perf) { auto* p = proc.param (v, "pfEnable"); setNorm (p, p->getValue() > 0.5f ? 0.0f : 1.0f); return; }
        selectVoice (v);
        proc.dm.auditionVoice (v);
        if (recArmed && proc.dm.running.load())
        {
            const int s = proc.dm.sshared[v].playStep.load();
            if (s >= 0) proc.dm.sshared[v].manual[s] = true;
        }
    }

    void trigKey (int i)
    {
        const int s = stepPage * 16 + i;
        if (s >= seqLength()) return;
        if (page == P_Lane) { laneStep = s; return; }
        auto& m = proc.dm.sshared[voice].manual[s];
        m = ! m.load();
    }

    // ------------------------------------------------ encoder
    void turn (int slotIdx, int d)
    {
        const Slot& s = currentBank()[(size_t) slotIdx];
        lastSlot = slotIdx; lastTouchMs = juce::Time::getMillisecondCounter();
        const float acc = std::abs (d) >= 3 ? 2.5f : 1.0f;
        if (mapMode && s.target >= 0)
        {
            auto& dep = proc.dm.vshared[slotVoice (s)].depth[mapSource][s.target];
            dep = juce::jlimit (-1.0f, 1.0f, dep.load() + d * (func ? 0.002f : 0.01f) * acc);
            return;
        }
        switch (s.kind)
        {
            case Slot::Param: case Slot::VoiceParam: nudge (paramOf (s), d, acc); break;
            case Slot::LaneValue:
            {
                const auto& li = mp::laneInfo (lane);
                auto& cell = proc.dm.sshared[voice].lane[lane][laneStep];
                const float step = li.integer ? (float) d : d * (li.hi - li.lo) / 100.0f;
                float v = juce::jlimit (li.lo, li.hi, cell.load() + step * (func || li.hi - li.lo < 20 ? 1.0f : acc));
                cell = li.integer ? std::round (v) : v;
                break;
            }
            case Slot::LaneLength: { auto& l = proc.dm.sshared[voice].laneLen[lane]; l = juce::jlimit (1, mp::kMaxSteps, l.load() + d); break; }
            case Slot::LaneSync: { auto& l = proc.dm.sshared[voice].laneSync[lane]; l = (l.load() + (d > 0 ? 1 : 3)) % 4; break; }
            case Slot::SampleFile: browseSample (d); break;
            default: break;
        }
    }

    void turnData (int d)
    {
        if (mapMode) { mapSource = (mapSource + (d > 0 ? 1 : mp::kNumSources - 1)) % mp::kNumSources; say (juce::String ("MAP: ") + mp::sourceName (mapSource)); return; }
        switch (page)
        {
            case P_Voice: nudge (proc.param (voice, "engine"), d, 1); break;
            case P_Smpl: browseSample (d); break;
            case P_Tone: nudge (proc.param (voice, "fmode"), d, 1); break;
            case P_Mod: mapSource = (mapSource + (d > 0 ? 1 : mp::kNumSources - 1)) % mp::kNumSources; say (juce::String ("Sorgente: ") + mp::sourceName (mapSource)); break;
            case P_Seq: stepPage = juce::jlimit (0, numStepPages() - 1, stepPage + (d > 0 ? 1 : -1)); break;
            case P_Lane: lane = (lane + (d > 0 ? 1 : mp::kNumLanes - 1)) % mp::kNumLanes; break;
            case P_Mix: nudge (proc.apvts.getParameter ("bpm"), d, 1); break;
            case P_Perf: nudge (proc.apvts.getParameter ("pf_depth"), d, 1); break;
            default: break;
        }
    }

    void resetSlot (int slotIdx)
    {
        const Slot& s = currentBank()[(size_t) slotIdx];
        lastSlot = slotIdx; lastTouchMs = juce::Time::getMillisecondCounter();
        if (mapMode && s.target >= 0) { proc.dm.vshared[slotVoice (s)].depth[mapSource][s.target] = 0.0f; return; }
        if (auto* p = paramOf (s)) setNorm (p, p->getDefaultValue());
        else if (s.kind == Slot::LaneValue) proc.dm.sshared[voice].lane[lane][laneStep] = mp::laneInfo (lane).def;
    }

    void toggleLock()
    {
        if (lastSlot < 0) return;
        const Slot& s = currentBank()[(size_t) lastSlot];
        if (s.target >= 0) { auto& l = proc.dm.vshared[slotVoice (s)].locked[s.target]; l = ! l.load(); say (s.label + (l.load() ? " bloccato" : " sbloccato")); }
        else if (s.perf >= 0) { auto& l = proc.perfLock[s.perf]; l = ! l.load(); say (s.label + (l.load() ? " bloccato" : " sbloccato")); }
    }
    bool lastSlotLocked() const
    {
        if (lastSlot < 0) return false;
        const Slot& s = currentBank()[(size_t) lastSlot];
        if (s.target >= 0) return proc.dm.vshared[slotVoice (s)].locked[s.target].load();
        if (s.perf >= 0) return proc.perfLock[s.perf].load();
        return false;
    }

    // ------------------------------------------------ valori per lo schermo
    int slotVoice (const Slot& s) const { return s.voice >= 0 ? s.voice : voice; }
    juce::RangedAudioParameter* paramOf (const Slot& s) const
    {
        if (s.kind == Slot::Param) return proc.apvts.getParameter (s.id);
        if (s.kind == Slot::VoiceParam) return proc.param (slotVoice (s), s.id);
        return nullptr;
    }
    float slotNorm (const Slot& s) const
    {
        if (auto* p = paramOf (s)) return p->getValue();
        if (s.kind == Slot::LaneValue) { const auto& li = mp::laneInfo (lane); return (proc.dm.sshared[voice].lane[lane][laneStep].load() - li.lo) / (li.hi - li.lo); }
        if (s.kind == Slot::LaneLength) return proc.dm.sshared[voice].laneLen[lane].load() / (float) mp::kMaxSteps;
        if (s.kind == Slot::LaneSync) return proc.dm.sshared[voice].laneSync[lane].load() / 3.0f;
        if (s.kind == Slot::SampleFile) return sampleList.isEmpty() ? 0.0f : (float) std::max (0, sampleIdx) / (float) std::max (1, sampleList.size() - 1);
        return 0.0f;
    }
    juce::String slotText (const Slot& s) const
    {
        if (auto* p = paramOf (s)) return p->getCurrentValueAsText();
        if (s.kind == Slot::LaneValue) return juce::String (juce::roundToInt (proc.dm.sshared[voice].lane[lane][laneStep].load()));
        if (s.kind == Slot::LaneLength) return juce::String (proc.dm.sshared[voice].laneLen[lane].load());
        if (s.kind == Slot::LaneSync) { static const char* n[] = { "SYNC", "FREE", "TRIG", "RETRIG" }; return n[proc.dm.sshared[voice].laneSync[lane].load() & 3]; }
        if (s.kind == Slot::SampleFile) { auto f = proc.getSampleFile (voice); return f.existsAsFile() ? f.getFileNameWithoutExtension() : juce::String ("nessuno"); }
        return {};
    }
    float slotDepth (const Slot& s) const { return s.target >= 0 ? proc.dm.vshared[slotVoice (s)].depth[mapSource][s.target].load() : 0.0f; }
    bool slotBipolar (const Slot& s) const
    {
        if (auto* p = paramOf (s)) { auto r = p->getNormalisableRange(); return r.start < 0 && r.end > 0; }
        return s.kind == Slot::LaneValue && mp::laneInfo (lane).lo < 0;
    }

    int seqLength() const { return std::max (1, mp::sequenceLength (seqSettings (proc, voice))); }
    int numStepPages() const { return (seqLength() + 15) / 16; }
    bool stepOn (int s) const { return proc.dm.sshared[voice].manual[s].load(); }
    bool stepPattern (int s) const { auto st = seqSettings (proc, voice); return mp::patternHit (st, s % std::max (1, st.steps)); }

    bool showToast() const { return toast.isNotEmpty() && juce::Time::getMillisecondCounter() - toastMs < 1800; }
    juce::String engineName() const
    {
        auto* p = proc.param (voice, "engine");
        return p->getCurrentValueAsText();
    }

    // ------------------------------------------------ LED
    std::array<juce::uint8, kNumKeys> ledColours() const
    {
        std::array<juce::uint8, kNumKeys> c {};
        for (int p = 0; p < kNumPages; ++p) c[(size_t) p] = (juce::uint8) (p == page ? L_White : L_White + L_Half);
        c[K_FUNC] = (juce::uint8) (func ? L_Orange : L_White + L_Half);
        c[K_MAP] = (juce::uint8) (mapMode ? L_Cyan : L_White + L_Half);
        c[K_LOCK] = (juce::uint8) (lastSlotLocked() ? L_Red : L_White + L_Half);
        c[K_DADO] = c[K_COMMIT] = c[K_KIT] = (juce::uint8) (L_White + L_Half);
        c[K_COPY] = (juce::uint8) (clip.isValid() ? L_Violet + L_Half : L_White + L_Half);
        bool anySolo = false;
        for (int v = 0; v < mp::kNumVoices; ++v) anySolo |= proc.dm.solo[v].load();
        for (int v = 0; v < mp::kNumVoices; ++v)
        {
            const bool muted = proc.dm.mute[v].load() || (anySolo && ! proc.dm.solo[v].load());
            const bool hit = proc.dm.vshared[v].level.load() > 0.05f;
            int col = v == voice ? L_Orange : L_White + L_Half;
            if (page == P_Perf) col = proc.param (v, "pfEnable")->getValue() > 0.5f ? (v == voice ? L_Blue : L_Blue + L_Half) : L_Off;
            if (muted) col = page == P_Mix || func ? L_Red : L_Red + L_Half;
            if (hit && ! muted) col = v == voice ? L_Yellow : L_Amber;
            c[(size_t) (K_V1 + v)] = (juce::uint8) col;
        }
        const bool running = proc.dm.running.load();
        c[K_REC] = (juce::uint8) (recArmed ? L_Red : L_Red + L_Half);
        c[K_PLAY] = (juce::uint8) (running ? L_Green : L_Green + L_Half);
        c[K_STOP] = (juce::uint8) (running ? L_White + L_Half : L_White);
        c[K_LEFT] = c[K_RIGHT] = c[K_UP] = c[K_DOWN] = (juce::uint8) (L_White + L_Half);
        const int L = seqLength(), play = proc.dm.sshared[voice].playStep.load();
        for (int i = 0; i < 16; ++i)
        {
            const int s = stepPage * 16 + i;
            int col = L_Off;
            if (s < L)
            {
                if (page == P_Lane) col = s == laneStep ? L_Cyan : (std::abs (proc.dm.sshared[voice].lane[lane][s].load() - mp::laneInfo (lane).def) > 0.01f ? L_Cyan + L_Half : L_Off);
                else if (stepOn (s)) col = L_Orange;
                else if (stepPattern (s)) col = L_Yellow + L_Half;
                if (running && s == play) col = L_White;
            }
            c[(size_t) (K_T1 + i)] = (juce::uint8) col;
        }
        return c;
    }

    /** chiamata ~30 volte al secondo: invia al circuito solo i LED cambiati */
    void refreshLeds (bool force = false)
    {
        if (! sendToPanel) return;
        const auto c = ledColours();
        for (int k = 0; k < kNumKeys; ++k)
            if (force || c[(size_t) k] != sentLeds[(size_t) k])
            {
                sendToPanel (juce::MidiMessage::noteOn (kMidiCh, k, (juce::uint8) c[(size_t) k]));
                sentLeds[(size_t) k] = c[(size_t) k];
            }
    }

    // ------------------------------------------------ campioni
    void rescanSamples()
    {
        sampleList.clear();
        auto dir = ManipoloProcessor::userFolder().getChildFile ("Samples");
        for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3"))
            sampleList.add (f);
        struct Cmp { int compareElements (const juce::File& a, const juce::File& b) const { return a.getFullPathName().compareNatural (b.getFullPathName()); } } cmp;
        sampleList.sort (cmp);
        rescanSampleIndex();
    }
    void rescanSampleIndex() { sampleIdx = sampleList.indexOf (proc.getSampleFile (voice)); }
    void browseSample (int d)
    {
        if (sampleList.isEmpty()) { say ("Nessun campione in Drummatic/Samples"); return; }
        sampleIdx = sampleIdx < 0 ? (d > 0 ? 0 : sampleList.size() - 1) : (sampleIdx + d % sampleList.size() + sampleList.size()) % sampleList.size();
        pendingLoad = sampleIdx; pendingLoadMs = juce::Time::getMillisecondCounter();
    }
    /** carica il campione scelto quando l'encoder si ferma (evita di leggere ogni file mentre si scorre) */
    void tick()
    {
        if (pendingLoad >= 0 && juce::Time::getMillisecondCounter() - pendingLoadMs > 200)
        {
            const auto f = sampleList[pendingLoad];
            pendingLoad = -1;
            if (proc.loadSample (voice, f)) { auto* on = proc.param (voice, "sampOn"); if (on->getValue() < 0.5f) setNorm (on, 1.0f); }
            rescanSampleIndex();
        }
    }
    juce::String pendingSampleName() const { return pendingLoad >= 0 ? sampleList[pendingLoad].getFileNameWithoutExtension() : juce::String(); }

    void say (const juce::String& s) { toast = s; toastMs = juce::Time::getMillisecondCounter(); }

private:
    std::array<bool, kNumKeys> held {};
    std::array<juce::uint8, kNumKeys> sentLeds {};
    juce::ValueTree clip;
    int pendingLoad = -1; juce::uint32 pendingLoadMs = 0;

    static void setNorm (juce::RangedAudioParameter* p, float v)
    {
        if (p == nullptr) return;
        p->beginChangeGesture(); p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, v)); p->endChangeGesture();
    }
    void nudge (juce::RangedAudioParameter* p, int d, float acc)
    {
        if (p == nullptr || d == 0) return;
        const int n = p->getNumSteps();
        const bool discrete = n > 1 && n < 200 && (p->isDiscrete() || dynamic_cast<juce::AudioParameterBool*> (p) != nullptr
                              || dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr || dynamic_cast<juce::AudioParameterInt*> (p) != nullptr);
        if (discrete)
        {
            const int idx = juce::roundToInt (p->getValue() * (n - 1));
            setNorm (p, (float) juce::jlimit (0, n - 1, idx + (d > 0 ? 1 : -1) * (std::abs (d) >= 3 ? 2 : 1)) / (float) (n - 1));
            return;
        }
        setNorm (p, p->getValue() + d * (func ? 0.002f : 0.01f) * acc);
    }
    void saveKit()
    {
        auto dir = ManipoloProcessor::userFolder().getChildFile ("Kits");
        int i = 1; juce::File f;
        do f = dir.getChildFile ("Kit " + juce::String (i++).paddedLeft ('0', 3) + ".mkit"); while (f.exists());
        say (proc.saveKit (f) ? "Salvato " + f.getFileNameWithoutExtension() : juce::String ("Salvataggio non riuscito"));
        if (f.existsAsFile()) proc.currentKit = f;
    }
};
} // namespace hw
