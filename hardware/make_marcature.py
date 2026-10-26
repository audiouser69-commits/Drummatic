"""Incisioni del pannello superiore, versione "generica" (26/10/2026), per i file JLCCNC_finale.

Sul pannello restano solo le diciture valide con qualunque firmware: il nome DRUMMATIC, i numeri 1-8 (fila voci)
e 1-16 (passi), REC PLAY STOP, FUNC, le frecce, DATA e VOLUME. I nomi delle 8 pagine e dei tasti MAP LOCK DADO
COMMIT KIT COPY non si incidono piu': li mostra il display.

Coordinate: le stesse del pannello (u, v in mm, v dal bordo anteriore), come make_2d.py. Nei file finali i fori dei
tasti sono 18,5 x 19 mm (19 lungo v): le diciture stanno 1,6 mm sotto il bordo del foro come prima, quindi si usa
KEY_HOLE_V = 19 invece dei 15,8 di geom.py.
Uscite: finale/pannello_superiore_marcature.dxf (solo layer INCISIONE) e la stessa lista per il modello 3D."""
from pathlib import Path
import ezdxf
from geom import *
from make_2d import add_text, text_polys

KEY_HOLE_V = 19.0
OUT = Path(__file__).parent / "finale"
OUT.mkdir(exist_ok=True)
P = lambda xy: L2P(*xy)
ARROW_NAMES = ["<", "UP", "DOWN", ">"]
KEEP_FUNC = {"FUNC"}


def labels():
    """(testo, u, v, altezza maiuscole, ancora, tracking)"""
    out = []
    lu, lv = P((LOGO_RIGHT, LOGO_BASE))
    out.append(("DRUMMATIC", lu, lv, LOGO_CAP, "end", 1.2))
    cap = 2.2
    named = [(k, n) for k, n in zip(FUNC_KEYS, FUNC_NAMES) if n in KEEP_FUNC]
    named += list(zip(TRANSPORT_KEYS, TRANSPORT_NAMES))
    named += [(k, str(i + 1)) for i, k in enumerate(VOICE_KEYS)]
    named += list(zip(ARROW_KEYS, ARROW_NAMES))
    for k, name in named:
        u, v = P(k); out.append((name, u, v - KEY_HOLE_V / 2 - 1.6 - cap, cap, "middle", 0.0))
    for i, k in enumerate(TRIG_KEYS):
        u, v = P(k); out.append((str(i + 1), u, v - KEY_HOLE_V / 2 - 1.4 - 1.8, 1.8, "middle", 0.0))
    for name, k in (("VOLUME", VOLUME), ("DATA", DATA)):
        u, v = P(k); out.append((name, u, v - 10.5 - cap, cap, "middle", 0.0))
    return out


def write_dxf():
    doc = ezdxf.new("R2010", setup=True)
    doc.units = ezdxf.units.MM
    doc.layers.add("INCISIONE", color=5)
    msp = doc.modelspace()
    for s, u, v, cap, anchor, tr in labels():
        add_text(msp, s, u, v, cap, "INCISIONE", anchor=anchor, tracking=tr)
    doc.saveas(OUT / "pannello_superiore_marcature.dxf")


if __name__ == "__main__":
    write_dxf()
    print(len(labels()), "diciture:", " ".join(l[0] for l in labels()))
