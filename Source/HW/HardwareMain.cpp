// Drummatic hardware — applicazione a schermo intero per Raspberry Pi 5 + Pisound + circuito di controllo.
//   - audio su ALSA (preferisce la scheda "pisound"), MIDI DIN e USB verso il motore come nel plugin
//   - il circuito "Drummatic Control" (USB-MIDI, canale 16) comanda pagine, encoder e LED
//   - stato salvato in ~/Drummatic/hw_state.bin ogni 10 s e all'uscita
//   opzioni: --windowed (finestra 1280x720 per provarla su un PC), --list (elenca dispositivi ed esce)
#include <juce_audio_utils/juce_audio_utils.h>
#include <csignal>
#include "HardwareScreen.h"

namespace hw
{
static std::atomic<bool> quitRequested { false };      // SIGTERM da systemd: salva e chiudi
static void onSignal (int) { quitRequested = true; }

// lettore con volume generale dal potenziometro
class Player : public juce::AudioProcessorPlayer
{
public:
    explicit Player (std::atomic<float>& v) : vol (v) {}
    void audioDeviceIOCallbackWithContext (const float* const* in, int nIn, float* const* out, int nOut, int n,
                                           const juce::AudioIODeviceCallbackContext& ctx) override
    {
        juce::AudioProcessorPlayer::audioDeviceIOCallbackWithContext (in, nIn, out, nOut, n, ctx);
        const float target = std::pow (vol.load(), 2.0f);          // curva quasi logaritmica
        for (int i = 0; i < n; ++i)
        {
            g += (target - g) * 0.002f;
            for (int c = 0; c < nOut; ++c) if (out[c] != nullptr) out[c][i] *= g;
        }
    }
private:
    std::atomic<float>& vol; float g = 0.0f;
};

class MainComponent : public juce::Component, private juce::MidiInputCallback, private juce::Timer
{
public:
    MainComponent()
        : player (ctl.volume)
    {
        proc.setPlayConfigDetails (0, 2, 48000, 256);
        loadState();
        ctl.sendToPanel = [this] (const juce::MidiMessage& m) { if (panelOut != nullptr) panelOut->sendMessageNow (m); };
        addAndMakeVisible (screen);
        screen.onFrame = [this] { ctl.tick(); ctl.refreshLeds(); screen.panelConnected = panelOut != nullptr && panelInId.isNotEmpty(); };
        setupAudio();
        player.setProcessor (&proc);
        devices.addAudioCallback (&player);
        scanMidi();
        startTimer (500);
        setWantsKeyboardFocus (true);
    }

    ~MainComponent() override
    {
        stopTimer();
        saveState();
        if (panelOut != nullptr) for (int k = 0; k < kNumKeys; ++k) panelOut->sendMessageNow (juce::MidiMessage::noteOn (kMidiCh, k, (juce::uint8) 0));
        devices.removeAudioCallback (&player);
        player.setProcessor (nullptr);
        for (auto& id : openedInputs) devices.removeMidiInputDeviceCallback (id, this);
    }

    void resized() override
    {
        const float s = std::min (getWidth() / (float) Screen::W, getHeight() / (float) Screen::H);
        screen.setTransform (juce::AffineTransform::scale (s).translated ((getWidth() - Screen::W * s) * 0.5f, (getHeight() - Screen::H * s) * 0.5f));
    }
    void paint (juce::Graphics& g) override { g.fillAll (juce::Colours::black); }

    // tastiera del computer per le prove senza circuito: 1-8 pagine, Q-I voci, A-K + Z-, passi, spazio play
    bool keyPressed (const juce::KeyPress& k) override
    {
        const juce::String pages ("12345678"), voices ("qwertyui"), trigs ("asdfghjkzxcvbnm,");
        const auto ch = (juce::juce_wchar) juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
        auto press = [this] (int key) { ctl.noteOn (key); ctl.noteOff (key); return true; };
        if (pages.containsChar (ch)) return press (K_VOICE + pages.indexOfChar (ch));
        if (voices.containsChar (ch)) return press (K_V1 + voices.indexOfChar (ch));
        if (trigs.containsChar (ch)) return press (K_T1 + trigs.indexOfChar (ch));
        if (k == juce::KeyPress::spaceKey) return press (K_PLAY);
        if (k == juce::KeyPress::leftKey) return press (K_LEFT);
        if (k == juce::KeyPress::rightKey) return press (K_RIGHT);
        if (k == juce::KeyPress::upKey) return press (K_UP);
        if (k == juce::KeyPress::downKey) return press (K_DOWN);
        return false;
    }

    static juce::File stateFile() { return ManipoloProcessor::userFolder().getChildFile ("hw_state.bin"); }

private:
    ManipoloProcessor proc;
    Controller ctl { proc };
    Screen screen { ctl };
    juce::AudioDeviceManager devices;
    Player player;
    std::unique_ptr<juce::MidiOutput> panelOut;
    juce::StringArray openedInputs;
    juce::String panelInId;
    juce::AbstractFifo fifo { 1024 };
    std::array<juce::MidiMessage, 1024> fifoData;
    juce::MemoryBlock lastSaved;
    int saveCountdown = 5;

    void setupAudio()
    {
        auto xml = juce::XmlDocument::parse (ManipoloProcessor::userFolder().getChildFile ("audio.xml"));
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.sampleRate = 48000; setup.bufferSize = 128;
        devices.initialise (0, 2, xml.get(), true, {}, &setup);
        if (xml == nullptr)
        {
            // preferisci la Pisound se presente
            if (auto* type = devices.getCurrentDeviceTypeObject())
            {
                type->scanForDevices();
                for (auto& name : type->getDeviceNames (false))
                    if (name.containsIgnoreCase ("pisound"))
                    {
                        auto s = devices.getAudioDeviceSetup();
                        s.outputDeviceName = name; s.sampleRate = 48000; s.bufferSize = 128;
                        devices.setAudioDeviceSetup (s, true);
                        break;
                    }
            }
        }
        if (auto* d = devices.getCurrentAudioDevice())
            screen.audioStatus = d->getName() + "  " + juce::String (d->getCurrentSampleRate(), 0) + " Hz  " + juce::String (d->getCurrentBufferSizeSamples());
    }

    // collega il circuito e tutti gli altri ingressi MIDI (DIN della Pisound, tastiere USB)
    void scanMidi()
    {
        for (auto& d : juce::MidiInput::getAvailableDevices())
        {
            if (openedInputs.contains (d.identifier)) continue;
            const bool isPanel = d.name.containsIgnoreCase ("Drummatic Control");
            devices.setMidiInputDeviceEnabled (d.identifier, true);
            devices.addMidiInputDeviceCallback (d.identifier, isPanel ? (juce::MidiInputCallback*) this : &player);
            openedInputs.add (d.identifier);
            if (isPanel) panelInId = d.identifier;
        }
        bool stillThere = false;
        for (auto& d : juce::MidiInput::getAvailableDevices()) stillThere |= d.identifier == panelInId;
        if (! stillThere && panelInId.isNotEmpty()) { openedInputs.removeString (panelInId); panelInId = {}; panelOut.reset(); }
        if (panelOut == nullptr)
            for (auto& d : juce::MidiOutput::getAvailableDevices())
                if (d.name.containsIgnoreCase ("Drummatic Control"))
                {
                    panelOut = juce::MidiOutput::openDevice (d.identifier);
                    ctl.refreshLeds (true);
                    break;
                }
    }

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& m) override
    {
        // thread MIDI: accoda; lo elabora il thread dell'interfaccia
        int s1, n1, s2, n2;
        fifo.prepareToWrite (1, s1, n1, s2, n2);
        if (n1 > 0) fifoData[(size_t) s1] = m;
        fifo.finishedWrite (n1 + n2);
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)]
        {
            if (auto* c = safe.getComponent()) c->drain();
        });
    }
    void drain()
    {
        int s1, n1, s2, n2;
        fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) ctl.handle (fifoData[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) ctl.handle (fifoData[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
    }

    void timerCallback() override
    {
        if (quitRequested.load()) { juce::JUCEApplication::getInstance()->systemRequestedQuit(); return; }
        scanMidi();
        // messaggi dallo script della chiavetta USB (copia campioni, backup, aggiornamenti)
        auto msg = ManipoloProcessor::userFolder().getChildFile (".messaggio");
        if (msg.existsAsFile())
        {
            const auto text = msg.loadFileAsString().trim();
            msg.deleteFile();
            if (text.isNotEmpty()) ctl.say (text);
            ctl.rescanSamples();
        }
        if (--saveCountdown <= 0) { saveCountdown = 20; saveState(); }
    }

    void loadState()
    {
        juce::MemoryBlock mb;
        if (stateFile().loadFileAsData (mb) && mb.getSize() > 0) proc.setStateInformation (mb.getData(), (int) mb.getSize());
    }
    void saveState()
    {
        juce::MemoryBlock mb; proc.getStateInformation (mb);
        if (mb == lastSaved) return;
        auto tmp = stateFile().getSiblingFile ("hw_state.tmp");
        if (tmp.replaceWithData (mb.getData(), mb.getSize()) && tmp.moveFileTo (stateFile())) lastSaved = mb;
    }
};

class Window : public juce::DocumentWindow
{
public:
    Window (bool windowed) : juce::DocumentWindow ("Drummatic", juce::Colours::black, windowed ? juce::DocumentWindow::allButtons : 0)
    {
        setUsingNativeTitleBar (windowed);
        setContentOwned (new MainComponent(), false);
        if (windowed) { centreWithSize (Screen::W, Screen::H); setResizable (true, false); }
        else
        {
            setTitleBarHeight (0);
            setFullScreen (true);
            if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) setBounds (d->totalArea);
            setMouseCursor (juce::MouseCursor::NoCursor);
        }
        setVisible (true);
        getContentComponent()->grabKeyboardFocus();
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class App : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "Drummatic"; }
    const juce::String getApplicationVersion() override { return "0.3.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }
    void initialise (const juce::String& cmd) override
    {
        if (cmd.contains ("--list"))
        {
            juce::AudioDeviceManager dm; dm.initialise (0, 2, nullptr, true);
            for (auto* t : dm.getAvailableDeviceTypes()) { t->scanForDevices(); for (auto& n : t->getDeviceNames (false)) std::cout << "audio: " << t->getTypeName() << " / " << n << "\n"; }
            for (auto& d : juce::MidiInput::getAvailableDevices()) std::cout << "midi in: " << d.name << "\n";
            for (auto& d : juce::MidiOutput::getAvailableDevices()) std::cout << "midi out: " << d.name << "\n";
            quit(); return;
        }
        std::signal (SIGTERM, onSignal); std::signal (SIGINT, onSignal);
        window = std::make_unique<Window> (cmd.contains ("--windowed"));
    }
    void shutdown() override { window.reset(); }
    void systemRequestedQuit() override { quit(); }
private:
    std::unique_ptr<Window> window;
};
} // namespace hw

START_JUCE_APPLICATION (hw::App)
