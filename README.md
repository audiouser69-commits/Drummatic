# Drummatic 0.2.3 — drum synth a 8 voci

Standalone per Windows (`Drummatic.exe` / `Drummatic.app`) e plugin VST3 (`Drummatic.vst3`) per Windows e macOS dallo stesso codice JUCE.
Architettura modellata sulle funzioni di una drum machine ibrida synth/sampler con sequencer euclideo;
nome, grafica e codice sono originali.

## Installazione
- **Standalone**: copia `Drummatic.exe` dove vuoi e avvialo. In *Options → Audio/MIDI Settings* scegli scheda audio
  (WASAPI/DirectSound) e, se vuoi, l'ingresso MIDI.
- **VST3 Windows**: copia `Drummatic.vst3` in `C:\Program Files\Common Files\VST3\` e riesegui la scansione in Ableton.
- **macOS**: `Drummatic.vst3` in `~/Library/Audio/Plug-Ins/VST3/`, `Drummatic.app` in Applicazioni; al primo avvio
  `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Drummatic.vst3 /Applications/Drummatic.app`.
- Preset e kit vengono salvati in `Documenti/Drummatic/` (`Kits`, `Voices`, `Samples`); i file di `Documenti/Manipolo` vengono copiati lì al primo avvio.
- Manuale completo: `docs/Drummatic-Manuale.pdf`.

## Funzioni
**Voce (×8)**
- Synth con 25 motori: Kick, Snare, Hat, Clap, FM, Modal, Cowbell, Zap, Pillow, Tripwire, Radiator, Kombucha
  (Karplus-Strong), LPG, Builder, Additive, Chaos, VOSIM, SumSyn, Reson, Combinatoric, Terraining, Shame, Entangled,
  Haywire, Scrapyard — PITCH + 4 manopole specifiche, ENV 2 come sorgente d'ampiezza, STEREO.
- Sampler con 4 motori: Classic (FINE, TILT, COMP, GAIN), Phase Warp (MIRROR, SHAPE, SPLIT, SWING),
  Cloner (FINE, TILT, VARIATION, DAMP — ogni colpo leggermente diverso; realizzato con un banco di 8 bande),
  Granular (SIZE, RATE, SPEED, BLUR). START/END trascinabili (END prima di START = reverse),
  drag & drop di WAV/AIFF/FLAC/OGG/MP3, file precedente/successivo/casuale nella cartella.
- BLEND synth/sampler (al 50% entrambi al massimo).
- TONE: 11 filtri (Scream/Dark LP-HP-BP, Comb FB±, FF±, AP), 11 distorsioni (Tape, Tube, Bitcrush, Rate Crush,
  Wavefold, Phase, Square, Dropout, MiniGlitch, MegaGlitch, Contrast), POST.
- Modulazione: RANDOM, VELOCITY, MACRO 1/2, ENV 1/2, LFO 1/2. Premi **MAP** su una sorgente e trascina le manopole
  per la profondità (doppio clic azzera). Tasto destro su una manopola = **lucchetto** (esclusa da VARIATION,
  randomizzazione e cambio preset). Alt+clic = valore di default.
- 2 inviluppi AHD (curva, velocity, soft reset), 2 LFO (Wavetable/Triangle Tilt/Pulse/Smooth Random,
  5 modalità di reset, sync o Hz fino a 440 Hz).
- VARIATION, mandate Delay/Reverb, PAN, VOICE GAIN; preset di voce, RESET, DADO.

**Sequencer per voce**
- Generatore euclideo STEPS/FILL/PATTERN (permutazioni)/ROTATE/INVERT + trigger manuali.
- SHIFT, SWING, SHAPE (accelera/rallenta), DENSITY; SPEED × MULT, LENGTH, SNAP, RESET; HUMANIZE globale condiviso.
- Lane per passo: VEL (127 = spinta extra di distorsione), SHIFT, PITCH, RATCH, ENV, PAN, VAR, PROB, SENDS, MACRO 2,
  ognuna con SYNC/FREE/TRIG/RETRIG e lunghezza propria; X, ruota ◀ ▶ (Alt = anche i trigger), DADO.
- Vista **ALL**: le sequenze delle 8 voci insieme, clic per aggiungere/togliere trigger, clic sul numero per selezionare la voce.
- Trasporto: segue il DAW, PLAY indipendente, STOP. In standalone usa il tempo in alto (BPM).
  Quando il DAW parte, il sequencer si riarma sempre, anche dopo uno STOP del plugin.

**Performance**
- ENV, SYNTH, SAMPLER, MODULATION (Macro 1/2, Variation, MOD AMP), TONE e SEQUENCER (Steps, Fill, Pattern, Density,
  Prob, Ratchet) agiscono su tutte le voci insieme.
- Per voce ENABLE e FLIP; GLOBAL con DEPTH bipolare, DADO (con profondità propria), COMMIT, RESET, ENABLED.
- Tasto destro su una manopola Performance la esclude dal DADO; i lucchetti delle voci la escludono dalla Performance.

**Mixer / FX / Master**
- Strip per voce con DELAY, REVERB, PAN, GAIN, TRIGGER (catena), CHOKE, ROUTING, Mute/Solo.
- **Uscite multiple**: Main 1-2 + 7 coppie stereo (3-4 … 15-16). In Ableton attiva le uscite del plugin e crea tracce
  audio con ingresso da Manipolo. Il MASTER agisce solo sul Main; le mandate FX escono sempre dal Main.
- Shatter Delay (tempo istantaneo senza glissati, GLITCH, STEREO, PONG, DUCKING, FB TILT, S.RATE, mandata al riverbero).
- Headspace Reverb (FDN, 4 modi, PREDELAY sync, DECAY, S.RATE, IN/OUT TILT).
- Master: MAXIMIZE, EQ 3 bande, Soft/Hard/Wavefold/No clip, GAIN.

**MIDI** (come da specifica)
- Ch 1: Do…Sol (qualsiasi ottava) = voci 1–8 · Ch 2–9: cromatico per voce (C3/60 = intonazione base)
- Ch 10: 0 play/stop, 1 play, 2 stop, 12–19 mute, 24–31 solo, 36–43 mute momentaneo, 48–55 solo momentaneo, 60–67 choke
- Ch 11: 0/1 kit prec./succ., 2 kit casuale, 3 randomizza tutto, 12 dado Performance, 13 reset Performance,
  14 commit Performance, 24–31 randomizza voce

**Tastiera** (pulsante TASTI attivo e finestra a fuoco): Q suona la voce, W/S voce su/giù, A/D pagina (VOICE/SEQ/MIX/PERFORM);
Spazio play/stop solo nello standalone. Nel DAW TASTI è spento di default, così spazio e tastiera MIDI del computer restano al DAW.
Shift+clic o doppio clic su una voce a sinistra = ascolto.

## Drummatic per M-VAVE FM-1
La cartella [`fm1/`](fm1/README.md) contiene Drummatic come **firmware per l'M-VAVE FM-1** (tutto tranne il sampler e
le uscite multiple: 8 voci, 25 motori, TONE, inviluppi, LFO, modulazione MAP, sequencer euclideo con le 10 lane,
Performance, delay, riverbero, master, MIDI) e un **emulatore nel browser** con lo stesso codice. Licenza GPL-3.0
(la piattaforma FM-1 viene da Felucca). Build: workflow *FM-1* di GitHub Actions (pacchetto `.fwsc`, emulatore,
Flasher per Windows).

## Drummatic hardware (Raspberry Pi 5) — novità 0.3.0
- Nuova app a schermo intero `drummatic-hw` per la macchina: Raspberry Pi 5 + Pisound + Touch Display 2 + circuito di controllo
  (cartella `hardware/`). Stesso motore audio del plugin.
- Il circuito "Drummatic Control" (USB-MIDI, canale 16) comanda 8 pagine (VOICE, SAMPLER, TONE, MOD, SEQ, LANE, MIX, PERF),
  8 encoder con banchi (frecce sinistra/destra), DATA, 16 tasti di passo, 8 tasti voce, trasporto, MAP, LOCK, DADO,
  COMMIT, KIT, COPY; i LED mostrano pagina, voce, passi e testina.
- Installazione: pacchetto `Drummatic-SD` (Raspberry Pi OS Lite 64 bit trixie + cartella `drummatic` sulla partizione bootfs),
  aggiornamenti, campioni e backup da chiavetta USB.
- Compilare: `cmake -S . -B build-a64 -G Ninja -DCMAKE_TOOLCHAIN_FILE=aarch64-toolchain.cmake -DDRUMMATIC_HW=ON`
  (sysroot con alsa-lib e freetype per aarch64), oppure nativo su Linux con `-DDRUMMATIC_HW=ON`.
  `DrummaticHWTest` simula il pannello e salva le schermate di tutte le pagine.

## Novità 0.2.3
- START ed END del sampler modulabili in modalità MAP trascinando i marcatori sulla forma d'onda (es. lane MACRO 2 → START per affettare un loop).
- Tempi visualizzati in modo pulito ("50 ms").
- Manuale: nuovo capitolo "Il sampler in pratica" con schermate annotate, grafici dei motori e sei tecniche.

## Novità 0.2.2
- Nuovo nome Drummatic, versione macOS universale, cartella utente `Documenti/Drummatic` con migrazione automatica.
- Corretta l'evidenziazione delle pagine nella barra superiore.

## Novità 0.2.1
- Risolto: l'audio poteva sparire durante la riproduzione nel DAW. Con la finestra del plugin attiva, lo spazio
  (e i tasti A/W/S/D della tastiera MIDI di Ableton) venivano catturati da Manipolo, che fermava il proprio sequencer.
  Ora i tasti restano al DAW e il play del DAW riarma comunque il sequencer.
- Un nuovo colpo annulla un CHOKE ancora in corso sulla stessa voce (prima poteva restare attenuata per un attimo).
- Distorsioni azzerate quando una voce riparte dal silenzio (Dropout non può più restare bloccato).
- Il DADO di voce/RANDOM non cambia più ROUTING ed ENABLE/FLIP della Performance (poteva mandare una voce
  su un'uscita ausiliaria non attiva, facendola sparire dal Main).
- Protezione contro blocchi audio più grandi di quelli annunciati dall'host.

## Limiti noti
- Driver **ASIO** non incluso nello standalone (l'SDK ASIO di Steinberg non è ridistribuibile): usa WASAPI in modalità
  esclusiva per una latenza bassa, oppure il VST3 dentro Ableton.
- La modulazione è calcolata ogni 16 campioni (≈0,3 ms): gli LFO fino a 440 Hz sono "audio-rate" solo in modo approssimato.
- Il Cloner usa un banco di filtri a 8 bande invece di una vera elaborazione FFT.
- Binari non firmati da un'autorità: Windows SmartScreen e macOS Gatekeeper mostrano un avviso al primo avvio.

## Compilare
Cross-compilazione da Linux: `cmake -S . -B build-win -G Ninja -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake -DCMAKE_BUILD_TYPE=Release && cmake --build build-win`
(JUCE 7.0.12, scaricato automaticamente se manca la cartella `JUCE`). Su Windows funziona anche con Visual Studio 2022;
su macOS: `cmake -S . -B build -G Xcode -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" && cmake --build build --config Release`.
Test: `-DMANIPOLO_TESTS=ON` poi `ManipoloTest`.
