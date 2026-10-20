// Collaudo dell'app hardware senza circuito: simula i messaggi MIDI del pannello,
// verifica parametri, sequencer e LED, e salva una schermata PNG per ogni pagina.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/HW/HardwareScreen.h"

static int failures = 0;
#define CHECK(c) do { if (! (c)) { std::cout << "FALLITO: " #c " (riga " << __LINE__ << ")\n"; ++failures; } } while (0)

using namespace hw;
static juce::MidiMessage note (int n, bool on = true) { return on ? juce::MidiMessage::noteOn (kMidiCh, n, (juce::uint8) 127) : juce::MidiMessage::noteOff (kMidiCh, n); }
static juce::MidiMessage enc (int i, int d) { return juce::MidiMessage::controllerEvent (kMidiCh, kEncCC0 + i, d > 0 ? d : 128 + d); }
static void press (Controller& c, int k) { c.handle (note (k)); c.handle (note (k, false)); }

static void runAudio (ManipoloProcessor& p, int blocks)
{
    juce::AudioBuffer<float> buf (p.getTotalNumOutputChannels(), 256); juce::MidiBuffer mb;
    for (int i = 0; i < blocks; ++i) { buf.clear(); p.processBlock (buf, mb); }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::File outDir (argc > 1 ? argv[1] : "hw_screens");
    outDir.createDirectory();
    ManipoloProcessor proc;
    proc.setPlayConfigDetails (0, proc.getTotalNumOutputChannels(), 48000, 256);
    proc.prepareToPlay (48000, 256);
    Controller ctl (proc);
    std::vector<juce::MidiMessage> sent;
    ctl.sendToPanel = [&] (const juce::MidiMessage& m) { sent.push_back (m); };

    // pagine e voce
    press (ctl, K_SEQ); CHECK (ctl.page == P_Seq);
    press (ctl, K_V3); CHECK (ctl.voice == 2);
    press (ctl, K_DOWN); CHECK (ctl.voice == 3);
    press (ctl, K_UP); CHECK (ctl.voice == 2);

    // encoder 2 sulla pagina SEQ = FILL della voce 3
    const int fill0 = (int) proc.param (2, "fill")->convertFrom0to1 (proc.param (2, "fill")->getValue());
    ctl.handle (enc (1, 1)); ctl.handle (enc (1, 1)); ctl.handle (enc (1, 1));
    const int fill1 = (int) proc.param (2, "fill")->convertFrom0to1 (proc.param (2, "fill")->getValue());
    CHECK (fill1 == fill0 + 3);
    ctl.handle (juce::MidiMessage::noteOn (kMidiCh, kEncNote0 + 1, (juce::uint8) 127));        // pressione = valore iniziale
    CHECK ((int) proc.param (2, "fill")->convertFrom0to1 (proc.param (2, "fill")->getValue()) == mpp::voiceDefaults (2).fill);

    // passi: TRIG 1 e 5 accesi
    press (ctl, K_T1); press (ctl, K_T1 + 4);
    CHECK (proc.dm.sshared[2].manual[0].load() && proc.dm.sshared[2].manual[4].load());
    press (ctl, K_T1 + 4); CHECK (! proc.dm.sshared[2].manual[4].load());
    press (ctl, K_T1 + 4);

    // pagina VOICE: encoder 1 = pitch, DATA = motore
    press (ctl, K_VOICE);
    const float pitch0 = proc.param (2, "pitch")->getValue();
    ctl.handle (enc (0, 5)); CHECK (proc.param (2, "pitch")->getValue() > pitch0);
    const int eng0 = proc.param (2, "engine")->getValue() > 0 ? juce::roundToInt (proc.param (2, "engine")->getValue() * (proc.param (2, "engine")->getNumSteps() - 1)) : 0;
    ctl.handle (juce::MidiMessage::controllerEvent (kMidiCh, kDataCC, 1));
    CHECK (juce::roundToInt (proc.param (2, "engine")->getValue() * (proc.param (2, "engine")->getNumSteps() - 1)) == eng0 + 1);

    // banchi con le frecce
    press (ctl, K_RIGHT); CHECK (ctl.bank[P_Voice] == 1);
    press (ctl, K_RIGHT); CHECK (ctl.bank[P_Voice] == 1);       // non oltre l'ultimo
    press (ctl, K_LEFT); CHECK (ctl.bank[P_Voice] == 0);

    // MAP: profondità di modulazione su PITCH dalla sorgente scelta
    press (ctl, K_MAP); CHECK (ctl.mapMode);
    ctl.handle (juce::MidiMessage::controllerEvent (kMidiCh, kDataCC, 1));
    const int src = ctl.mapSource;
    ctl.handle (enc (0, 10));
    CHECK (proc.dm.vshared[2].depth[src][mp::T_Pitch].load() > 0.05f);
    press (ctl, K_MAP); CHECK (! ctl.mapMode);

    // LOCK sull'ultimo parametro toccato
    ctl.handle (enc (0, 1)); press (ctl, K_LOCK);
    CHECK (proc.dm.vshared[2].locked[mp::T_Pitch].load());

    // FUNC + voce = mute; pagina MIX: tasto voce = mute, encoder = volume della voce fissa
    ctl.handle (note (K_FUNC)); press (ctl, K_V5); ctl.handle (note (K_FUNC, false));
    CHECK (proc.dm.mute[4].load());
    press (ctl, K_MIX); press (ctl, K_V5); CHECK (! proc.dm.mute[4].load());
    const float g7 = proc.param (6, "gain")->getValue();
    ctl.handle (enc (6, -4)); CHECK (proc.param (6, "gain")->getValue() < g7);

    // LANE: TRIG sceglie il passo, encoder 1 cambia il valore della lane
    press (ctl, K_LANE);
    ctl.handle (juce::MidiMessage::controllerEvent (kMidiCh, kDataCC, 1)); CHECK (ctl.lane == mp::L_Shift);
    ctl.handle (juce::MidiMessage::controllerEvent (kMidiCh, kDataCC, 1)); CHECK (ctl.lane == mp::L_Pitch);
    press (ctl, K_T1 + 6); CHECK (ctl.laneStep == 6);
    for (int i = 0; i < 7; ++i) ctl.handle (enc (0, 1));
    CHECK (juce::roundToInt (proc.dm.sshared[2].lane[mp::L_Pitch][6].load()) == 7);

    // COPY / FUNC+COPY
    press (ctl, K_VOICE); press (ctl, K_V3); press (ctl, K_COPY);
    press (ctl, K_V8); ctl.handle (note (K_FUNC)); press (ctl, K_COPY); ctl.handle (note (K_FUNC, false));
    CHECK (std::abs (proc.param (7, "pitch")->getValue() - proc.param (2, "pitch")->getValue()) < 1e-6f);
    press (ctl, K_V3);

    // volume dal potenziometro
    ctl.handle (juce::MidiMessage::controllerEvent (kMidiCh, kVolumeCC, 64)); CHECK (std::abs (ctl.volume.load() - 64 / 127.0f) < 1e-4f);

    // trasporto: PLAY fa partire il sequencer, i LED dei passi seguono
    press (ctl, K_SEQ);
    press (ctl, K_PLAY);
    runAudio (proc, 60);
    CHECK (proc.dm.running.load());
    sent.clear(); ctl.refreshLeds (true);
    CHECK (sent.size() == (size_t) kNumKeys);
    int trig1 = -1, play = -1, page = -1;
    for (auto& m : sent)
    {
        if (m.getNoteNumber() == K_T1) trig1 = m.getVelocity();
        if (m.getNoteNumber() == K_PLAY) play = m.getVelocity();
        if (m.getNoteNumber() == K_SEQ) page = m.getVelocity();
    }
    CHECK (play == L_Green); CHECK (page == L_White); CHECK (trig1 == L_Orange || trig1 == L_White);
    sent.clear(); ctl.refreshLeds(); const size_t diff = sent.size();
    CHECK (diff < 10);    // solo i LED cambiati

    // REC: suonando una voce si scrive il passo corrente
    press (ctl, K_REC); CHECK (ctl.recArmed);
    runAudio (proc, 7);
    const int ps = proc.dm.sshared[5].playStep.load();
    press (ctl, K_V6);
    CHECK (ps < 0 || proc.dm.sshared[5].manual[ps].load() || proc.dm.sshared[5].manual[proc.dm.sshared[5].playStep.load()].load());
    press (ctl, K_STOP); CHECK (! ctl.recArmed);
    press (ctl, K_PLAY); runAudio (proc, 40);
    press (ctl, K_V3);

    // schermate di tutte le pagine
    Screen scr (ctl);
    scr.setSize (Screen::W, Screen::H);
    for (int p = 0; p < kNumPages; ++p)
    {
        press (ctl, K_VOICE + p);
        if (p == P_Mix) { ctl.handle (enc (0, 2)); }
        if (p == P_Mod) { press (ctl, K_MAP); ctl.handle (enc (0, 3)); }
        runAudio (proc, 9);
        auto img = scr.createComponentSnapshot (scr.getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png; auto f = outDir.getChildFile (juce::String ("pagina_") + juce::String (p + 1) + "_" + pageName (p) + ".png");
        f.deleteFile(); juce::FileOutputStream os (f); png.writeImageToStream (img, os);
        if (p == P_Mod) press (ctl, K_MAP);
    }
    std::cout << (failures == 0 ? "TUTTI I TEST SUPERATI" : "TEST FALLITI: ") << (failures ? juce::String (failures) : juce::String()) << "\n";
    return failures == 0 ? 0 : 1;
}
