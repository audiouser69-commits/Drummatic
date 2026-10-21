# Drummatic per M-VAVE FM-1

Il drum synth Drummatic (8 voci, 25 motori di sintesi, sequencer euclideo per voce con 10 lane, Performance, delay,
riverbero e master) come **firmware per l'M-VAVE FM-1**. Lo stesso motore audio gira anche nel **browser**
(emulatore), così si prova tutto prima di installarlo.

Il motore è un port in C del DSP di Drummatic 0.2.3 (`../Source/DSP`), verificato voce per voce contro l'originale
(`tests/dm_compare.cpp`). La piattaforma dell'FM-1 (schermo, tasti, USB MIDI e audio, flash, aggiornamento) viene da
[Felucca](https://github.com/hugelton/Felucca) di Leo Kuroshita, GPL-3.0: per questo la cartella `fm1/` è
**GPL-3.0-only** (vedi `LICENSE`).

## Cosa c'è e cosa no

C'è tutto Drummatic tranne il **sampler** (niente campioni sull'FM-1: BLEND, i 4 motori del sampler, START/END) e le
**uscite multiple** (l'FM-1 ha un'uscita stereo; la pagina ROUTING tiene CHOKE, TRIGGER e gli interruttori della
Performance).

| Drummatic (plugin) | FM-1 |
|---|---|
| 8 voci, 25 motori, PITCH + 4 manopole, STEREO, ENV 2 come ampiezza | EDIT: SYNTH, ENGINE |
| TONE: 11 filtri, 11 distorsioni, POST | FX: TONE, TONE 2 |
| 2 inviluppi AHD (curva, velocity) | ENV: ENV 1, ENV 2, VELOCITY |
| 2 LFO (4 forme, 5 reset, sync o Hz fino a 440) | LFO: LFO 1, LFO 1+, LFO 2, LFO 2+ |
| MAP: sorgenti RANDOM VELOCITY MACRO 1/2 ENV 1/2 LFO 1/2 | SCL tenuto + una manopola; SCL: MOD, MACRO, LOCKS |
| Lucchetti (tasto destro) | SAVE tenuto + una manopola |
| VARIATION, MACRO 1/2 | SCL: MACRO |
| Sequencer euclideo STEPS FILL PATTERN ROTATE INVERT, SHIFT SWING SHAPE DENSITY, SPEED × MULT, LENGTH, SNAP, RESET | SEQ: EUCLID, TIME, GROOVE, MORE |
| 10 lane per passo (VEL SHIFT PITCH RATCH ENV PAN VAR PROB SENDS MACRO 2) con SYNC/FREE/TRIG/RETRIG e lunghezza | tasti bianchi tenuti + manopole; SEQ: LANE, TOOLS |
| Trigger manuali, vista ALL | tasti bianchi; le 8 sequenze sempre sullo schermo |
| Mixer: GAIN PAN DELAY REVERB, TRIGGER, CHOKE, MUTE, SOLO | HOME: MIX, ROUTING; tasti neri 9 MUTE e 10 SOLO |
| Performance (22 controlli, DEPTH, ENABLE/FLIP per voce, DADO, COMMIT, RESET) | ARP: 7 pagine PF |
| Shatter Delay, Headspace Reverb, Master (MAXIMIZE, EQ, clip) | GLO: DELAY, REVERB, MASTER |
| DADO di voce, di lane, della Performance, randomizza tutto | tasto nero 11 DADO (dipende dalla pagina) |
| Kit e preset di voce in Documenti | SAVE: 16 kit e 16 preset di voce nella flash, autosave |
| MIDI ch 1–11 come da specifica | uguale, via USB e TRS; clock MIDI in ingresso (GLO: TEMPO > CLOCK EXT) |

Differenze volute:
- gli **LFO sincronizzati restano a tempo**: nell'originale il tick di controllo dura 17 campioni ma ne conta 16, così
  gli LFO in sync girano il 6 % più lenti e scivolano rispetto al tempo;
- lo **Shatter Delay** arriva a 1,49 s e il **predelay** del riverbero a 0,5 s (la RAM dell'FM-1: delay e riverbero
  girano a 22 kHz); il glitch dentro la voce (MINI/MEGA GLITCH) ha un buffer di ~160 ms (l'originale 250 ms);
- l'interfaccia dell'FM-1 al posto del mouse (sotto).

## Manuale

[`docs/Drummatic-FM1-Manuale.pdf`](docs/Drummatic-FM1-Manuale.pdf): il pannello, lo schermo e tutti i menu del firmware, pagina per pagina (39 pagine). Sorgenti in `docs/manual/`: `manual.html`, `build.py` (PDF) e `capture.mjs` (le schermate dall'emulatore).

## Il pannello

- **PLAY** avvia e ferma. **Tasti neri 1–8**: scegli la voce. **Tasti bianchi 1–16**: i passi della voce (OCT− / OCT+:
  le pagine dei passi 17–32 … 113–128); un tocco aggiunge o toglie un trigger.
- **Tieni uno o più tasti bianchi e gira KNOB 1–4**: le lane di quei passi (VEL, PITCH, RATCH, PROB); **PRESETS**
  mentre li tieni passa al gruppo SHIFT ENV PAN VAR e poi SENDS MACRO 2.
- I gruppi di pagine: **EDIT** SYNTH · ENGINE · VOICE (COPY, PASTE, RESET, PRESETS) — **FX** TONE · TONE 2 — **ENV**
  ENV 1 · ENV 2 · VELOCITY — **LFO** LFO 1 · LFO 1+ · LFO 2 · LFO 2+ — **SCL** MOD · MACRO · LOCKS — **SEQ** EUCLID ·
  TIME · GROOVE · MORE · LANE · TOOLS — **HOME** MIX · ROUTING — **ARP** PF ENV · PF SYNTH · PF MOD · PF TONE · PF SEQ ·
  PF SEQ 2 · PF GLOBAL — **GLO** TEMPO · DELAY (3) · REVERB (3) · MASTER (2) — **SAVE** KITS · VOICES.
  Lo stesso tasto di nuovo: la pagina dopo. **PRESETS** scorre le pagine del gruppo.
- **SELECT** tempo, **ALGORITHM** voce, **MASTER** volume.
- **SCL tenuto + una manopola** (su qualsiasi pagina del suono): MAP, la sorgente scelta in SCL > MOD modula quel
  controllo e il giro ne regola la profondità. Un puntino a destra della barra: il controllo è modulato.
- **SAVE tenuto + una manopola**: lucchetto (resta fuori da VARIATION, DADO, preset di voce); sulle pagine PF esclude il
  controllo dal DADO della Performance. Le etichette bloccate sono rosse.
- **Tasto nero 9 MUTE, 10 SOLO**: un tocco sulla voce scelta; tenuto insieme ai tasti voce, su quelle voci.
  **11 DADO**: il dado della pagina (pagine del suono: la voce; SEQ: la lane; ARP: la Performance; GLO: tutte le voci);
  tenuto insieme ai tasti voce: quelle voci.
- **REC**: PADS (bianchi 1–8 suonano le voci 1–8, 9–16 la voce scelta in altezza, OCT± l'ottava), di nuovo REC (i pad
  scrivono trigger mentre suona), di nuovo i passi.
- **SAVE**: KITS. I tasti bianchi caricano gli slot 1–16 (accesi: usati), **OCT+** salva nello slot di KNOB 1 (se è
  usato, OCT+ due volte), KNOB 2 e 3 il nome. SAVE di nuovo: VOICES, 16 preset di voce (bianco: carica nella voce
  scelta, OCT+ salva la voce).
- **HOME tenuto**: il menu (palette, LED, livello dell'uscita USB, autosave). **OCT− + OCT+ tenuti 5 s**: UBOOT (per
  installare qualunque firmware).

**Kit di riferimento "Dael"**: SAVE > KITS, gira KNOB 4 (NEW) a destra due volte e premi OCT+: l'intro di "Dael" come
l'hai ricostruita sull'FM-1 (90 BPM, −20 cent, una battuta: colpo e buzz che cade sull'1, buzz che cresce sul 4, rumore
sul 5, zap sul 6, pila metallica sull'8, hat sul 9 e sul 13; la voce 8 è la cassa del riferimento cassa/basso, fuori dal
pattern). I livelli delle voci sono misurati sui tuoi numeri (`tests/dm_ref.c`: RMS di ogni parte entro 0,6 dB, mix
−20,4 dBFS RMS, picco −2,5, crest 18 dB); timbri e bilanciamento di banda sono un'approssimazione con i motori di
Drummatic (girato a destra una volta sola: il kit INIT).

L'**autosave**: con la macchina ferma e niente toccato per 4 s, il kit va nella flash; all'accensione riparte da lì.
Un salvataggio cancella la flash e ferma l'audio per qualche decina di ms (un piccolo buco, meglio da fermi).

MIDI (USB e TRS): ch 1 Do…Sol = voci 1–8 · ch 2–9 la voce in altezza (Do3 = intonazione base) · ch 10: 0 play/stop,
1 play, 2 stop, 12–19 mute, 24–31 solo, 36–43 mute momentaneo, 48–55 solo momentaneo, 60–67 choke · ch 11: 0/1 kit
prec./succ., 2 kit a caso, 3 randomizza tutto, 12 dado Performance, 13 reset, 14 commit, 24–31 randomizza voce.
Clock: GLO > TEMPO > CLOCK EXT segue START/STOP/clock (24 per quarto) da USB o TRS.

## Installare

> Sostituisce il firmware dell'FM-1: a tuo rischio. Per tornare indietro: il tuo firmware Autechre Machine Plus con lo
> stesso Flasher, o il V15 ufficiale (M-VAVE o "Return to official V15" del web installer di Felucca).

- **Windows**: `DrummaticFlasher` (dalle build di GitHub Actions) ha il firmware dentro: collega l'FM-1, Installa.
  Va bene anche l'**Autechre Machine Plus Flasher** che hai già: "Un altro file .fwsc" → `drummatic-fm1-*.fwsc`.
- **Da riga di comando** (Windows, macOS, Linux): `pip3 install mido python-rtmidi`, poi
  `python3 fm1/tools/fm1_install.py drummatic-fm1-X.Y.fwsc`.
- **Browser** (Chrome/Edge): il [FM-1 Firmware Hub](https://fm1.designburgapps.com) accetta qualunque `.fwsc` per FM-1.

## Emulatore

`fm1/web/`: lo stesso codice del firmware compilato in WebAssembly, con il pannello disegnato. I kit restano nel
browser. Tastiera del computer: Z… e Q… i tasti, 1…= i bottoni, [ ] OCT, frecce SELECT e ALGORITHM, Shift-clic tiene
premuto un bottone.

`sh web/build.sh` fa `build/web/` e anche `drummatic-fm1-emulator.html`, tutto in un file. Il browser vuole una pagina
servita (non `file://`): `python3 -m http.server -d build/web`, poi http://localhost:8000.

## Compilare

Vedi [BUILDING.md](BUILDING.md). In breve, su Linux x86-64:

```
fm1/tools/get_toolchain.sh                    # toolchain JieLi in ~/.jieli/toolchain
git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK
cd fm1 && JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py   # build/drummatic.fwsc
sh web/build.sh                                # build/web/: l'emulatore (clang + wasm-ld)
sh tests/run_tests.sh                          # il port contro il DSP originale, il pannello nell'emulatore
```

## Carico della CPU

L'FM-1 non è un PC. Il kit di accensione costa circa 1500 istruzioni host per campione (stima: ~25 % dell'FM-1),
otto voci pesanti in stereo con filtri a pettine, glitch, delay e riverbero insieme molto di più. In alto a destra lo
schermo mostra il carico misurato sull'FM-1; sopra l'85 % per due mezzi buffer di fila il firmware chiude la voce più
bassa (come Felucca). I motori più cari: ADDITIVE (24 parziali), SCRAPYARD, MODAL, RESON; STEREO raddoppia il motore,
MINI/MEGA GLITCH e i filtri COMB sono le parti di TONE più pesanti. Delay e riverbero si spengono da soli quando non
ricevono niente. **Il carico reale sull'FM-1 non è ancora stato misurato.**
