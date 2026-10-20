"""File 2D (DXF) per i fornitori: pannello superiore, piastra posteriore, staffa display, contorno circuito."""
import math
from pathlib import Path
import ezdxf
from matplotlib.textpath import TextPath
from matplotlib.font_manager import FontProperties
from geom import *

OUT = Path(__file__).parent / "out"
OUT.mkdir(exist_ok=True)
FONT = FontProperties(fname=str(Path(__file__).parent / "fonts/Michroma-Regular.ttf"))


def text_polys(s, x, y, cap, anchor="middle", tracking=0.0):
    """contorni di un testo in mm (poligoni chiusi), y verso l'alto; cap = altezza delle maiuscole"""
    tp0 = TextPath((0, 0), "H", size=100, prop=FONT)
    cap100 = tp0.get_extents().height
    size = 100 * cap / cap100
    polys, cursor = [], 0.0
    glyphs = []
    for ch in s:
        if ch == " ":
            cursor += size * 0.45 + tracking
            continue
        tp = TextPath((cursor, 0), ch, size=size, prop=FONT)
        ext = tp.get_extents()
        glyphs.append(tp)
        cursor = ext.x1 + size * 0.08 + tracking
    width = cursor - (size * 0.08 + tracking)
    dx = {"start": 0, "middle": -width / 2, "end": -width}[anchor]
    for tp in glyphs:
        for poly in tp.to_polygons(closed_only=True):
            polys.append([(px + x + dx, py + y) for px, py in poly])
    return polys, width


def add_text(msp, s, x, y, cap, layer, anchor="middle", tracking=0.0):
    polys, w = text_polys(s, x, y, cap, anchor, tracking)
    for p in polys:
        msp.add_lwpolyline(p, close=True, dxfattribs={"layer": layer})
    return w


def rrect(msp, cx, cy, w, h, r, layer):
    """rettangolo con angoli arrotondati come polilinea con bulge"""
    x0, y0, x1, y1 = cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2
    b = math.tan(math.pi / 8)
    pts = [(x0 + r, y0, 0), (x1 - r, y0, b), (x1, y0 + r, 0), (x1, y1 - r, b), (x1 - r, y1, 0), (x0 + r, y1, b),
           (x0, y1 - r, 0), (x0, y0 + r, b)]
    msp.add_lwpolyline([(p[0], p[1], 0, 0, p[2]) for p in pts], format="xyseb", close=True, dxfattribs={"layer": layer})


def new_doc():
    doc = ezdxf.new("R2010", setup=True)
    doc.units = ezdxf.units.MM
    for name, color in (("TAGLIO", 1), ("INCISIONE", 5), ("PRIGIONIERI_M3", 3), ("SVASATI", 6), ("RIFERIMENTI", 8)):
        doc.layers.add(name, color=color)
    return doc


# ======================= PANNELLO SUPERIORE =======================
def panel_dxf():
    doc = new_doc(); msp = doc.modelspace()
    # contorno
    rrect(msp, W / 2, PANEL_L / 2, W, PANEL_L, 3.0, "TAGLIO")
    P = lambda xy: L2P(*xy)
    # finestra display
    cx, cy = P(DISPLAY_CENTER)
    rrect(msp, cx, cy, DISPLAY_WINDOW[0], DISPLAY_WINDOW[1], 2.0, "TAGLIO")
    # encoder, DATA, volume
    for e in ENCODERS + [DATA]:
        u, v = P(e); msp.add_circle((u, v), ENC_HOLE / 2, dxfattribs={"layer": "TAGLIO"})
    u, v = P(VOLUME); msp.add_circle((u, v), POT_HOLE / 2, dxfattribs={"layer": "TAGLIO"})
    # tasti
    for k in ALL_KEYS:
        u, v = P(k); rrect(msp, u, v, KEY_HOLE, KEY_HOLE, 1.5, "TAGLIO")
    # viti di fissaggio svasate M3
    for vv in PANEL_SIDE_SCREWS_V:
        for uu in (CHEEK_T / 2, W - CHEEK_T / 2):
            msp.add_circle((uu, vv), 1.7, dxfattribs={"layer": "SVASATI"})
    for uu in PANEL_FRONT_SCREWS_U:
        msp.add_circle((uu, PANEL_EDGE_SCREW_INSET), 1.7, dxfattribs={"layer": "SVASATI"})
    for uu in PANEL_REAR_SCREWS_U:
        msp.add_circle((uu, PANEL_L - PANEL_EDGE_SCREW_INSET), 1.7, dxfattribs={"layer": "SVASATI"})
    # prigionieri filettati M3 sul retro (segnati con croce + cerchio)
    for s in PCB_STUDS + DISPLAY_STUDS:
        u, v = P(s)
        msp.add_circle((u, v), 2.5, dxfattribs={"layer": "PRIGIONIERI_M3"})
        msp.add_line((u - 3, v), (u + 3, v), dxfattribs={"layer": "PRIGIONIERI_M3"})
        msp.add_line((u, v - 3), (u, v + 3), dxfattribs={"layer": "PRIGIONIERI_M3"})
    # ---------- incisioni ----------
    lu, lv = P((LOGO_RIGHT, LOGO_BASE))
    add_text(msp, "DRUMMATIC", lu, lv, LOGO_CAP, "INCISIONE", anchor="end", tracking=1.2)
    cap = 2.2
    for (k, name) in zip(PAGE_KEYS + FUNC_KEYS + TRANSPORT_KEYS, PAGE_NAMES + FUNC_NAMES + TRANSPORT_NAMES):
        u, v = P(k); add_text(msp, name, u, v - KEY_HOLE / 2 - 1.6 - cap, cap, "INCISIONE")
    for i, k in enumerate(VOICE_KEYS):
        u, v = P(k); add_text(msp, str(i + 1), u, v - KEY_HOLE / 2 - 1.6 - cap, cap, "INCISIONE")
    for name, k in zip(["<", "UP", "DOWN", ">"], ARROW_KEYS):
        u, v = P(k); add_text(msp, name, u, v - KEY_HOLE / 2 - 1.6 - cap, cap, "INCISIONE")
    for i, k in enumerate(TRIG_KEYS):
        u, v = P(k); add_text(msp, str(i + 1), u, v - KEY_HOLE / 2 - 1.4 - 1.8, 1.8, "INCISIONE")
    u, v = P(VOLUME); add_text(msp, "VOLUME", u, v - 10.5 - cap, cap, "INCISIONE")
    u, v = P(DATA); add_text(msp, "DATA", u, v - 10.5 - cap, cap, "INCISIONE")
    # riquadro attorno ai tasti voce (linea incisa sottile come rettangolo stretto)
    doc.saveas(OUT / "pannello_superiore.dxf")


# ======================= PIASTRA POSTERIORE =======================
def dsize(msp, cx, cy):
    msp.add_circle((cx, cy), 12.0, dxfattribs={"layer": "TAGLIO"})
    msp.add_circle((cx - 9.5, cy + 12.0), 1.6, dxfattribs={"layer": "TAGLIO"})
    msp.add_circle((cx + 9.5, cy - 12.0), 1.6, dxfattribs={"layer": "TAGLIO"})


def rear_dxf():
    doc = new_doc(); msp = doc.modelspace()
    rrect(msp, REAR_PLATE_W / 2, REAR_PLATE_H / 2, REAR_PLATE_W, REAR_PLATE_H, 2.0, "TAGLIO")
    for name, u in REAR_CONNECTORS:
        dsize(msp, u, REAR_CONN_V)
        add_text(msp, name, u, REAR_CONN_V - 17.5 - 2.2, 2.2, "INCISIONE")
    # ventola 40 mm: 4 fori + griglia
    fu, fv = REAR_FAN_U, REAR_CONN_V
    for dx in (-16, 16):
        for dy in (-16, 16):
            msp.add_circle((fu + dx, fv + dy), 1.7, dxfattribs={"layer": "TAGLIO"})
    for i in range(-3, 4):
        for j in range(-3, 4):
            x, y = fu + i * 5.0 + (2.5 if j % 2 else 0), fv + j * 4.33
            if math.hypot(x - fu, y - fv) < 16.5:
                msp.add_circle((x, y), 1.8, dxfattribs={"layer": "TAGLIO"})
    add_text(msp, "FAN", fu, REAR_CONN_V - 17.5 - 2.2, 2.2, "INCISIONE")
    # fissaggio al vassoio: 6 svasati M3
    for u in (8, REAR_PLATE_W / 2, REAR_PLATE_W - 8):
        for v in (4.5, REAR_PLATE_H - 4.5):
            msp.add_circle((u, v), 1.7, dxfattribs={"layer": "SVASATI"})
    add_text(msp, "DRUMMATIC", REAR_PLATE_W - 18, REAR_PLATE_H - 11.5, 3.0, "INCISIONE", anchor="end", tracking=0.5)
    doc.saveas(OUT / "piastra_posteriore.dxf")


# ======================= STAFFA DISPLAY =======================
def bracket_dxf():
    doc = new_doc(); msp = doc.modelspace()
    # in coordinate layout relative al centro display; la staffa copre l'ingombro + 8 mm
    bx0, by0, bx1, by1 = 30.0, 8.0, 200.0, 120.0
    w, h = bx1 - bx0, by1 - by0
    rrect(msp, w / 2, h / 2, w, h, 3.0, "TAGLIO")
    # asole per regolazione ±3 mm in entrambe le direzioni (asola orizzontale + verticale = foro 9 mm con rondella)
    for (sx, sy) in DISPLAY_STUDS:
        x, y = sx - bx0, h - (sy - by0)
        msp.add_circle((x, y), 4.5, dxfattribs={"layer": "TAGLIO"})
    # finestra per connettori e cavi sul retro del display
    rrect(msp, w / 2, h / 2, 90, 40, 4, "TAGLIO")
    doc.saveas(OUT / "staffa_display.dxf")


# ======================= CONTORNO CIRCUITO DI CONTROLLO =======================
def pcb_dxf():
    doc = new_doc(); msp = doc.modelspace()
    x0, y0, x1, y1 = PCB_RECT
    nx0, ny0, nx1, ny1 = PCB_NOTCH
    # coordinate layout → pannello (u, v) per avere la stessa origine del pannello
    pts_l = [(x0, y1), (x1, y1), (x1, y0), (nx1, y0), (nx1, ny1), (nx0, ny1), (nx0, y0), (x0, y0)]
    msp.add_lwpolyline([L2P(*p) for p in pts_l], close=True, dxfattribs={"layer": "TAGLIO"})
    for s in PCB_STUDS:
        u, v = L2P(*s); msp.add_circle((u, v), 1.6, dxfattribs={"layer": "TAGLIO"})
    for k in ALL_KEYS:
        u, v = L2P(*k)
        rrect(msp, u, v, 15.0, 15.0, 0.5, "RIFERIMENTI")
    for e in ENCODERS + [DATA, VOLUME]:
        u, v = L2P(*e); msp.add_circle((u, v), 3.0, dxfattribs={"layer": "RIFERIMENTI"})
    doc.saveas(OUT / "circuito_controllo_contorno.dxf")


if __name__ == "__main__":
    panel_dxf(); rear_dxf(); bracket_dxf(); pcb_dxf()
    print("DXF ok:", sorted(p.name for p in OUT.glob("*.dxf")))
