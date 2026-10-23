"""Diagrammi SVG del capitolo LAZER (stesse formule del motore audio: BrkTapz::configure, BrkRepeat)."""
from pathlib import Path
import math

# stessi colori e funzioni di make_figs.py
INK, DIM, LINE, AMBER, AMBER_D, MINT, SOFT = "#1d2126", "#5f6873", "#d5dae0", "#e8962a", "#b86f12", "#2fae8e", "#f4f5f7"
FONT = 'font-family="Inter, sans-serif"'


def svg(w, h, body):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">{body}</svg>'


def text(x, y, s, size=11, fill=INK, anchor="start", weight=400):
    return f'<text x="{x}" y="{y}" {FONT} font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{s}</text>'

OUT = Path(__file__).parent / "img"


def tapz_times(taps, first, shape, decay):
    ratio = 2 ** (-shape * 1.2); gap = max(0.004, first); acc = 0; g = 1.0; fall = 0.35 + decay * 0.62; out = []
    for _ in range(taps):
        acc += gap; out.append((acc, g)); g *= fall; gap = max(0.0015, gap * ratio)
    return out


def fig_tapz():
    W, H = 720, 250
    b = []
    rows = [("SHAPE +0,7: accelera (pallina che rimbalza)", 0.7), ("SHAPE 0: ripetizioni regolari", 0.0), ("SHAPE −0,7: rallenta", -0.7)]
    x0, x1 = 230, 700
    for r, (label, shape) in enumerate(rows):
        y = 40 + r * 72
        b.append(text(10, y + 6, label.split(":")[0], 11, INK, weight=650))
        b.append(text(10, y + 22, label.split(":")[1].strip(), 10, DIM))
        b.append(f'<line x1="{x0}" y1="{y+28}" x2="{x1}" y2="{y+28}" stroke="{LINE}" stroke-width="1"/>')
        b.append(f'<rect x="{x0-2}" y="{y-14}" width="5" height="42" fill="{AMBER}"/>')
        taps = tapz_times(12, 0.12, shape, 0.6)
        span = 1.6
        for t, g in taps:
            if t > span: break
            x = x0 + t / span * (x1 - x0); h = 42 * g
            b.append(f'<rect x="{x-1.5:.1f}" y="{y+28-h:.1f}" width="3" height="{h:.1f}" fill="{MINT}"/>')
    for k in range(5):
        x = x0 + k / 4 * (x1 - x0)
        b.append(text(x, H - 6, f"{k*0.4:.1f} s".replace(".", ","), 9, DIM, "middle"))
    b.append(text(x0, 16, "colpo", 9, AMBER_D, "middle"))
    return svg(W, H, "".join(b))


def fig_repeat():
    W, H = 720, 170
    b = []
    x0, y = 20, 70
    sl = 70
    # fetta registrata
    b.append(f'<rect x="{x0}" y="{y-30}" width="{sl}" height="60" fill="{AMBER}" opacity="0.85" rx="3"/>')
    b.append(text(x0 + sl / 2, y + 48, "SLICE", 10, AMBER_D, "middle", 650))
    b.append(text(x0 + sl / 2, y - 38, "colpo registrato", 9, DIM, "middle"))
    kinds = ["norm", "gap", "rev", "norm", "crush", "rev", "gap", "norm"]
    lab = {"norm": "ripete", "gap": "GAP", "rev": "REVERSE", "crush": "CRUSH"}
    for i, k in enumerate(kinds):
        x = x0 + (i + 1) * (sl + 8)
        if k == "gap":
            b.append(f'<rect x="{x}" y="{y-30}" width="{sl}" height="60" fill="none" stroke="{LINE}" stroke-dasharray="4 3" rx="3"/>')
        else:
            pts = []
            for j in range(31):
                u = j / 30
                env = (1 - u) ** 2 if k != "rev" else u ** 2
                v = math.sin(u * 40) * env * 26
                if k == "crush": v = round(v / 9) * 9
                pts.append(f"{x + u*sl:.1f},{y - v:.1f}")
            b.append(f'<rect x="{x}" y="{y-30}" width="{sl}" height="60" fill="{SOFT}" stroke="{LINE}" rx="3"/>')
            b.append(f'<polyline points="{" ".join(pts)}" fill="none" stroke="{MINT}" stroke-width="1.6"/>')
        b.append(text(x + sl / 2, y + 48, lab[k], 9.5, INK if k != "norm" else DIM, "middle", 650 if k != "norm" else 400))
    b.append(text(x0, H - 6, "REPEATS = 8 · GAPS, REVERSE e CRUSH decidono a caso, ripetizione per ripetizione, cosa succede", 10, DIM))
    return svg(W, H, "".join(b))


def fig_flow():
    W, H = 720, 150
    box = lambda x, y, w, t, fill=SOFT, col=INK: (f'<rect x="{x}" y="{y}" width="{w}" height="34" rx="4" fill="{fill}" stroke="{LINE}"/>'
                                                  + text(x + w / 2, y + 21, t, 10.5, col, "middle", 650))
    arrow = lambda x1, y1, x2, y2: f'<line x1="{x1}" y1="{y1}" x2="{x2}" y2="{y2}" stroke="{DIM}" stroke-width="1.2" marker-end="url(#a)"/>'
    b = ['<defs><marker id="a" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="6" markerHeight="6" orient="auto"><path d="M0,0 L10,5 L0,10 z" fill="#5f6873"/></marker></defs>']
    b.append(box(10, 10, 110, "SYNTH")); b.append(box(10, 58, 110, "SAMPLER"))
    b.append(box(10, 106, 110, "LAZER + TAIL", "#fdf1e0", AMBER_D))
    b.append(box(150, 34, 70, "BLEND"))
    b.append(arrow(120, 27, 150, 46)); b.append(arrow(120, 75, 150, 56))
    b.append(box(250, 58, 110, "FILTRO / DIST."))
    b.append(arrow(220, 51, 250, 70)); b.append(arrow(120, 123, 250, 80))
    b.append(box(390, 58, 110, "BRK REPEAT", "#fdf1e0", AMBER_D)); b.append(arrow(360, 75, 390, 75))
    b.append(box(530, 58, 90, "BRK TAPZ", "#fdf1e0", AMBER_D)); b.append(arrow(500, 75, 530, 75))
    b.append(box(645, 58, 70, "GAIN · PAN")); b.append(arrow(620, 75, 645, 75))
    return svg(W, H, "".join(b))


if __name__ == "__main__":
    (OUT / "f_tapz.svg").write_text(fig_tapz())
    (OUT / "f_repeat.svg").write_text(fig_repeat())
    (OUT / "f_lzflow.svg").write_text(fig_flow())
    print("ok")
