"""Collegamento manuale ROW4 (pin 6 del Pico) che il router ha lasciato aperto."""
import sys, pcbnew
from route import fill, DST
b = pcbnew.LoadBoard(f"{DST}.kicad_pcb")
net = b.FindNet("ROW4")
pts = [tuple(map(float, p.split(","))) for p in sys.argv[1:]]   # x,y,layer(F/B) ; ultimo punto: via se cambia lato
mm = pcbnew.FromMM
for (x0, y0, l0), (x1, y1, l1) in zip(pts, pts[1:]):
    t = pcbnew.PCB_TRACK(b); t.SetStart(pcbnew.VECTOR2I(mm(x0), mm(y0))); t.SetEnd(pcbnew.VECTOR2I(mm(x1), mm(y1)))
    t.SetWidth(mm(0.25)); t.SetLayer(pcbnew.F_Cu if l0 == 0 else pcbnew.B_Cu); t.SetNet(net); b.Add(t)
    if l1 != l0:
        v = pcbnew.PCB_VIA(b); v.SetPosition(pcbnew.VECTOR2I(mm(x1), mm(y1))); v.SetWidth(mm(0.7)); v.SetDrill(mm(0.35))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu); v.SetNet(net); b.Add(v)
fill(b)
pcbnew.SaveBoard(f"{DST}.kicad_pcb", b)
