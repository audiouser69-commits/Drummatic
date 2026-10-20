"""Modelli 3D (STEP/STL) del contenitore Drummatic + controllo ingombri.
Pannello: costruito nel piano (u, v, w) e poi ruotato della pendenza attorno a X e alzato di CHEEK_H_FRONT.
"""
import math, json
from pathlib import Path
import cadquery as cq
from shapely.geometry import Polygon
from shapely.ops import unary_union
from geom import *
from make_2d import text_polys

OUT = Path(__file__).parent / "out"
STL = Path(__file__).parent / "render" / "stl"
OUT.mkdir(exist_ok=True); STL.mkdir(parents=True, exist_ok=True)
DEG = math.degrees(SLOPE)


def to_global(shape):
    """da coordinate pannello (u, v, w) a globali"""
    return shape.rotate((0, 0, 0), (1, 0, 0), DEG).translate((0, 0, CHEEK_H_FRONT))


def P(xy):
    return L2P(*xy)


# ---------------------------------------------------------------- testo inciso
def text_solid(s, x, y, cap, depth, anchor="middle", tracking=0.0):
    polys, _ = text_polys(s, x, y, cap, anchor, tracking)
    geom = None
    for p in polys:                      # pari-dispari: i contorni interni diventano fori
        g = Polygon(p).buffer(0)
        geom = g if geom is None else geom.symmetric_difference(g)
    solids = []
    parts = getattr(geom, "geoms", [geom])
    for poly in parts:
        outer = cq.Wire.makePolygon([cq.Vector(a, b, 0) for a, b in poly.exterior.coords[:-1]], close=True)
        inners = [cq.Wire.makePolygon([cq.Vector(a, b, 0) for a, b in r.coords[:-1]], close=True) for r in poly.interiors]
        f = cq.Face.makeFromWires(outer, inners)
        solids.append(cq.Solid.extrudeLinear(f, cq.Vector(0, 0, depth)))
    return cq.Compound.makeCompound(solids)


# ---------------------------------------------------------------- pannello
def panel(engrave=True):
    wp = cq.Workplane("XY").box(W, PANEL_L, PANEL_T, centered=False).edges("|Z").fillet(3.0)
    cuts = []
    cx, cy = P(DISPLAY_CENTER)
    cuts.append(cq.Workplane("XY").center(cx, cy).rect(*DISPLAY_WINDOW).extrude(PANEL_T).edges("|Z").fillet(2.0))
    for e in ENCODERS + [DATA]:
        u, v = P(e); cuts.append(cq.Workplane("XY").center(u, v).circle(ENC_HOLE / 2).extrude(PANEL_T))
    u, v = P(VOLUME); cuts.append(cq.Workplane("XY").center(u, v).circle(POT_HOLE / 2).extrude(PANEL_T))
    for k in ALL_KEYS:
        u, v = P(k)
        cuts.append(cq.Workplane("XY").center(u, v).rect(KEY_HOLE, KEY_HOLE).extrude(PANEL_T).edges("|Z").fillet(1.5))
    screws = [(uu, vv) for vv in PANEL_SIDE_SCREWS_V for uu in (CHEEK_T / 2, W - CHEEK_T / 2)]
    screws += [(uu, PANEL_EDGE_SCREW_INSET) for uu in PANEL_FRONT_SCREWS_U]
    screws += [(uu, PANEL_L - PANEL_EDGE_SCREW_INSET) for uu in PANEL_REAR_SCREWS_U]
    body = wp
    for c in cuts:
        body = body.cut(c)
    body = body.faces(">Z").workplane(origin=(0, 0, PANEL_T)).pushPoints(screws).cskHole(3.4, 6.4, 90)
    # prigionieri M3 x 8 pressati sul retro (PEM FHS-M3-8): modellati come cilindri
    studs = cq.Workplane("XY")
    for s in PCB_STUDS + DISPLAY_STUDS:
        u, v = P(s)
        studs = studs.union(cq.Workplane("XY").center(u, v).circle(1.5).extrude(-8.0))
    body = body.union(studs)
    if engrave:
        lu, lv = P((LOGO_RIGHT, LOGO_BASE))
        t = text_solid("DRUMMATIC", lu, lv, LOGO_CAP, 0.3, "end", 1.2).translate((0, 0, PANEL_T - 0.3))
        body = body.cut(cq.Workplane().add(t))
    return body


# ---------------------------------------------------------------- fianchi
def cheek(left=True):
    pts = [(0, 0), (D, 0), (D, CHEEK_H_REAR), (0, CHEEK_H_FRONT)]
    prof = cq.Workplane("YZ").polyline(pts).close().extrude(CHEEK_T)
    prof = prof.edges("|X").fillet(2.0)
    # fori filettati M3 sul bordo superiore, perpendicolari al pannello
    for v in PANEL_SIDE_SCREWS_V:
        y = v * math.cos(SLOPE); z = CHEEK_H_FRONT + v * math.sin(SLOPE)
        drill = (cq.Workplane("XY").circle(1.25).extrude(-10).rotate((0, 0, 0), (1, 0, 0), DEG)
                 .translate((CHEEK_T / 2, y, z)))
        prof = prof.cut(drill)
    # fori svasati laterali verso le pareti del vassoio (dadi PEM nel bordo del vassoio)
    face = "<X" if left else ">X"
    pts_side = [(y, 6.0) for y in CHEEK_SIDE_SCREWS_Y]
    for y, z in pts_side:
        hole = cq.Workplane("YZ").center(y, z).circle(1.7).extrude(CHEEK_T)
        csk = cq.Solid.makeCone(3.2, 1.7, 1.5, cq.Vector(0 if left else CHEEK_T, y, z),
                                cq.Vector(1 if left else -1, 0, 0))
        prof = prof.cut(hole).cut(cq.Workplane().add(csk))
    if not left:
        prof = prof.translate((W - CHEEK_T, 0, 0))
    return prof


# ---------------------------------------------------------------- vassoio in lamiera
def tray():
    t = SHEET_T; h = t / 2
    fl_c = math.cos(SLOPE) * FLANGE; fl_s = math.sin(SLOPE) * FLANGE
    zf = CHEEK_H_FRONT - h; zr = CHEEK_H_REAR - h
    pts = [(h + fl_c, zf + fl_s), (h, zf), (h, h), (D - h, h), (D - h, zr), (D - h - fl_c, zr - fl_s)]
    w = cq.Wire.makePolygon([cq.Vector(0, y, z) for y, z in pts])
    w = w.fillet2D(2.0 + h, w.Vertices()[1:-1])
    loop = w.offset2D(h, "arc")[0]
    sol = cq.Solid.extrudeLinear(cq.Face.makeFromWires(loop), cq.Vector(INNER_W, 0, 0)).translate((TRAY_X0, 0, 0))
    body = cq.Workplane().add(sol)
    # bordi laterali piegati dal fondo (avvitati ai fianchi)
    for x in (TRAY_X0, TRAY_X1 - t):
        body = body.union(cq.Workplane("XY").box(t, D - 2 * t - 4, SIDE_LIP_H, centered=False).translate((x, t + 2, 0)))
    # apertura posteriore per la piastra connettori
    body = body.cut(cq.Workplane("XY").box(REAR_OPEN_W, 10, REAR_OPEN_H)
                    .translate((W / 2, D, REAR_OPEN_Z + REAR_OPEN_H / 2)))
    # fori per le viti della piastra posteriore (dadi PEM M3 all'interno)
    for (x, z) in rear_plate_screws_global():
        body = body.cut(cq.Workplane("XZ").center(x, z).circle(2.1).extrude(-10).translate((0, D - 5, 0)))
    # asole di ventilazione sul fondo
    for (x, y) in VENT_SLOTS:
        body = body.cut(cq.Workplane("XY").center(x + TRAY_X0, y).slot2D(16, 3).extrude(10).translate((0, 0, -2)))
    # fori dadi PEM nei bordi laterali (viti dai fianchi)
    for x in (TRAY_X0, TRAY_X1 - t):
        for y in CHEEK_SIDE_SCREWS_Y:
            body = body.cut(cq.Workplane("YZ").center(y, 6.0).circle(2.1).extrude(t + 2).translate((x - 1, 0, 0)))
    # fori dadi PEM nelle alette per le viti del pannello
    for uu in PANEL_FRONT_SCREWS_U:
        body = body.cut(to_global(cq.Workplane("XY").center(uu, PANEL_EDGE_SCREW_INSET).circle(2.1).extrude(-6)))
    for uu in PANEL_REAR_SCREWS_U:
        body = body.cut(to_global(cq.Workplane("XY").center(uu, PANEL_L - PANEL_EDGE_SCREW_INSET).circle(2.1).extrude(-6)))
    # distanziali PEM per il Raspberry Pi (SO-M2.5-5) e per i piedini
    for hx, hy in PI_HOLES:
        body = body.union(cq.Workplane("XY").center(PI_X0 + hx, PI_Y0 + hy).circle(2.1).circle(1.1)
                          .extrude(PI_STANDOFF).translate((0, 0, t)))
    for fx, fy in FEET:
        body = body.cut(cq.Workplane("XY").center(fx, fy).circle(1.7).extrude(10).translate((0, 0, -2)))
    return body


def rear_plate_screws_global():
    """centri delle 6 viti della piastra posteriore in (X, Z)"""
    x0 = W / 2 + REAR_PLATE_W / 2           # u = 0 della piastra (vista da dietro) = lato destro in X
    out = []
    for u in (8, REAR_PLATE_W / 2, REAR_PLATE_W - 8):
        for v in (4.5, REAR_PLATE_H - 4.5):
            out.append((x0 - u, REAR_PLATE_Z + v))
    return out


def rear_plate():
    x0 = W / 2 + REAR_PLATE_W / 2
    pl = (cq.Workplane("XZ").center(W / 2, REAR_PLATE_Z + REAR_PLATE_H / 2).rect(REAR_PLATE_W, REAR_PLATE_H)
          .extrude(-2.0).edges("|Y").fillet(2.0).translate((0, D, 0)))
    for name, u in REAR_CONNECTORS:
        x, z = x0 - u, REAR_PLATE_Z + REAR_CONN_V
        pl = pl.cut(cq.Workplane("XZ").center(x, z).circle(12).extrude(-5).translate((0, D - 1, 0)))
        # corpo del passante formato D (Neutrik NA* / Switchcraft EH*): flangia 26 x 31, corpo Ø24 x 22 all'interno
    for (x, z) in rear_plate_screws_global():
        pl = pl.cut(cq.Workplane("XZ").center(x, z).circle(1.7).extrude(-5).translate((0, D - 1, 0)))
    fx = x0 - REAR_FAN_U; fz = REAR_PLATE_Z + REAR_CONN_V
    pl = pl.cut(cq.Workplane("XZ").center(fx, fz).circle(16.5).extrude(-5).translate((0, D - 1, 0)))
    return pl


def connectors():
    """passanti formato D sulla piastra: flangia + corpo verso l'interno (ingombro)"""
    x0 = W / 2 + REAR_PLATE_W / 2
    c = cq.Workplane("XY")
    for name, u in REAR_CONNECTORS:
        x, z = x0 - u, REAR_PLATE_Z + REAR_CONN_V
        c = c.union(cq.Workplane("XZ").center(x, z).rect(26, 31).extrude(-3).translate((0, D + 2, 0)))
        c = c.union(cq.Workplane("XZ").center(x, z).circle(12).extrude(22).translate((0, D, 0)))
    fx = x0 - REAR_FAN_U
    c = c.union(cq.Workplane("XY").box(40, 10, 40).translate((fx, D - 5, REAR_PLATE_Z + REAR_CONN_V)))
    return c


# ---------------------------------------------------------------- staffa display
def bracket():
    bx0, by0, bx1, by1 = 30.0, 8.0, 200.0, 120.0
    (u0, v1), (u1, v0) = P((bx0, by0)), P((bx1, by1))
    w0 = -(DISPLAY_DEPTH + 2.0)
    b = cq.Workplane("XY").box(u1 - u0, v1 - v0, 2.0, centered=False).translate((u0, v0, w0)).edges("|Z").fillet(3.0)
    for s in DISPLAY_STUDS:
        u, v = P(s); b = b.cut(cq.Workplane("XY").center(u, v).circle(4.5).extrude(10).translate((0, 0, w0 - 1)))
    cx, cy = P(DISPLAY_CENTER)
    b = b.cut(cq.Workplane("XY").center(cx, cy).rect(90, 40).extrude(10).translate((0, 0, w0 - 1)))
    # distanziali M3 F/F lunghi DISPLAY_DEPTH fra prigioniero e staffa
    for s in DISPLAY_STUDS:
        u, v = P(s)
        b = b.union(cq.Workplane("XY").polygon(6, 5.5 / math.cos(math.pi / 6)).extrude(-DISPLAY_DEPTH).translate((u, v, 0)))
    return b


# ---------------------------------------------------------------- tasto (STL da stampare)
def keycap():
    h = keycap_height()
    cap = cq.Workplane("XY").box(KEYCAP, KEYCAP, h, centered=(True, True, False)).edges("|Z").fillet(1.2)
    cap = cap.faces(">Z").edges().chamfer(0.5)
    # leggera concavità per il dito
    cap = cap.cut(cq.Workplane("XY").sphere(60).translate((0, 0, h + 60 - 0.35)))
    # piedini Choc v1 (da verificare col set di prova): 2 × 1,20 x 3,00, interasse 5,70, lunghezza 3,0
    for x in (-2.85, 2.85):
        cap = cap.union(cq.Workplane("XY").box(1.2, 3.0, 3.0, centered=(True, True, False)).translate((x, 0, -3.0)))
    return cap


# ---------------------------------------------------------------- elementi fittizi per l'ingombro
def dummies():
    d = {}
    # circuito di controllo
    x0, y0, x1, y1 = PCB_RECT; nx0, ny0, nx1, ny1 = PCB_NOTCH
    (u0, v1), (u1, v0) = P((x0, y0)), P((x1, y1))
    pcb = cq.Workplane("XY").box(u1 - u0, v1 - v0, PCB_T, centered=False).translate((u0, v0, -PCB_GAP - PCB_T))
    (nu0, nv1), (nu1, nv0) = P((nx0, ny0)), P((nx1, ny1))
    pcb = pcb.cut(cq.Workplane("XY").box(nu1 - nu0, nv1 - nv0 + 5, 10, centered=False).translate((nu0, nv0, -15)))
    d["pcb"] = pcb
    # interruttori, prese hot-swap, tasti
    sw = cq.Workplane("XY"); caps = cq.Workplane("XY")
    kc = keycap(); stem_top = -PCB_GAP + SWITCH_STEM_TOP
    for k in ALL_KEYS:
        u, v = P(k)
        sw = sw.union(cq.Workplane("XY").box(15, 15, 5.0, centered=(True, True, False)).translate((u, v, -PCB_GAP)))
        sw = sw.union(cq.Workplane("XY").box(13, 6, 1.8, centered=(True, True, False)).translate((u, v - 3, -PCB_GAP - PCB_T - 1.8)))
        caps = caps.union(kc.translate((u, v, stem_top)))
    d["switch"] = sw; d["keycaps"] = caps
    # encoder EC11E e manopole
    enc = cq.Workplane("XY"); knobs = cq.Workplane("XY")
    for e in ENCODERS + [DATA, VOLUME]:
        u, v = P(e)
        enc = enc.union(cq.Workplane("XY").box(12, 13, 6.5, centered=(True, True, False)).translate((u, v, -PCB_GAP)))
        enc = enc.union(cq.Workplane("XY").circle(3.5).extrude(4.5).translate((u, v, -PCB_GAP + 6.5)))
        enc = enc.union(cq.Workplane("XY").circle(3.0).extrude(ENC_SHAFT - 11).translate((u, v, -PCB_GAP + 11)))
        r = 9.0 if e == DATA else (6.5 if e == VOLUME else 7.5)
        knobs = knobs.union(cq.Workplane("XY").circle(r).workplane(offset=14).circle(r - 1.2).loft()
                            .translate((u, v, PANEL_T + 0.8)).edges(">Z").fillet(1.0))
    d["encoder"] = enc; d["knobs"] = knobs
    # display
    cx, cy = P(DISPLAY_CENTER)
    d["display"] = cq.Workplane("XY").box(*DISPLAY_OUTLINE, DISPLAY_DEPTH).translate((cx, cy, -DISPLAY_DEPTH / 2))
    d["screen"] = cq.Workplane("XY").box(*DISPLAY_ACTIVE, 0.2).translate((cx, cy, -0.1 - 0.05))
    # Raspberry Pi 5 + dissipatore attivo + Pisound (in coordinate globali)
    z0 = SHEET_T + PI_STANDOFF
    pi = cq.Workplane("XY").box(PI_W, PI_L, 1.6, centered=False).translate((PI_X0, PI_Y0, z0))
    pi = pi.union(cq.Workplane("XY").box(PI_W, PI_L, PI_STACK_H - 1.6, centered=False)
                  .translate((PI_X0, PI_Y0, z0 + 1.6)).edges("|Z").fillet(2))
    d["pi"] = pi
    return d


# ---------------------------------------------------------------- controllo ingombri
def underside_w_at(y_global):
    """quota Z del lato inferiore del pannello sopra la coordinata Y"""
    return CHEEK_H_FRONT + y_global * math.tan(SLOPE)


def clearance_report():
    rep = {}
    # Pi: punto più alto contro il punto più basso del circuito sopra di esso (lato prese hot-swap)
    pi_top = SHEET_T + PI_STANDOFF + PI_STACK_H
    y_front = PI_Y0                         # dove il pannello è più basso
    pcb_bottom = underside_w_at(y_front) - (PCB_GAP + PCB_T + 1.8) / math.cos(SLOPE)
    rep["pi_vs_pcb_mm"] = round(pcb_bottom - pi_top, 1)
    # display + staffa contro il fondo
    cx, cy = P(DISPLAY_CENTER)
    v_front = cy - DISPLAY_OUTLINE[1] / 2
    low = CHEEK_H_FRONT + v_front * math.sin(SLOPE) - (DISPLAY_DEPTH + 2) * math.cos(SLOPE)
    rep["staffa_display_vs_fondo_mm"] = round(low - SHEET_T, 1)
    # parte bassa del circuito davanti (tasti trig) contro il fondo
    u, v = P(TRIG_KEYS[0])
    low = CHEEK_H_FRONT + (v - 8) * math.sin(SLOPE) - (PCB_GAP + PCB_T + 1.8 + 3) * math.cos(SLOPE)
    rep["circuito_davanti_vs_fondo_mm"] = round(low - SHEET_T, 1)
    # connettori posteriori contro il Pi (in Y)
    rep["connettori_vs_pi_mm"] = round((D - 22) - (PI_Y0 + PI_L + 12), 1)   # 12 mm = spine USB/Pisound sporgenti
    rep["sporgenza_tasto_mm"] = KEY_PROUD
    rep["sporgenza_albero_encoder_mm"] = round(ENC_SHAFT - PCB_GAP - PANEL_T, 1)
    rep["altezza_tasto_mm"] = keycap_height()
    rep["pendenza_gradi"] = round(DEG, 2)
    return rep


def export(name, wp, step=True, stl=True, tol=0.1):
    shape = wp.val() if isinstance(wp, cq.Workplane) and len(wp.vals()) == 1 else cq.Compound.makeCompound(
        [v for v in wp.vals()]) if isinstance(wp, cq.Workplane) else wp
    if step:
        cq.exporters.export(cq.Workplane().add(shape), str(OUT / f"{name}.step"))
    if stl:
        cq.exporters.export(cq.Workplane().add(shape), str(STL / f"{name}.stl"), tolerance=tol, angularTolerance=0.2)


if __name__ == "__main__":
    import sys
    parts = {}
    pnl = panel(); export("pannello_superiore_locale", pnl, stl=False)
    parts["pannello"] = to_global(pnl)
    parts["fianco_sx"] = cheek(True); parts["fianco_dx"] = cheek(False)
    parts["vassoio"] = tray(); parts["piastra_posteriore"] = rear_plate()
    br = bracket(); export("staffa_display_locale", br, stl=False); parts["staffa"] = to_global(br)
    kc = keycap(); export("tasto_choc", kc, step=True, stl=True, tol=0.02)
    cq.exporters.export(kc, str(OUT / "tasto_choc.stl"), tolerance=0.02, angularTolerance=0.1)
    for n in ("fianco_sx", "fianco_dx", "vassoio", "piastra_posteriore"):
        export(n, parts[n])
    export("pannello", parts["pannello"], step=False)
    export("staffa", parts["staffa"], step=False)
    export("connettori", connectors(), step=False)
    dm = {}
    for n, s in dummies().items():
        g = s if n == "pi" else to_global(s)
        dm[n] = g
        export(n, g, step=False, tol=0.3)
    # distanze minime reali fra i solidi
    def dist(a, b):
        return round(a.val().distance(b.val()) if len(a.vals()) == 1 else
                     cq.Compound.makeCompound(a.vals()).distance(cq.Compound.makeCompound(b.vals())), 1)
    con = connectors()
    real = {"pi_vs_interruttori_e_prese": dist(dm["pi"], dm["switch"]), "pi_vs_circuito": dist(dm["pi"], dm["pcb"]),
            "pi_vs_connettori_posteriori": dist(dm["pi"], con), "pi_vs_encoder": dist(dm["pi"], dm["encoder"]),
            "display_vs_vassoio": dist(dm["display"], parts["vassoio"]), "staffa_vs_vassoio": dist(parts["staffa"], parts["vassoio"]),
            "circuito_vs_vassoio": dist(dm["pcb"], parts["vassoio"]), "interruttori_vs_vassoio": dist(dm["switch"], parts["vassoio"]),
            "connettori_vs_vassoio": dist(con, parts["vassoio"].cut(rear_plate())),
            "encoder_vs_vassoio": dist(dm["encoder"], parts["vassoio"])}
    # assieme completo in STEP
    asm = cq.Assembly()
    for n, s in parts.items():
        asm.add(s, name=n)
    asm.save(str(OUT / "assieme_drummatic.step"))
    rep = clearance_report(); rep.pop("connettori_vs_pi_mm"); rep.pop("pi_vs_pcb_mm"); rep["distanze_minime_mm"] = real
    (OUT / "ingombri.json").write_text(json.dumps(rep, indent=2))
    print(json.dumps(rep, indent=2))


def engravings():
    """solo per il rendering: il fondo delle incisioni (alluminio chiaro sotto l'anodizzazione)"""
    sol = []
    lu, lv = P((LOGO_RIGHT, LOGO_BASE))
    sol.append(text_solid("DRUMMATIC", lu, lv, LOGO_CAP, 0.32, "end", 1.2))
    cap = 2.2
    labels = list(zip(PAGE_KEYS + FUNC_KEYS + TRANSPORT_KEYS, PAGE_NAMES + FUNC_NAMES + TRANSPORT_NAMES))
    labels += [(k, str(i + 1)) for i, k in enumerate(VOICE_KEYS)]
    labels += list(zip(ARROW_KEYS, ["<", "UP", "DOWN", ">"]))
    for k, name in labels:
        u, v = P(k); sol.append(text_solid(name, u, v - KEY_HOLE / 2 - 1.6 - cap, cap, 0.32))
    for i, k in enumerate(TRIG_KEYS):
        u, v = P(k); sol.append(text_solid(str(i + 1), u, v - KEY_HOLE / 2 - 1.4 - 1.8, 1.8, 0.32))
    for name, k in (("VOLUME", VOLUME), ("DATA", DATA)):
        u, v = P(k); sol.append(text_solid(name, u, v - 10.5 - cap, cap, 0.32))
    c = cq.Compound.makeCompound(sol).translate(cq.Vector(0, 0, PANEL_T - 0.3))
    return to_global(cq.Workplane().add(c))


def rear_engravings():
    x0 = W / 2 + REAR_PLATE_W / 2
    sol = [text_solid(n, u, REAR_CONN_V - 17.5 - 2.2, 2.2, 0.32) for n, u in REAR_CONNECTORS]
    sol.append(text_solid("FAN", REAR_FAN_U, REAR_CONN_V - 17.5 - 2.2, 2.2, 0.32))
    sol.append(text_solid("DRUMMATIC", REAR_PLATE_W - 18, REAR_PLATE_H - 11.5, 3.0, 0.32, "end", 0.5))
    c = cq.Compound.makeCompound(sol).rotate(cq.Vector(0, 0, 0), cq.Vector(1, 0, 0), 90)
    c = c.rotate(cq.Vector(0, 0, 0), cq.Vector(0, 0, 1), 180).translate(cq.Vector(x0, D + 2 - 0.3, REAR_PLATE_Z))
    return cq.Workplane().add(c)
