"""Seconda passata di Freerouting partendo dal circuito già sbrogliato (completa le connessioni mancanti)."""
import subprocess, sys, time
from pathlib import Path
import pcbnew
from route import import_ses, fill, DST, OUT
b = pcbnew.LoadBoard(f"{DST}.kicad_pcb")
for z in list(b.Zones()):
    if not z.GetIsRuleArea():
        b.Remove(z)
dsn = OUT / "drummatic_controllo_2.dsn"; ses = OUT / "drummatic_controllo_2.ses"
assert pcbnew.ExportSpecctraDSN(b, str(dsn))
with open(OUT / "freerouting2.log", "w") as lf:
    subprocess.run(["xvfb-run", "-a", "java", "-jar", str(Path(__file__).parent / "tools/fr19.jar"), "-de", str(dsn),
                    "-do", str(ses), "-mp", "10", "-mt", "1"], stdout=lf, stderr=subprocess.STDOUT)
print("ok", ses.exists())
