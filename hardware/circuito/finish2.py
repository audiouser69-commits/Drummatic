import pcbnew
from route import import_ses, fill, DST, OUT
b = pcbnew.LoadBoard(f"{DST}.kicad_pcb")
for t in list(b.GetTracks()):
    b.Remove(t)
import_ses(b, OUT / "drummatic_controllo_2.ses")
fill(b)
pcbnew.SaveBoard(f"{DST}.kicad_pcb", b)
print("tracce:", len(b.GetTracks()))
