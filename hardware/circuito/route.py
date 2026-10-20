"""Classi di rete, riempimento piani, esportazione DSN, Freerouting, importazione SES, DRC."""
import json, shutil, subprocess, sys, time
from pathlib import Path
import pcbnew

OUT = Path(__file__).parent / "out"
SRC = OUT / "drummatic_controllo_unrouted"
DST = OUT / "drummatic_controllo"
PASSES = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 60


def setup_project():
    shutil.copy(f"{SRC}.kicad_pcb", f"{DST}.kicad_pcb")
    pro = json.load(open(f"{SRC}.kicad_pro"))
    ns = pro["net_settings"]
    d = ns["classes"][0]
    d.update(track_width=0.25, clearance=0.2, via_diameter=0.7, via_drill=0.35)
    pw = dict(d); pw.update(name="Power", track_width=0.6, clearance=0.25, via_diameter=0.9, via_drill=0.45)
    p3 = dict(d); p3.update(name="Supply3V3", track_width=0.4, clearance=0.2, via_diameter=0.8, via_drill=0.4)
    ns["classes"] = [d, pw, p3]
    gn = dict(d); gn.update(name="Ground", track_width=0.4, clearance=0.2, via_diameter=0.8, via_drill=0.4)
    ns["classes"].append(gn)
    ns["netclass_patterns"] = [{"netclass": "Power", "pattern": p} for p in ("+5V", "VBUS")] + \
                              [{"netclass": "Ground", "pattern": p} for p in ("GND", "AGND")] + \
                              [{"netclass": "Supply3V3", "pattern": "+3V3"}]
    # regole di fabbricazione (classe 6 Eurocircuits: 0,15 mm; restiamo larghi)
    rules = pro["board"]["design_settings"]["rules"]
    rules.update(min_clearance=0.2, min_track_width=0.2, min_via_diameter=0.6, min_through_hole_diameter=0.3,
                 min_hole_to_hole=0.25, min_copper_edge_clearance=0.3, min_via_annular_width=0.13)
    sev = pro["board"]["design_settings"].setdefault("rule_severities", {})
    sev.update(courtyards_overlap="ignore", silk_overlap="ignore", silk_over_copper="ignore", silk_edge_clearance="ignore",
               lib_footprint_issues="ignore", lib_footprint_mismatch="ignore", text_height="ignore",
               text_thickness="ignore")
    json.dump(pro, open(f"{DST}.kicad_pro", "w"), indent=2)


def sexp(text):
    """parser S-expression minimale"""
    import re
    toks = re.findall(r'"[^"]*"|\(|\)|[^\s()]+', text)
    stack = [[]]
    for t in toks:
        if t == "(":
            stack.append([])
        elif t == ")":
            x = stack.pop(); stack[-1].append(x)
        else:
            stack[-1].append(t.strip('"'))
    return stack[0][0]


def find(node, key):
    return [c for c in node if isinstance(c, list) and c and c[0] == key]


def import_ses(board, path):
    tree = sexp(Path(path).read_text())
    routes = find(tree, "routes")[0]
    res = find(routes, "resolution")[0]           # ("resolution", "um", "10")
    scale = {"um": 1000, "mil": 25400, "mm": 1000000, "inch": 25400000}[res[1]] / float(res[2])   # unità -> nm
    nets = {n.GetNetname(): n for n in board.GetNetsByName().values()} if False else None
    netmap = {}
    for code, ni in board.GetNetInfo().NetsByNetcode().items():
        netmap[ni.GetNetname()] = ni
    layers = {"F.Cu": pcbnew.F_Cu, "B.Cu": pcbnew.B_Cu}
    nw, nv = 0, 0
    for netnode in find(find(routes, "network_out")[0], "net"):
        ni = netmap.get(netnode[1])
        for w in find(netnode, "wire"):
            p = find(w, "path")[0]
            layer = layers[p[1]]; width = int(float(p[2]) * scale)
            co = [float(v) for v in p[3:] if not isinstance(v, list)]
            pts = [(int(co[i] * scale), int(-co[i + 1] * scale)) for i in range(0, len(co) - 1, 2)]
            for a, c in zip(pts, pts[1:]):
                t = pcbnew.PCB_TRACK(board)
                t.SetStart(pcbnew.VECTOR2I(*a)); t.SetEnd(pcbnew.VECTOR2I(*c))
                t.SetWidth(width); t.SetLayer(layer)
                if ni: t.SetNet(ni)
                board.Add(t); nw += 1
        for v in find(netnode, "via"):
            import re
            m = re.search(r"(\d+):(\d+)_um", v[1])
            dia, drill = (int(m.group(1)) * 1000, int(m.group(2)) * 1000) if m else (700000, 350000)
            via = pcbnew.PCB_VIA(board)
            via.SetPosition(pcbnew.VECTOR2I(int(float(v[2]) * scale), int(-float(v[3]) * scale)))
            via.SetWidth(dia); via.SetDrill(drill); via.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
            if ni: via.SetNet(ni)
            board.Add(via); nv += 1
    print("importati", nw, "segmenti e", nv, "via")


def fill(board):
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())


if __name__ == "__main__":
    setup_project()
    b = pcbnew.LoadBoard(f"{DST}.kicad_pcb")
    for z in list(b.Zones()):            # niente piani nel DSN: la massa viene sbrogliata come rete normale
        if not z.GetIsRuleArea():
            b.Remove(z)
    dsn = OUT / "drummatic_controllo.dsn"; ses = OUT / "drummatic_controllo.ses"
    assert pcbnew.ExportSpecctraDSN(b, str(dsn))
    t = time.time()
    with open(OUT / "freerouting.log", "w") as lf:
        r = subprocess.run(["xvfb-run", "-a", "java", "-jar", str(Path(__file__).parent / "tools/fr19.jar"), "-de", str(dsn),
                            "-do", str(ses), "-mp", str(PASSES), "-mt", "1"], stdout=lf, stderr=subprocess.STDOUT)
    log = (OUT / "freerouting.log").read_text()
    print("freerouting", r.returncode, round(time.time() - t), "s")
    print("\n".join(l for l in log.splitlines() if "unrouted" in l.lower() or "ERROR" in l)[-1500:])
    subprocess.run([sys.executable, str(Path(__file__).parent / "finish.py")], check=True)
