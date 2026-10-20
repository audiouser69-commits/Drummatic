"""DRC + file di produzione per Eurocircuits / Aisler: Gerber, foratura, BOM, CPL, immagini."""
import csv, json, subprocess, shutil, re
from collections import defaultdict
from pathlib import Path
import pcbnew

HERE = Path(__file__).parent
OUT = HERE / "out"
PCB = OUT / "drummatic_controllo.kicad_pcb"
FAB = OUT / "produzione"
OFF = 20.0


def drc():
    b = pcbnew.LoadBoard(str(PCB))
    rep = OUT / "drc.txt"
    pcbnew.WriteDRCReport(b, str(rep), pcbnew.EDA_UNITS_MILLIMETRES, True)
    txt = rep.read_text()
    counts = {k: int(v) for v, k in re.findall(r"\*\* Found (\d+) (DRC violations|unconnected pads|Footprint errors)", txt)}
    return counts, txt


def run(*args):
    r = subprocess.run(["kicad-cli", *args], capture_output=True, text=True)
    if r.returncode:
        print(r.stdout, r.stderr)
    return r


def fab():
    if FAB.exists():
        shutil.rmtree(FAB)
    g = FAB / "gerber"; g.mkdir(parents=True)
    run("pcb", "export", "gerbers", "-o", str(g) + "/", "--layers",
        "F.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts", "--subtract-soldermask", str(PCB))
    run("pcb", "export", "drill", "-o", str(g) + "/", "--format", "excellon", "--excellon-separate-th",
        "--generate-map", "--map-format", "pdf", str(PCB))
    # posizionamento (CPL) e distinta (BOM) con codici produttore
    parts = {p["ref"]: p for p in json.loads((OUT / "parts.json").read_text())}
    b = pcbnew.LoadBoard(str(PCB))
    ox = b.GetDesignSettings().GetAuxOrigin()
    rows = []
    for fp in b.GetFootprints():
        ref = fp.GetReference()
        if ref.startswith("H"):
            continue
        p = parts[ref]
        pos = fp.GetPosition()
        side = "Bottom" if fp.IsFlipped() else "Top"
        rows.append(dict(Designator=ref, Value=fp.GetValue(), Footprint=p["fp"], MPN=p["mpn"], Supplier=p["supplier"],
                         MidX=round(pcbnew.ToMM(pos.x) - OFF, 3), MidY=round(-(pcbnew.ToMM(pos.y) - OFF), 3),
                         Layer=side, Rotation=round(fp.GetOrientationDegrees() % 360, 1),
                         Mount="THT" if p["fp"].startswith(("RotaryEncoder", "Potentiometer", "PinHeader")) else
                         ("Non montare (switch a pressione)" if False else "SMD")))
    run("pcb", "export", "pos", "-o", str(FAB / "CPL_posizionamento.csv"), "--format", "csv", "--units", "mm",
        "--side", "both", str(PCB))
    groups = defaultdict(list)
    for r in rows:
        v = r["Value"]
        if v.startswith("Choc"): v = "Presa hot-swap Choc v1"
        if v.startswith("EC11E"): v = "Encoder EC11E con pulsante"
        groups[(v, r["MPN"], r["Footprint"], r["Mount"], r["Layer"])].append(r["Designator"])
    def natkey(s):
        m = re.match(r"([A-Z]+)(\d+)", s); return (m.group(1), int(m.group(2))) if m else (s, 0)
    with open(FAB / "BOM_distinta.csv", "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Quantità", "Valore", "Codice produttore", "Footprint", "Montaggio", "Lato", "Designatori"])
        for (v, mpn, fpn, mount, side), refs in sorted(groups.items(), key=lambda t: natkey(sorted(t[1], key=natkey)[0])):
            refs = sorted(refs, key=natkey)
            w.writerow([len(refs), v, mpn, fpn, mount, side, " ".join(refs)])
    # immagini dei due lati
    for side, layers, mirror in (("superiore", "F.Cu,F.SilkS,F.Mask,Edge.Cuts", False),
                                 ("inferiore", "B.Cu,B.SilkS,B.Mask,Edge.Cuts", True)):
        args = ["pcb", "export", "svg", "-l", layers, "--exclude-drawing-sheet", "--page-size-mode", "2",
                "-o", str(OUT / f"lato_{side}.svg"), str(PCB)]
        if mirror:
            args.insert(3, "--mirror")
        run(*args)
    shutil.copy(PCB, FAB / PCB.name)
    shutil.copy(OUT / "drummatic_controllo.kicad_pro", FAB / "drummatic_controllo.kicad_pro")
    shutil.make_archive(str(OUT / "Drummatic-Circuito-Controllo-produzione"), "zip", FAB)
    return len(rows)


if __name__ == "__main__":
    counts, txt = drc()
    print("DRC:", counts)
    print("componenti piazzati:", fab())
