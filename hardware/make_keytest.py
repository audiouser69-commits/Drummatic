"""Set di prova: 4 tasti con piedini Choc 1,15 / 1,20 / 1,25 / 1,30 mm (1-4 tacche sul bordo)."""
import cadquery as cq
from geom import *
h = keycap_height()
parts = []
for i, lw in enumerate((1.15, 1.20, 1.25, 1.30)):
    cap = cq.Workplane("XY").box(KEYCAP, KEYCAP, h, centered=(True, True, False)).edges("|Z").fillet(1.2)
    cap = cap.faces(">Z").edges().chamfer(0.5)
    cap = cap.cut(cq.Workplane("XY").sphere(60).translate((0, 0, h + 60 - 0.35)))
    for x in (-2.85, 2.85):
        cap = cap.union(cq.Workplane("XY").box(lw, 3.0, 3.0, centered=(True, True, False)).translate((x, 0, -3.0)))
    for k in range(i + 1):
        cap = cap.cut(cq.Workplane("XY").box(0.8, 1.2, 1.2).translate((-6 + 1.6 * k, -KEYCAP / 2, h - 0.6)))
    parts.append(cap.translate((i * 20, 0, 0)).val())
cq.exporters.export(cq.Workplane().add(cq.Compound.makeCompound(parts)), "out/tasti_prova_choc.stl", tolerance=0.02, angularTolerance=0.1)
print("set di prova, altezza tasto", h)
