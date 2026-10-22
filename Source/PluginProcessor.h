#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Params.h"

class ManipoloProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ManipoloProcessor();
    ~ManipoloProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Drummatic"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- API per l'interfaccia ---
    juce::AudioProcessorValueTreeState apvts;
    mp::DrumMachine dm;

    juce::RangedAudioParameter* param (int voice, const juce::String& name) { return apvts.getParameter (mpp::vid (voice, name)); }
    void setParam (int voice, const juce::String& name, float realValue);

    bool loadSample (int voice, const juce::File& f);
    void clearSample (int voice);
    juce::File getSampleFile (int voice) const { return sampleFiles[(size_t) voice]; }
    std::shared_ptr<const mp::SampleData> getSample (int voice) const;

    void randomizeVoice (int voice);
    void resetVoice (int voice);
    void randomizeAll() { for (int v = 0; v < mp::kNumVoices; ++v) randomizeVoice (v); }
    // Performance
    void perfRandomize();
    void perfReset();
    void perfCommit();
    std::atomic<bool> perfLock[mpp::kNumPerf];

    juce::ValueTree voiceToTree (int voice);
    void voiceFromTree (int voice, const juce::ValueTree& t);
    bool saveVoicePreset (int voice, const juce::File& f);
    bool loadVoicePreset (int voice, const juce::File& f);
    bool saveKit (const juce::File& f);
    bool loadKit (const juce::File& f);
    void stepKit (int dir);          // preset precedente/successivo (cartella Kits)
    static juce::File userFolder();
    juce::File currentKit;

    std::atomic<bool> hostDrivesTransport { false }, hostPlaying { false };
    juce::AudioFormatManager formats;

private:
    std::shared_ptr<mp::SampleData> sampleData[mp::kNumVoices];
    std::array<juce::File, mp::kNumVoices> sampleFiles;
    juce::SpinLock sampleLock;
    std::vector<std::pair<std::shared_ptr<mp::SampleData>, juce::uint64>> graveyard;
    std::atomic<juce::uint64> blockCount { 0 };

    // puntatori ai valori dei parametri (letti nel thread audio)
    struct VP
    {
        std::atomic<float>* target[mp::kNumTargets] {};
        std::atomic<float>* synthOn, *engine, *env2, *stereo, *sampOn, *sampEng, *sampEnv, *fmode, *post, *dmode,
                          *e1curve, *e1vel, *e2curve, *e2vel, *l1sync, *l1bip, *l1wave, *l1reset, *l1shape,
                          *l2sync, *l2bip, *l2wave, *l2reset, *l2shape, *macro1, *macro2, *variation, *choke, *link,
                          *steps, *fill, *pattern, *rotate, *invert, *sshift, *swing, *shape, *density, *speed, *mult,
                          *length, *snap, *resetOn, *resetLen, *route, *pfEnable, *pfFlip;
    } vp[mp::kNumVoices];
    std::atomic<float>* g (const char* id) { return apvts.getRawParameterValue (id); }
    struct GP
    {
        std::atomic<float>* dSync, *dDiv, *dTime, *dFb, *dGlitch, *dFbTilt, *dDuck, *dSRate, *dGain, *dStereo, *dPong, *dToRev,
                          *rSync, *rDiv, *rPre, *rDecay, *rSRate, *rInTilt, *rOutTilt, *rGain, *rMode,
                          *mMax, *mLow, *mMid, *mHigh, *mGain, *mClip, *humanize, *bpm, *pfDepth, *pfEnabled;
        std::atomic<float>* perf[mpp::kNumPerf];
    } gp {};

    void cacheParams();
    void pullSettings (double bpm);
    void applyPerformance (int v, float k);
    void timerCallback() override;
    juce::Random random;
    juce::ValueTree extraToTree();
    void extraFromTree (const juce::ValueTree& t);
};
