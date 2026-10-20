"""Importa il risultato di Freerouting nel circuito con i piani di massa e lo salva."""
import sys
import pcbnew
from route import import_ses, fill, DST, OUT
b = pcbnew.LoadBoard(f"{DST}.kicad_pcb")
two = pcbnew.LSET(); two.AddLayer(pcbnew.F_Cu); two.AddLayer(pcbnew.B_Cu)
for z in b.Zones():
    if z.GetIsRuleArea() and z.GetLayerSet().Contains(pcbnew.In1_Cu):
        z.SetLayerSet(two)
import_ses(b, OUT / (sys.argv[1] if len(sys.argv) > 1 else "drummatic_controllo.ses"))
fill(b)
pcbnew.SaveBoard(f"{DST}.kicad_pcb", b)
print("tracce:", len(b.GetTracks()))
