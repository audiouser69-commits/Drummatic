#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace mp;
using namespace mpp;

ManipoloProcessor::ManipoloProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Out 3-4", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Out 5-6", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Out 7-8", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Out 9-10", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Out 11-12", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Out 13-14", juce::AudioChannelSet::stereo(), false)
                                       .withOutput ("Out 15-16", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "MANIPOLO", createLayout())
{
    formats.registerBasicFormats();
    for (auto& l : perfLock) l = false;
    cacheParams();
    startTimerHz (20);
}

ManipoloProcessor::~ManipoloProcessor() { stopTimer(); }

bool ManipoloProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
        if (! layouts.outputBuses[i].isDisabled() && layouts.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void ManipoloProcessor::cacheParams()
{
    for (int v = 0; v < kNumVoices; ++v)
    {
        auto& p = vp[v];
        auto r = [&] (const char* n) { auto* x = apvts.getRawParameterValue (vid (v, n)); jassert (x != nullptr); return x; };
        for (int t = 0; t < kNumTargets; ++t) p.target[t] = r (targetId (t));
        p.synthOn = r ("synthOn"); p.engine = r ("engine"); p.env2 = r ("env2"); p.stereo = r ("stereo");
        p.sampOn = r ("sampOn"); p.sampEng = r ("sampEng"); p.sampEnv = r ("sampEnv"); p.fmode = r ("fmode");
        p.post = r ("post"); p.dmode = r ("dmode"); p.e1curve = r ("e1curve"); p.e1vel = r ("e1vel");
        p.e2curve = r ("e2curve"); p.e2vel = r ("e2vel");
        p.l1sync = r ("l1sync"); p.l1bip = r ("l1bip"); p.l1wave = r ("l1wave"); p.l1reset = r ("l1reset"); p.l1shape = r ("l1shape");
        p.l2sync = r ("l2sync"); p.l2bip = r ("l2bip"); p.l2wave = r ("l2wave"); p.l2reset = r ("l2reset"); p.l2shape = r ("l2shape");
        p.macro1 = r ("macro1"); p.macro2 = r ("macro2"); p.variation = r ("variation"); p.choke = r ("choke"); p.link = r ("link");
        p.steps = r ("steps"); p.fill = r ("fill"); p.pattern = r ("pattern"); p.rotate = r ("rotate"); p.invert = r ("invert");
        p.sshift = r ("sshift"); p.swing = r ("swing"); p.shape = r ("shape"); p.density = r ("density");
        p.speed = r ("speed"); p.mult = r ("mult"); p.length = r ("length"); p.snap = r ("snap");
        p.resetOn = r ("resetOn"); p.resetLen = r ("resetLen");
        p.route = r ("route"); p.pfEnable = r ("pfEnable"); p.pfFlip = r ("pfFlip");
        p.lzOn = r ("lzOn"); p.lzEng = r ("lzEng"); p.lzEnv = r ("lzEnv"); p.lzKit = r ("lzKit");
        p.tail = r ("tail"); p.tailLvl = r ("tailLvl"); p.tailDec = r ("tailDec"); p.tailTone = r ("tailTone");
        p.tzOn = r ("tzOn"); p.tzTaps = r ("tzTaps"); p.tzSync = r ("tzSync"); p.tzDiv = r ("tzDiv"); p.tzTime = r ("tzTime");
        p.tzShape = r ("tzShape"); p.tzDecay = r ("tzDecay"); p.tzSpread = r ("tzSpread"); p.tzMix = r ("tzMix");
        p.rpOn = r ("rpOn"); p.rpSync = r ("rpSync"); p.rpDiv = r ("rpDiv"); p.rpTime = r ("rpTime"); p.rpCount = r ("rpCount");
        p.rpGaps = r ("rpGaps"); p.rpRev = r ("rpRev"); p.rpCrush = r ("rpCrush"); p.rpMix = r ("rpMix");
    }
    gp.dSync = g ("d_sync"); gp.dDiv = g ("d_div"); gp.dTime = g ("d_time"); gp.dFb = g ("d_fb"); gp.dGlitch = g ("d_glitch");
    gp.dFbTilt = g ("d_fbtilt"); gp.dDuck = g ("d_duck"); gp.dSRate = g ("d_srate"); gp.dGain = g ("d_gain");
    gp.dStereo = g ("d_stereo"); gp.dPong = g ("d_pong"); gp.dToRev = g ("d_torev");
    gp.rSync = g ("r_sync"); gp.rDiv = g ("r_div"); gp.rPre = g ("r_pre"); gp.rDecay = g ("r_decay"); gp.rSRate = g ("r_srate");
    gp.rInTilt = g ("r_intilt"); gp.rOutTilt = g ("r_outtilt"); gp.rGain = g ("r_gain"); gp.rMode = g ("r_mode");
    gp.mMax = g ("m_max"); gp.mLow = g ("m_low"); gp.mMid = g ("m_mid"); gp.mHigh = g ("m_high"); gp.mGain = g ("m_gain");
    gp.mClip = g ("m_clip"); gp.humanize = g ("humanize"); gp.bpm = g ("bpm");
    gp.pfDepth = g ("pf_depth"); gp.pfEnabled = g ("pf_enabled");
    gp.psOn = g ("ps_on"); gp.psLen = g ("ps_len"); gp.psSpeed = g ("ps_speed");
    for (int i = 0; i < kNumPerf; ++i) gp.perf[i] = g (perfId (i));
}

void ManipoloProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    dm.prepare ((float) sampleRate, samplesPerBlock);
}

void ManipoloProcessor::pullSettings (double bpm)
{
    auto b = [] (std::atomic<float>* x) { return x->load() > 0.5f; };
    auto i = [] (std::atomic<float>* x) { return juce::roundToInt (x->load()); };
    updatePerfSeq();
    for (int v = 0; v < kNumVoices; ++v)
    {
        auto& p = vp[v]; auto& s = dm.vs[v];
        for (int t = 0; t < kNumTargets; ++t) s.base[t] = norm (t, p.target[t]->load());
        s.synthOn = b (p.synthOn); s.engine = juce::jlimit (0, kNumSynthEngines - 1, i (p.engine));
        s.synthEnv2 = b (p.env2); s.stereo = b (p.stereo); s.sampOn = b (p.sampOn);
        s.sampEngine = i (p.sampEng); s.sampEnvSrc = i (p.sampEnv); s.filterMode = i (p.fmode); s.post = b (p.post);
        s.distMode = i (p.dmode); s.e1Curve = p.e1curve->load(); s.e1Vel = p.e1vel->load();
        s.e2Curve = p.e2curve->load(); s.e2Vel = p.e2vel->load();
        s.l1Sync = b (p.l1sync); s.l1Bip = b (p.l1bip); s.l1Wave = i (p.l1wave); s.l1Reset = i (p.l1reset); s.l1Shape = p.l1shape->load();
        s.l2Sync = b (p.l2sync); s.l2Bip = b (p.l2bip); s.l2Wave = i (p.l2wave); s.l2Reset = i (p.l2reset); s.l2Shape = p.l2shape->load();
        s.macro1 = p.macro1->load(); s.macro2 = p.macro2->load(); s.variation = p.variation->load();
        s.bpm = (float) bpm;
        dm.gs.choke[v] = i (p.choke); dm.gs.link[v] = i (p.link);

        auto& q = dm.ss[v];
        q.steps = i (p.steps); q.fill = i (p.fill); q.pattern = i (p.pattern); q.rotate = i (p.rotate); q.invert = b (p.invert);
        q.shift = p.sshift->load(); q.swing = p.swing->load(); q.shape = p.shape->load(); q.density = p.density->load();
        q.speedIdx = i (p.speed); q.multIdx = i (p.mult); q.length = i (p.length); q.snap = b (p.snap);
        q.resetOn = b (p.resetOn); q.resetLen = i (p.resetLen);
        s.route = i (p.route);
        // strato LAZER, coda, BRK
        s.lzOn = b (p.lzOn); s.lzEngine = juce::jlimit (0, kNumLzEngines - 1, i (p.lzEng)); s.lzEnvSrc = i (p.lzEnv); s.lzKit = b (p.lzKit);
        s.tail = i (p.tail); s.tailLevel = p.tailLvl->load(); s.tailDecay = p.tailDec->load(); s.tailTone = p.tailTone->load();
        s.tzOn = b (p.tzOn); s.tzTaps = i (p.tzTaps);
        s.tzFirst = b (p.tzSync) ? (float) (brkDivBeats (i (p.tzDiv)) * 60.0 / std::max (20.0, bpm)) : p.tzTime->load();
        s.tzShape = p.tzShape->load(); s.tzDecay = p.tzDecay->load(); s.tzSpread = p.tzSpread->load(); s.tzMix = p.tzMix->load();
        s.rpOn = b (p.rpOn); s.rpCount = i (p.rpCount);
        s.rpSlice = b (p.rpSync) ? (float) (brkDivBeats (i (p.rpDiv)) * 60.0 / std::max (20.0, bpm)) : p.rpTime->load();
        s.rpGaps = p.rpGaps->load(); s.rpRev = p.rpRev->load(); s.rpCrush = p.rpCrush->load(); s.rpMix = p.rpMix->load();
        s.modAmp = 1.0f; q.probOffset = 0; q.ratchOffset = 0;
        if (b (gp.pfEnabled) && b (p.pfEnable))
            applyPerformance (v, gp.pfDepth->load() * (b (p.pfFlip) ? -1.0f : 1.0f));
    }
    auto& G = dm.gs;
    G.dSync = b (gp.dSync); G.dDivIdx = i (gp.dDiv); G.dTimeMs = gp.dTime->load(); G.dFeedback = gp.dFb->load();
    G.dGlitch = gp.dGlitch->load(); G.dFbTilt = gp.dFbTilt->load(); G.dDucking = gp.dDuck->load(); G.dSRate = gp.dSRate->load();
    G.dGain = dbToGain (gp.dGain->load()); G.dStereo = b (gp.dStereo); G.dPong = b (gp.dPong); G.dToRev = gp.dToRev->load();
    G.rSync = b (gp.rSync); G.rDivIdx = i (gp.rDiv); G.rPreMs = gp.rPre->load(); G.rDecay = gp.rDecay->load();
    G.rSRate = gp.rSRate->load(); G.rInTilt = gp.rInTilt->load(); G.rOutTilt = gp.rOutTilt->load();
    G.rGain = dbToGain (gp.rGain->load()); G.rMode = i (gp.rMode);
    G.maximize = gp.mMax->load(); G.eqLow = gp.mLow->load(); G.eqMid = gp.mMid->load(); G.eqHigh = gp.mHigh->load();
    G.masterDb = gp.mGain->load(); G.clip = i (gp.mClip); G.humanize = gp.humanize->load();
}

// Somma i controlli della pagina Performance ai valori della voce (k = DEPTH × FLIP)
void ManipoloProcessor::applyPerformance (int v, float k)
{
    auto& s = dm.vs[v]; auto& q = dm.ss[v]; auto& sh = dm.vshared[v];
    auto pv = [&] (int i) { return perfValue (i) * k; };
    auto addT = [&] (int t, float d) { if (d != 0.0f && ! sh.locked[t].load()) s.base[t] = clamp01 (s.base[t] + d); };
    for (int i = 0; i < kNumPerf; ++i)
        if (const int t = perfTarget (i); t >= 0) addT (t, pv (i) * perfScale (i));
    addT (T_E1Attack, pv (PF_EnvA)); addT (T_E2Attack, pv (PF_EnvA));
    addT (T_E1Hold, pv (PF_EnvH));   addT (T_E2Hold, pv (PF_EnvH));
    addT (T_E1Decay, pv (PF_EnvD));  addT (T_E2Decay, pv (PF_EnvD));
    s.e1Curve = clamp01 (s.e1Curve + pv (PF_EnvC)); s.e2Curve = clamp01 (s.e2Curve + pv (PF_EnvC));
    s.macro1 = clamp01 (s.macro1 + pv (PF_Macro1)); s.macro2 = clamp01 (s.macro2 + pv (PF_Macro2));
    s.variation = clamp01 (s.variation + pv (PF_Var));
    s.modAmp = clampf (1.0f + pv (PF_ModAmp), 0.0f, 2.0f);
    q.steps = juce::jlimit (1, 64, q.steps + juce::roundToInt (pv (PF_Steps) * 32));
    q.fill = juce::jlimit (0, 64, q.fill + juce::roundToInt (pv (PF_Fill) * 32));
    q.pattern = juce::jlimit (1, 64, q.pattern + juce::roundToInt (pv (PF_Pattern) * 32));
    q.density = clampf (q.density + pv (PF_Density), -1, 1);
    q.probOffset = pv (PF_Prob);
    q.ratchOffset = juce::roundToInt (pv (PF_Ratchet) * 4);
    // glitch: un valore positivo su LEVEL / MIX accende lo strato anche se nella voce è spento
    if (const float d = pv (PF_LzLevel); d != 0.0f && ! sh.locked[T_LzLevel].load())
    {
        if (! s.lzOn) { if (d > 0) { s.lzOn = true; s.base[T_LzLevel] = clamp01 (0.6f + d * 0.4f); } }
        else s.base[T_LzLevel] = clamp01 (s.base[T_LzLevel] + d);
    }
    if (const float d = pv (PF_LzEngine); d != 0.0f)
        s.lzEngine = ((s.lzEngine + juce::roundToInt (d * 32.0f)) % kNumLzEngines + kNumLzEngines) % kNumLzEngines;
    if (const float d = pv (PF_TzMix); d != 0.0f)
    {
        if (! s.tzOn) { if (d > 0) { s.tzOn = true; s.tzMix = d; } }
        else s.tzMix = clamp01 (s.tzMix + d);
    }
    s.tzShape = clampf (s.tzShape + pv (PF_TzShape), -1.0f, 1.0f);
    if (const float d = pv (PF_RpMix); d != 0.0f)
    {
        if (! s.rpOn) { if (d > 0) { s.rpOn = true; s.rpMix = d; } }
        else s.rpMix = clamp01 (s.rpMix + d);
    }
    s.rpGaps = clamp01 (s.rpGaps + pv (PF_RpGaps)); s.rpRev = clamp01 (s.rpRev + pv (PF_RpRev)); s.rpCrush = clamp01 (s.rpCrush + pv (PF_RpCrush));
}

// valore effettivo di un controllo Performance: il blocco del passo del sequencer, se c'è, altrimenti la manopola
float ManipoloProcessor::perfValue (int i) const
{
    const float o = perfSeq.out[i].load (std::memory_order_relaxed);
    return std::isnan (o) ? gp.perf[i]->load() : o;
}

// Sequencer della Performance (thread audio, una volta per blocco)
void ManipoloProcessor::updatePerfSeq()
{
    const float nan = std::numeric_limits<float>::quiet_NaN();
    if (gp.psOn->load() < 0.5f || ! dm.running.load())
    {
        if (psCurStep != -1 || perfSeq.playStep.load() != -1) { for (auto& o : perfSeq.out) o = nan; perfSeq.playStep = -1; }
        psCurStep = psPrevStep = psLastRaw = -1;
        return;
    }
    const int len = juce::jlimit (1, kPerfSeqSteps, juce::roundToInt (gp.psLen->load()));
    const double stepBeats = speedBeats (juce::jlimit (0, kNumSpeeds - 1, juce::roundToInt (gp.psSpeed->load())));
    const double pos = std::max (0.0, dm.displayBeat.load()) / stepBeats;
    const int raw = (int) std::floor (pos);
    const int step = raw % len;
    psFrac = (float) (pos - std::floor (pos));
    if (raw != psLastRaw)
    {
        psLastRaw = raw;
        if (psCurStep < 0) { psCurStep = psPrevStep = step; }
        else if (psRng.uni() < perfSeq.prob[step].load()) { psPrevStep = psCurStep; psCurStep = step; }
        perfSeq.playStep = step;
    }
    const float g = perfSeq.glide[psCurStep].load();
    for (int i = 0; i < kNumPerf; ++i)
    {
        const float c = perfSeq.lock[psCurStep][i].load(), pr = perfSeq.lock[psPrevStep][i].load();
        if (std::isnan (c) && std::isnan (pr)) { perfSeq.out[i].store (nan, std::memory_order_relaxed); continue; }
        const float knob = gp.perf[i]->load();
        const float vc = std::isnan (c) ? knob : c, vp = std::isnan (pr) ? knob : pr;
        const float val = (g > 0.001f && psFrac < g && psPrevStep != psCurStep) ? lerp (vp, vc, psFrac / g) : vc;
        perfSeq.out[i].store (val, std::memory_order_relaxed);
    }
}

void ManipoloProcessor::perfSeqCapture (int step)
{
    if (step < 0 || step >= kPerfSeqSteps) return;
    for (int i = 0; i < kNumPerf; ++i)
    {
        const float kv = apvts.getRawParameterValue (perfId (i))->load();
        if (std::abs (kv) > 0.0005f) perfSeq.lock[step][i] = kv;
    }
}

void ManipoloProcessor::perfSeqRandomGlitch()
{
    static const int glitch[] = { PF_LzLevel, PF_LzEngine, PF_LzTimbre, PF_LzColor, PF_LzDensity, PF_LzDecay,
                                  PF_TzMix, PF_TzShape, PF_RpMix, PF_RpGaps, PF_RpRev, PF_RpCrush,
                                  PF_Distort, PF_Cutoff, PF_Ratchet, PF_Macro1 };
    juce::Array<int> pool;
    for (int i : glitch) if (! perfLock[i].load()) pool.add (i);
    if (pool.isEmpty()) return;
    const int len = juce::roundToInt (apvts.getRawParameterValue ("ps_len")->load());
    for (int st = 0; st < kPerfSeqSteps; ++st)
    {
        perfSeq.clearStep (st);
        if (st >= len || random.nextFloat() > 0.55f) continue;
        const int n = 1 + random.nextInt (4);
        for (int k = 0; k < n; ++k)
        {
            const int i = pool[random.nextInt (pool.size())];
            const bool positive = i == PF_LzLevel || i == PF_TzMix || i == PF_RpMix;
            perfSeq.lock[st][i] = positive ? 0.3f + 0.7f * random.nextFloat() : random.nextFloat() * 2.0f - 1.0f;
        }
        perfSeq.glide[st] = random.nextFloat() < 0.3f ? 0.3f + 0.6f * random.nextFloat() : 0.0f;
        perfSeq.prob[st] = random.nextFloat() < 0.7f ? 1.0f : 0.5f + 0.4f * random.nextFloat();
    }
}

// rende un colpo della voce in un WAV (dry, senza mandate e master): per il drag & drop nel DAW
juce::File ManipoloProcessor::renderVoiceToFile (int voice)
{
    const float srate = getSampleRate() > 0 ? (float) getSampleRate() : 48000.0f;
    auto vsh = std::make_unique<VoiceShared>();
    for (int s = 0; s < kNumSources; ++s) for (int t = 0; t < kNumTargets; ++t) vsh->depth[s][t] = dm.vshared[voice].depth[s][t].load();
    for (int t = 0; t < kNumTargets; ++t) vsh->locked[t] = dm.vshared[voice].locked[t].load();
    auto smp = getSample (voice);
    auto v = std::make_unique<Voice>();
    v->shared = vsh.get(); v->sample = smp.get(); v->prepare (srate);
    VoiceSettings s = dm.vs[voice]; s.route = 0;
    TriggerInfo ti; ti.velocity = 1.0f;
    v->trigger (s, ti);
    const int maxN = (int) (srate * 12.0f), blk = 512;
    juce::AudioBuffer<float> out (2, maxN); out.clear();
    std::vector<float> dl (blk), dr (blk), rl (blk), rr (blk);
    int n = 0;
    while (n < maxN)
    {
        const int len = std::min (blk, maxN - n);
        v->render (s, out.getWritePointer (0) + n, out.getWritePointer (1) + n, dl.data(), dr.data(), rl.data(), rr.data(), len);
        n += len;
        if (! v->isActive()) break;
    }
    int last = 0;
    for (int i = 0; i < n; ++i) if (std::abs (out.getSample (0, i)) > 1.0e-4f || std::abs (out.getSample (1, i)) > 1.0e-4f) last = i;
    const int total = std::max (64, std::min (n, last + (int) (srate * 0.01f)));
    const int fade = std::min (total / 4, (int) (srate * 0.005f));
    for (int c = 0; c < 2; ++c) out.applyGainRamp (c, total - fade, fade, 1.0f, 0.0f);
    for (int c = 0; c < 2; ++c) for (int i = 0; i < total; ++i) out.setSample (c, i, softClip (out.getSample (c, i)));

    auto folder = userFolder().getChildFile ("Renders"); folder.createDirectory();
    juce::String name = s.lzOn ? juce::String (lzEngineName (s.lzEngine)) : juce::String (engineInfo (s.engine).name);
    name = juce::File::createLegalFileName ("Drummatic V" + juce::String (voice + 1) + " " + name + " " + juce::Time::getCurrentTime().formatted ("%H%M%S"));
    auto f = folder.getNonexistentChildFile (name, ".wav");
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> os (f.createOutputStream());
    if (os == nullptr) return {};
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), srate, 2, 24, {}, 0));
    if (w == nullptr) return {};
    os.release();
    w->writeFromAudioSampleBuffer (out, 0, total);
    return f;
}

void ManipoloProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();

    Transport tp; tp.bpm = gp.bpm->load();
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm()) tp.bpm = *bpm;
            if (auto ppq = pos->getPpqPosition()) { tp.ppq = *ppq; tp.hostHasPosition = true; }
            tp.hostPlaying = pos->getIsPlaying();
        }
    }
    hostDrivesTransport = tp.hostHasPosition; hostPlaying = tp.hostPlaying;
    pullSettings (tp.bpm);

    {
        const juce::SpinLock::ScopedTryLockType lock (sampleLock);
        if (lock.isLocked())
        {
            for (int v = 0; v < kNumVoices; ++v) dm.samples[v] = sampleData[v].get();
            ++blockCount;
        }
    }

    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) dm.noteOn (msg.getChannel(), msg.getNoteNumber(), msg.getFloatVelocity(), m.samplePosition);
        else if (msg.isNoteOff()) dm.noteOff (msg.getChannel(), msg.getNoteNumber());
    }
    midi.clear();

    auto mainBuf = getBusBuffer (buffer, false, 0);
    float* L = mainBuf.getWritePointer (0);
    float* R = mainBuf.getNumChannels() > 1 ? mainBuf.getWritePointer (1) : L;
    float* auxL[7] {}; float* auxR[7] {};
    for (int k = 1; k < getBusCount (false) && k <= 7; ++k)
        if (auto* bus = getBus (false, k); bus != nullptr && bus->isEnabled() && bus->getNumberOfChannels() == 2)
        {
            auto bb = getBusBuffer (buffer, false, k);
            auxL[k - 1] = bb.getWritePointer (0); auxR[k - 1] = bb.getWritePointer (1);
        }
    // a blocchi interni per tenere il controllo fine sul tempo
    int pos = 0;
    while (pos < n)
    {
        const int len = std::min (std::min (256, dm.maxBlock()), n - pos);
        Transport t2 = tp; t2.ppq = tp.ppq + (double) pos / getSampleRate() * tp.bpm / 60.0;
        float* aL[7]; float* aR[7];
        for (int k = 0; k < 7; ++k) { aL[k] = auxL[k] ? auxL[k] + pos : nullptr; aR[k] = auxR[k] ? auxR[k] + pos : nullptr; }
        dm.process (L + pos, R + pos, len, t2, aL, aR);
        pos += len;
    }
}

// ---------------- parametri e azioni ----------------
void ManipoloProcessor::setParam (int voice, const juce::String& name, float realValue)
{
    if (auto* p = param (voice, name))
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
}

void ManipoloProcessor::timerCallback()
{
    int a;
    while (dm.actions.pop (a))
    {
        if (a == A_PrevPreset) stepKit (-1);
        else if (a == A_NextPreset) stepKit (1);
        else if (a == A_RandomPreset)
        {
            auto files = userFolder().getChildFile ("Kits").findChildFiles (juce::File::findFiles, false, "*.mkit");
            if (! files.isEmpty()) loadKit (files[random.nextInt (files.size())]);
        }
        else if (a == A_RandomizeAll) randomizeAll();
        else if (a == A_PerfRandom) perfRandomize();
        else if (a == A_PerfReset) perfReset();
        else if (a == A_PerfCommit) perfCommit();
        else if (a >= A_RandomizeVoice0 && a < A_RandomizeVoice0 + kNumVoices) randomizeVoice (a - A_RandomizeVoice0);
    }
    // REC del sequencer Performance: i movimenti delle manopole diventano blocchi del passo che sta suonando
    {
        const int st = perfSeq.playStep.load();
        for (int i = 0; i < kNumPerf; ++i)
        {
            const float kv = gp.perf[i]->load();
            if (perfSeq.rec.load() && st >= 0 && std::abs (kv - psLastKnob[i]) > 1.0e-4f) perfSeq.lock[st][i] = kv;
            psLastKnob[i] = kv;
        }
    }
    // libera i campioni sostituiti (il thread audio non li usa più dopo un blocco)
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    const auto now = blockCount.load();
    graveyard.erase (std::remove_if (graveyard.begin(), graveyard.end(), [now] (const auto& g) { return now > g.second + 2; }), graveyard.end());
}

static bool isVoiceParamLocked (ManipoloProcessor& p, int voice, const juce::String& name)
{
    for (int t = 0; t < kNumTargets; ++t)
        if (name == targetId (t)) return p.dm.vshared[voice].locked[t].load();
    return false;
}

void ManipoloProcessor::randomizeVoice (int voice)
{
    const float depth = apvts.getRawParameterValue ("rnd_depth")->load();
    static const juce::StringArray skip { "synthOn", "sampOn", "gain", "dsend", "rsend", "choke", "link", "steps", "fill", "pattern",
                                          "rotate", "invert", "sshift", "swing", "shape", "density", "speed", "mult", "length", "snap",
                                          "resetOn", "resetLen", "start", "end", "pan", "route", "pfEnable", "pfFlip",
                                          "lzOn", "lzKit", "tzOn", "rpOn", "tzSync", "rpSync" };
    const juce::String prefix = "v" + juce::String (voice + 1) + "_";
    for (auto* p : getParameters())
    {
        auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p);
        if (rp == nullptr || ! rp->paramID.startsWith (prefix)) continue;
        const auto name = rp->paramID.fromFirstOccurrenceOf ("_", false, false);
        if (skip.contains (name) || isVoiceParamLocked (*this, voice, name)) continue;
        const float cur = rp->getValue();
        float nv;
        if (dynamic_cast<juce::AudioParameterChoice*> (rp) != nullptr || dynamic_cast<juce::AudioParameterBool*> (rp) != nullptr)
            nv = random.nextFloat() < depth ? random.nextFloat() : cur;
        else nv = juce::jlimit (0.0f, 1.0f, cur + (random.nextFloat() * 2.0f - 1.0f) * depth);
        if (name == "cutoff") nv = std::max (nv, 0.45f);
        if (name == "e1d" || name == "e2d") nv = juce::jlimit (0.2f, 0.75f, nv);
        if (name == "e1a" || name == "e2a") nv = std::min (nv, 0.35f);          // niente attacchi lunghissimi
        if (name == "blend" && getSampleFile (voice) == juce::File()) nv = std::min (nv, 0.5f);   // senza campione resta il synth
        rp->setValueNotifyingHost (nv);
    }
}

void ManipoloProcessor::resetVoice (int voice)
{
    const juce::String prefix = "v" + juce::String (voice + 1) + "_";
    static const juce::StringArray keep { "steps", "fill", "pattern", "rotate", "invert", "sshift", "swing", "shape", "density",
                                          "speed", "mult", "length", "snap", "resetOn", "resetLen", "route", "pfEnable", "pfFlip" };
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID.startsWith (prefix) && ! keep.contains (rp->paramID.fromFirstOccurrenceOf ("_", false, false)))
                rp->setValueNotifyingHost (rp->getDefaultValue());
    auto& sh = dm.vshared[voice];
    for (auto& s : sh.depth) for (auto& d : s) d = 0.0f;
    for (auto& l : sh.locked) l = false;
}

// ---------------- Performance ----------------
void ManipoloProcessor::perfRandomize()
{
    const float depth = apvts.getRawParameterValue ("pf_rnd")->load();
    for (int i = 0; i < kNumPerf; ++i)
        if (! perfLock[i].load())
            if (auto* p = apvts.getParameter (perfId (i)))
                p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, 0.5f + (random.nextFloat() * 2.0f - 1.0f) * depth * 0.5f * 2.0f));
}

void ManipoloProcessor::perfReset()
{
    for (int i = 0; i < kNumPerf; ++i)
        if (auto* p = apvts.getParameter (perfId (i))) p->setValueNotifyingHost (p->getDefaultValue());
}

// COMMIT: scrive nei controlli delle voci il suono attuale e azzera la Performance
void ManipoloProcessor::perfCommit()
{
    if (apvts.getRawParameterValue ("pf_enabled")->load() < 0.5f) { perfReset(); return; }
    const float depth = apvts.getRawParameterValue ("pf_depth")->load();
    for (int v = 0; v < kNumVoices; ++v)
    {
        if (param (v, "pfEnable")->getValue() < 0.5f) continue;
        const float k = depth * (param (v, "pfFlip")->getValue() > 0.5f ? -1.0f : 1.0f);
        auto pv = [&] (int i) { return apvts.getRawParameterValue (perfId (i))->load() * k; };
        auto addN = [&] (const juce::String& name, float d, int t)
        {
            if (d == 0.0f || (t >= 0 && dm.vshared[v].locked[t].load())) return;
            auto* p = param (v, name);
            p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, p->getValue() + d));
        };
        auto addInt = [&] (const juce::String& name, int d)
        {
            if (d == 0) return;
            auto* p = param (v, name);
            p->setValueNotifyingHost (p->convertTo0to1 (p->convertFrom0to1 (p->getValue()) + (float) d));
        };
        for (int i = 0; i < kNumPerf; ++i)
            if (const int t = perfTarget (i); t >= 0) addN (targetId (t), pv (i) * perfScale (i), t);
        addN ("e1a", pv (PF_EnvA), T_E1Attack); addN ("e2a", pv (PF_EnvA), T_E2Attack);
        addN ("e1h", pv (PF_EnvH), T_E1Hold);   addN ("e2h", pv (PF_EnvH), T_E2Hold);
        addN ("e1d", pv (PF_EnvD), T_E1Decay);  addN ("e2d", pv (PF_EnvD), T_E2Decay);
        addN ("e1curve", pv (PF_EnvC), -1); addN ("e2curve", pv (PF_EnvC), -1);
        addN ("macro1", pv (PF_Macro1), -1); addN ("macro2", pv (PF_Macro2), -1); addN ("variation", pv (PF_Var), -1);
        addN ("density", pv (PF_Density) * 0.5f, -1);
        addInt ("steps", juce::roundToInt (pv (PF_Steps) * 32)); addInt ("fill", juce::roundToInt (pv (PF_Fill) * 32));
        addInt ("pattern", juce::roundToInt (pv (PF_Pattern) * 32));
        // glitch
        addN ("lzlevel", pv (PF_LzLevel), T_LzLevel);
        addInt ("lzEng", juce::roundToInt (pv (PF_LzEngine) * 32));
        addN ("tzMix", pv (PF_TzMix), -1); addN ("tzShape", pv (PF_TzShape) * 0.5f, -1);
        addN ("rpMix", pv (PF_RpMix), -1); addN ("rpGaps", pv (PF_RpGaps), -1); addN ("rpRev", pv (PF_RpRev), -1); addN ("rpCrush", pv (PF_RpCrush), -1);
        // MOD AMP: scala le profondità di modulazione
        const float amp = juce::jlimit (0.0f, 2.0f, 1.0f + pv (PF_ModAmp));
        if (amp != 1.0f)
            for (auto& src : dm.vshared[v].depth) for (auto& d : src) d = juce::jlimit (-1.0f, 1.0f, d.load() * amp);
        // PROB e RATCHET: applicati alle lane
        auto& sq = dm.sshared[v];
        const float po = pv (PF_Prob) * 100.0f; const int ro = juce::roundToInt (pv (PF_Ratchet) * 4);
        for (int st = 0; st < kMaxSteps; ++st)
        {
            if (po != 0) sq.lane[L_Prob][st] = juce::jlimit (-100.0f, 100.0f, sq.lane[L_Prob][st].load() + po);
            if (ro != 0) sq.lane[L_Ratch][st] = (float) juce::jlimit (1, 8, juce::roundToInt (sq.lane[L_Ratch][st].load()) + ro);
        }
    }
    perfReset();
}

// ---------------- campioni ----------------
bool ManipoloProcessor::loadSample (int voice, const juce::File& f)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (f));
    if (reader == nullptr) return false;
    const int len = (int) std::min<juce::int64> (reader->lengthInSamples, (juce::int64) reader->sampleRate * 60);
    juce::AudioBuffer<float> buf ((int) reader->numChannels, len);
    reader->read (&buf, 0, len, 0, true, true);
    auto sd = std::make_shared<SampleData>();
    sd->sourceRate = (float) reader->sampleRate;
    sd->l.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (buf.getNumChannels() > 1) sd->r.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
    {
        const juce::SpinLock::ScopedLockType lock (sampleLock);
        if (sampleData[voice]) graveyard.push_back ({ sampleData[voice], blockCount.load() });
        sampleData[voice] = sd;
    }
    sampleFiles[(size_t) voice] = f;
    if (auto* p = param (voice, "sampOn")) if (p->getValue() < 0.5f) p->setValueNotifyingHost (1.0f);
    return true;
}

void ManipoloProcessor::clearSample (int voice)
{
    const juce::SpinLock::ScopedLockType lock (sampleLock);
    if (sampleData[voice]) graveyard.push_back ({ sampleData[voice], blockCount.load() });
    sampleData[voice].reset();
    sampleFiles[(size_t) voice] = juce::File();
}

std::shared_ptr<const SampleData> ManipoloProcessor::getSample (int voice) const
{
    const juce::SpinLock::ScopedLockType lock (const_cast<juce::SpinLock&> (sampleLock));
    return sampleData[voice];
}

// ---------------- stato ----------------
static juce::String floatsToString (const std::atomic<float>* a, int n)
{
    juce::StringArray s; for (int i = 0; i < n; ++i) s.add (juce::String (a[i].load(), 4)); return s.joinIntoString (",");
}

juce::ValueTree ManipoloProcessor::extraToTree()
{
    juce::ValueTree extra ("EXTRA");
    for (int v = 0; v < kNumVoices; ++v) extra.addChild (voiceToTree (v).getChildWithName ("DATA").createCopy(), -1, nullptr);
    juce::StringArray ms, ss;
    for (int v = 0; v < kNumVoices; ++v) { ms.add (dm.mute[v].load() ? "1" : "0"); ss.add (dm.solo[v].load() ? "1" : "0"); }
    extra.setProperty ("mute", ms.joinIntoString (","), nullptr);
    extra.setProperty ("solo", ss.joinIntoString (","), nullptr);
    // sequencer della Performance
    juce::ValueTree ps ("PERFSEQ");
    for (int st = 0; st < kPerfSeqSteps; ++st)
    {
        juce::StringArray items;
        for (int i = 0; i < kNumPerf; ++i)
            if (perfSeq.hasLock (st, i)) items.add (juce::String (perfId (i)) + "=" + juce::String (perfSeq.lock[st][i].load(), 4));
        if (items.isEmpty() && perfSeq.glide[st].load() == 0.0f && perfSeq.prob[st].load() == 1.0f) continue;
        juce::ValueTree s ("STEP");
        s.setProperty ("i", st, nullptr); s.setProperty ("glide", perfSeq.glide[st].load(), nullptr);
        s.setProperty ("prob", perfSeq.prob[st].load(), nullptr); s.setProperty ("locks", items.joinIntoString (";"), nullptr);
        ps.addChild (s, -1, nullptr);
    }
    extra.addChild (ps, -1, nullptr);
    return extra;
}

void ManipoloProcessor::extraFromTree (const juce::ValueTree& extra)
{
    int v = 0;
    for (auto c : extra)
        if (c.hasType ("DATA") && v < kNumVoices)
        {
            juce::ValueTree wrapper ("VOICE"); wrapper.addChild (c.createCopy(), -1, nullptr);
            voiceFromTree (v++, wrapper);
        }
    auto ms = juce::StringArray::fromTokens (extra.getProperty ("mute").toString(), ",", "");
    auto ss = juce::StringArray::fromTokens (extra.getProperty ("solo").toString(), ",", "");
    for (int i = 0; i < kNumVoices; ++i)
    {
        dm.mute[i] = ms[i] == "1"; dm.solo[i] = ss[i] == "1";
    }
}

// Albero con TUTTI i dati di una voce: parametri, modulazioni, lucchetti, campione, sequenza
juce::ValueTree ManipoloProcessor::voiceToTree (int voice)
{
    juce::ValueTree t ("VOICE");
    juce::ValueTree params ("PARAMS");
    const juce::String prefix = "v" + juce::String (voice + 1) + "_";
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (rp->paramID.startsWith (prefix))
                params.setProperty (rp->paramID.fromFirstOccurrenceOf ("_", false, false), rp->getValue(), nullptr);
    t.addChild (params, -1, nullptr);

    juce::ValueTree data ("DATA");
    auto& sh = dm.vshared[voice];
    juce::StringArray mod;
    for (int s = 0; s < kNumSources; ++s)
        for (int tg = 0; tg < kNumTargets; ++tg)
            if (sh.depth[s][tg].load() != 0.0f) mod.add (juce::String (s) + ":" + targetId (tg) + ":" + juce::String (sh.depth[s][tg].load(), 4));
    data.setProperty ("mod", mod.joinIntoString (";"), nullptr);
    juce::StringArray locks;
    for (int tg = 0; tg < kNumTargets; ++tg) if (sh.locked[tg].load()) locks.add (targetId (tg));
    data.setProperty ("locks", locks.joinIntoString (","), nullptr);
    data.setProperty ("sample", sampleFiles[(size_t) voice].getFullPathName(), nullptr);
    auto& sq = dm.sshared[voice];
    juce::String man; for (int i = 0; i < kMaxSteps; ++i) man += sq.manual[i].load() ? "1" : "0";
    data.setProperty ("manual", man, nullptr);
    for (int l = 0; l < kNumLanes; ++l)
    {
        data.setProperty ("lane" + juce::String (l), floatsToString (sq.lane[l], kMaxSteps), nullptr);
        data.setProperty ("laneLen" + juce::String (l), sq.laneLen[l].load(), nullptr);
        data.setProperty ("laneSync" + juce::String (l), sq.laneSync[l].load(), nullptr);
    }
    t.addChild (data, -1, nullptr);
    return t;
}

void ManipoloProcessor::voiceFromTree (int voice, const juce::ValueTree& t)
{
    const auto params = t.getChildWithName ("PARAMS");
    if (params.isValid())
        for (int i = 0; i < params.getNumProperties(); ++i)
        {
            const auto name = params.getPropertyName (i).toString();
            if (auto* p = param (voice, name))
                if (! isVoiceParamLocked (*this, voice, name))
                    p->setValueNotifyingHost ((float) params.getProperty (name));
        }
    const auto data = t.getChildWithName ("DATA");
    if (! data.isValid()) return;
    auto& sh = dm.vshared[voice];
    for (auto& s : sh.depth) for (auto& d : s) d = 0.0f;
    for (auto& tok : juce::StringArray::fromTokens (data.getProperty ("mod").toString(), ";", ""))
    {
        auto parts = juce::StringArray::fromTokens (tok, ":", "");
        if (parts.size() != 3) continue;
        for (int tg = 0; tg < kNumTargets; ++tg)
            if (parts[1] == targetId (tg)) sh.depth[juce::jlimit (0, kNumSources - 1, parts[0].getIntValue())][tg] = parts[2].getFloatValue();
    }
    auto locks = juce::StringArray::fromTokens (data.getProperty ("locks").toString(), ",", "");
    for (int tg = 0; tg < kNumTargets; ++tg) sh.locked[tg] = locks.contains (targetId (tg));

    const juce::File f (data.getProperty ("sample").toString());
    if (data.getProperty ("sample").toString().isNotEmpty() && f.existsAsFile()) loadSample (voice, f);
    else if (data.getProperty ("sample").toString().isEmpty()) clearSample (voice);

    auto& sq = dm.sshared[voice];
    const auto man = data.getProperty ("manual").toString();
    for (int i = 0; i < kMaxSteps; ++i) sq.manual[i] = i < man.length() && man[i] == '1';
    for (int l = 0; l < kNumLanes; ++l)
    {
        auto vals = juce::StringArray::fromTokens (data.getProperty ("lane" + juce::String (l)).toString(), ",", "");
        for (int i = 0; i < kMaxSteps; ++i) sq.lane[l][i] = i < vals.size() ? vals[i].getFloatValue() : laneInfo (l).def;
        sq.laneLen[l] = (int) data.getProperty ("laneLen" + juce::String (l), 16);
        sq.laneSync[l] = (int) data.getProperty ("laneSync" + juce::String (l), 0);
    }
}

void ManipoloProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    auto old = state.getChildWithName ("EXTRA");
    if (old.isValid()) state.removeChild (old, nullptr);
    state.addChild (extraToTree(), -1, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, dest);
}

void ManipoloProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
    {
        auto state = juce::ValueTree::fromXml (*xml);
        if (! state.hasType (apvts.state.getType())) return;
        auto extra = state.getChildWithName ("EXTRA");
        juce::StringArray present;
        for (auto c : state) if (c.hasType ("PARAM")) present.add (c.getProperty ("id").toString());
        apvts.replaceState (state);
        if (extra.isValid())
        {
            // i parametri vengono già da replaceState; qui solo i dati extra
            int v = 0;
            for (auto c : extra)
                if (c.hasType ("DATA") && v < kNumVoices)
                {
                    juce::ValueTree w ("VOICE"); w.addChild (c.createCopy(), -1, nullptr);
                    voiceFromTree (v++, w);
                }
            auto ms = juce::StringArray::fromTokens (extra.getProperty ("mute").toString(), ",", "");
            auto ss = juce::StringArray::fromTokens (extra.getProperty ("solo").toString(), ",", "");
            for (int i = 0; i < kNumVoices; ++i) { dm.mute[i] = ms[i] == "1"; dm.solo[i] = ss[i] == "1"; }
        }
        // sequencer della Performance
        perfSeq.clearAll();
        if (extra.isValid())
            for (auto s : extra.getChildWithName ("PERFSEQ"))
            {
                const int st = (int) s.getProperty ("i", -1);
                if (st < 0 || st >= kPerfSeqSteps) continue;
                perfSeq.glide[st] = (float) s.getProperty ("glide", 0.0f); perfSeq.prob[st] = (float) s.getProperty ("prob", 1.0f);
                for (auto& tok : juce::StringArray::fromTokens (s.getProperty ("locks").toString(), ";", ""))
                    for (int i = 0; i < kNumPerf; ++i)
                        if (tok.upToFirstOccurrenceOf ("=", false, false) == perfId (i)) perfSeq.lock[st][i] = tok.fromFirstOccurrenceOf ("=", false, false).getFloatValue();
            }
        // kit di versioni precedenti: i parametri che non contengono (strato LAZER, BRK…) tornano ai valori predefiniti
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! present.contains (rp->paramID)) rp->setValueNotifyingHost (rp->getDefaultValue());
    }
}

// ---------------- preset ----------------
// preferenze dell'utente (comuni a standalone e plugin): AppData/Roaming/Drummatic, ~/Library/Application Support/Drummatic, ~/.config/Drummatic
static std::unique_ptr<juce::PropertiesFile> openPrefs()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Drummatic"; o.filenameSuffix = ".prefs"; o.folderName = "Drummatic";
    o.osxLibrarySubFolder = "Application Support";
    return std::make_unique<juce::PropertiesFile> (o);
}
static juce::CriticalSection folderLock;
static juce::File cachedFolder; static bool folderLoaded = false;

juce::File ManipoloProcessor::defaultUserFolder()
{
    const auto docs = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);
    auto f = docs.getChildFile ("Drummatic");
    // migrazione: le versioni 0.1–0.2.1 salvavano in Documenti/Manipolo
    if (! f.exists() && docs.getChildFile ("Manipolo").isDirectory())
        docs.getChildFile ("Manipolo").copyDirectoryTo (f);
    return f;
}

static void makeSubfolders (const juce::File& f)
{
    for (auto n : { "Kits", "Voices", "Samples", "Renders" }) f.getChildFile (n).createDirectory();
}

juce::File ManipoloProcessor::userFolder()
{
    juce::File f;
    {
        const juce::ScopedLock sl (folderLock);
        if (! folderLoaded)
        {
            folderLoaded = true;
            const auto path = openPrefs()->getValue ("userFolder");
            cachedFolder = juce::File::isAbsolutePath (path) ? juce::File (path) : juce::File();
        }
        f = cachedFolder;
    }
    // cartella scelta non raggiungibile (disco esterno scollegato, permessi): si usa quella predefinita
    if (f != juce::File() && ! (f.isDirectory() || f.createDirectory().wasOk()))
        f = juce::File();
    if (f == juce::File()) f = defaultUserFolder();
    makeSubfolders (f);
    return f;
}

bool ManipoloProcessor::userFolderIsCustom()
{
    const juce::ScopedLock sl (folderLock);
    if (! folderLoaded) { folderLoaded = true; const auto p = openPrefs()->getValue ("userFolder"); cachedFolder = juce::File::isAbsolutePath (p) ? juce::File (p) : juce::File(); }
    return cachedFolder != juce::File();
}

bool ManipoloProcessor::setUserFolder (const juce::File& folder, bool copyExisting)
{
    const auto oldF = userFolder();
    const auto newF = folder == juce::File() ? defaultUserFolder() : folder;
    if (! (newF.isDirectory() || newF.createDirectory().wasOk())) return false;
    // prova di scrittura
    auto probe = newF.getNonexistentChildFile (".drummatic_prova", ".tmp");
    if (! probe.replaceWithText ("ok")) return false;
    probe.deleteFile();
    makeSubfolders (newF);
    if (copyExisting && newF != oldF && ! newF.isAChildOf (oldF))
    {
        for (auto n : { "Kits", "Voices", "Samples", "Renders" })
        {
            const auto src = oldF.getChildFile (n);
            for (auto& file : src.findChildFiles (juce::File::findFiles, true))
            {
                const auto dst = newF.getChildFile (n).getChildFile (file.getRelativePathFrom (src));
                if (dst.exists()) continue;
                dst.getParentDirectory().createDirectory();
                if (file.hasFileExtension ("mkit;mvoice"))
                {
                    // i campioni copiati: il kit punta alla nuova posizione
                    auto txt = file.loadFileAsString();
                    txt = txt.replace (oldF.getChildFile ("Samples").getFullPathName(), newF.getChildFile ("Samples").getFullPathName());
                    dst.replaceWithText (txt);
                }
                else file.copyFileTo (dst);
            }
        }
    }
    {
        const juce::ScopedLock sl (folderLock);
        folderLoaded = true;
        cachedFolder = folder == juce::File() ? juce::File() : newF;
    }
    auto prefs = openPrefs();
    if (folder == juce::File()) prefs->removeValue ("userFolder"); else prefs->setValue ("userFolder", newF.getFullPathName());
    prefs->saveIfNeeded();
    return true;
}

bool ManipoloProcessor::saveVoicePreset (int voice, const juce::File& f)
{
    auto t = voiceToTree (voice);
    // la sequenza non fa parte del preset di voce
    auto params = t.getChildWithName ("PARAMS");
    for (auto n : { "steps", "fill", "pattern", "rotate", "invert", "sshift", "swing", "shape", "density", "speed", "mult", "length", "snap", "resetOn", "resetLen", "choke", "link", "route", "pfEnable", "pfFlip" })
        params.removeProperty (n, nullptr);
    auto data = t.getChildWithName ("DATA");
    data.removeProperty ("manual", nullptr);
    for (int l = 0; l < kNumLanes; ++l) { data.removeProperty ("lane" + juce::String (l), nullptr); data.removeProperty ("laneLen" + juce::String (l), nullptr); data.removeProperty ("laneSync" + juce::String (l), nullptr); }
    if (auto xml = t.createXml()) return xml->writeTo (f);
    return false;
}

bool ManipoloProcessor::loadVoicePreset (int voice, const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    auto t = juce::ValueTree::fromXml (*xml);
    if (! t.hasType ("VOICE")) return false;
    // conserva la sequenza corrente
    auto cur = voiceToTree (voice).getChildWithName ("DATA");
    auto data = t.getChildWithName ("DATA");
    if (data.isValid())
    {
        data.setProperty ("manual", cur.getProperty ("manual"), nullptr);
        for (int l = 0; l < kNumLanes; ++l)
            for (auto k : { "lane", "laneLen", "laneSync" })
                data.setProperty (k + juce::String (l), cur.getProperty (k + juce::String (l)), nullptr);
    }
    voiceFromTree (voice, t);
    // preset di versioni precedenti: strato LAZER e BRK ai valori predefiniti
    if (auto params = t.getChildWithName ("PARAMS"); params.isValid())
    {
        const juce::String prefix = "v" + juce::String (voice + 1) + "_";
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (rp->paramID.startsWith (prefix))
                {
                    const auto name = rp->paramID.fromFirstOccurrenceOf ("_", false, false);
                    const bool isNew = name.startsWith ("lz") || name.startsWith ("tz") || name.startsWith ("rp") || name.startsWith ("tail");
                    if (isNew && ! params.hasProperty (name)) rp->setValueNotifyingHost (rp->getDefaultValue());
                }
    }
    return true;
}

bool ManipoloProcessor::saveKit (const juce::File& f)
{
    juce::MemoryBlock mb; getStateInformation (mb);
    if (auto xml = getXmlFromBinary (mb.getData(), (int) mb.getSize()))
    {
        // il tempo standalone non va nel kit? lo teniamo: utile in standalone
        if (xml->writeTo (f)) { currentKit = f; return true; }
    }
    return false;
}

bool ManipoloProcessor::loadKit (const juce::File& f)
{
    auto xml = juce::XmlDocument::parse (f);
    if (xml == nullptr) return false;
    juce::MemoryBlock mb; copyXmlToBinary (*xml, mb);
    setStateInformation (mb.getData(), (int) mb.getSize());
    currentKit = f;
    return true;
}

void ManipoloProcessor::stepKit (int dir)
{
    auto files = userFolder().getChildFile ("Kits").findChildFiles (juce::File::findFiles, false, "*.mkit");
    if (files.isEmpty()) return;
    files.sort();
    int idx = files.indexOf (currentKit);
    idx = idx < 0 ? 0 : (idx + dir + files.size()) % files.size();
    loadKit (files[idx]);
}

juce::AudioProcessorEditor* ManipoloProcessor::createEditor() { return new ManipoloEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ManipoloProcessor(); }
