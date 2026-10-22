# Pacchetti pronti

| File | Contenuto |
| --- | --- |
| `Drummatic-0.2.4-Windows.zip` | Drummatic 0.2.4 per Windows 10/11 a 64 bit: `Drummatic.exe` (standalone), `Drummatic.vst3`, il manuale PDF |

Installazione: estrai lo zip, copia `Drummatic.vst3` in `C:\Program Files\Common Files\VST3\` e riesegui la scansione
in Ableton; `Drummatic.exe` si avvia da dove vuoi (SmartScreen: Ulteriori informazioni → Esegui comunque).

La versione macOS (universale, standalone + VST3) e una copia di quella Windows escono a ogni push dalla build
automatica: GitHub → Actions → Build → artefatti `Drummatic-macOS` e `Drummatic-Windows`. Creando su GitHub una release
con un tag `v…` (es. `v0.2.3`) la build vi allega i due zip da sola.

Il firmware per l'FM-1 e il suo manuale sono in `fm1/` (`fm1/docs/Drummatic-FM1-Manuale.pdf`).
