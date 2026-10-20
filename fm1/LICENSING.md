# Drummatic FM-1: licenze

Tutto ciò che sta in `fm1/` è software libero sotto la **GNU General Public License, versione 3 soltanto**
(`GPL-3.0-only`, testo completo in `LICENSE`), salvo le parti elencate sotto, i cui testi di licenza sono in `LICENSES/`.

- **La piattaforma FM-1** (`firmware/hal/`, `firmware/loader/`, `firmware/src/platform/`, `firmware/crt0.S`,
  `firmware/app.ld`, gli strumenti di build e d'installazione in `tools/`, la base della pagina dell'emulatore) viene
  da Felucca: Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments, GPL-3.0-only; attraverso MAKU
  (okumurari) e l'Autechre Machine Plus.
- **Il motore audio** (`firmware/src/dm/`) e **l'applicazione per l'FM-1** (`firmware/src/app/`, `web/dm_web.c`) sono il
  port di Drummatic per l'FM-1, GPL-3.0-only come il firmware che ne risulta.
- **Il pacchetto `.fwsc`** contiene tre file dell'SDK JieLi AC79 (`uboot.boot`, `cfg_tool.bin`, `eq_cfg_hw.bin`):
  Apache-2.0 (`LICENSES/Apache-2.0.txt`).
- **Font**: Inter Tight (interfaccia dell'FM-1, `assets/fonts/`), SIL OFL 1.1 (`LICENSES/OFL-InterTight.txt`);
  DotGothic16 (etichette dell'emulatore, `web/fonts/`), SIL OFL 1.1 (`LICENSES/OFL-DotGothic16.txt`).

Chi distribuisce il firmware, o un firmware derivato, deve dare anche il sorgente completo corrispondente con la stessa
licenza. M-VAVE e FM-1 sono marchi dei rispettivi proprietari; questo firmware non è affiliato né approvato da loro.
