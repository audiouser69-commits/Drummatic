# Compilare Drummatic per l'FM-1

Tutto da `fm1/`. Il build fa in `build/`:

| File | Cosa |
| --- | --- |
| `drummatic.bin` | il firmware (l'app) |
| `loader/ota.bin` | il loader di aggiornamento di Felucca |
| `drummatic.fwsc` | il pacchetto installabile (app + loader) |

## Cosa serve

- Python 3 con Pillow e fontTools: `pip3 install Pillow fonttools` (font e palette dell'interfaccia sono generati).
- La toolchain JieLi per Linux (clang 4.0.1 per pi32v2): `tools/get_toolchain.sh` (in `~/.jieli/toolchain`). Gira solo
  su Linux x86-64; su macOS / Apple silicon `tools/build.py` usa Docker (`linux/amd64 debian:bookworm-slim`).
- L'SDK JieLi AC79 (Apache-2.0), da cui il pacchetto prende tre file (`uboot.boot`, `cfg_tool.bin`,
  `cfg/eq_cfg_hw.bin`):
  `git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git ~/fw-AC79_AIoT_SDK`
- Per l'emulatore: `clang` con il target `wasm32` e `wasm-ld` (LLVM 15 o più recente). Per i test: un compilatore C/C++
  dell'host, Node.js, facoltativo valgrind.

## Build

```
JIELI_TOOLCHAIN=~/.jieli/toolchain AC79_SDK=~/fw-AC79_AIoT_SDK python3 tools/build.py
JIELI_TOOLCHAIN=... AC79_SDK=... python3 tools/build.py --release 0.1     # build/drummatic-fm1-0.1.fwsc + build/release-0.1/
```

Il firmware è un'unica unità di compilazione, `firmware/src/drummatic.c`, a `-O2` con l'unità in virgola mobile del
processore (`-mfprev1`): il motore audio è in `float`. Il build controlla l'immagine, la RAM (.data+.bss ≤ 96 KB), il
pool (≤ 336 KB con 8 KB liberi) e che i registri dell'hardware si tocchino solo in `firmware/hal/`.

Opzioni (variabile d'ambiente, `0` o `1`): `FELUCCA_UAC` (ingresso audio USB, 1), `FELUCCA_UAC_48K` (anche a 48 kHz, 1),
`FELUCCA_UART` (MIDI TRS, 1), `FELUCCA_OTA` (aggiornamento dal firmware in funzione, 1), `FELUCCA_SIZE=1` (`-Os`).

## Struttura

```
firmware/
  hal/            l'hardware dell'FM-1 (Felucca): tempo, interruzioni, guardie, tasti e LED, schermo, audio, ADC, flash, USB
  loader/         il loader di aggiornamento (Felucca)
  src/drummatic.c l'unità di compilazione
  src/dm/         il motore: dm_math (sin, exp, tanh senza libm), dm_engines (25 motori), dm_voice (voce, TONE, AHD, LFO,
                  modulazione), dm_fx (delay, riverbero, master), dm_kit (kit, DADO, Performance, formato flash),
                  dm_machine (sequencer, MIDI, mix)
  src/platform/   da Felucca: libc, lcd, gfx (testo antialias), usb (MIDI, audio, aggiornamento), midi_uart, storage_hw,
                  ota
  src/app/        Drummatic sull'FM-1: dm_audio (ISR audio), dm_store (kit nella flash), dm_params (le pagine), dm_ui
                  (pannello, LED, schermo), dm_panel, dm_main (avvio, ciclo principale)
web/              l'emulatore: dm_web.c (FM-1 simulato), index.html, worklet.js, build.sh, emu_test.mjs
tests/            dm_compare.cpp (contro ../Source/DSP), dm_cpu.c + cpu_measure.sh (costo), run_tests.sh
tools/            build.py, get_toolchain.sh, fm1_install.py (installatore), dm_flasher.py (Flasher Windows), generatori
```

## Test

```
sh web/build.sh          # prima: l'emulatore
sh tests/run_tests.sh    # CPU=1 aggiunge il costo per campione (valgrind)
```

## Memoria flash

| Area | Contenuto |
| --- | --- |
| 0x97000 – 0xD6FFF | 16 kit, due copie A/B da 8 KB ciascuno |
| 0xD7000 – 0xDEFFF | 16 preset di voce, 4 banchi A/B |
| 0xE5000 – 0xE6FFF | l'autosave |
| 0xFC000 – 0xFDFFF | le impostazioni A/B |

Ogni oggetto ha un record di commit (magia, tipo, sequenza, lunghezza, CRC) scritto per ultimo: un salvataggio
interrotto lascia la copia precedente.
