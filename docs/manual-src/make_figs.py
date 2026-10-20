"""Diagrammi SVG del capitolo sul sampler (stesse formule del motore audio)."""
import math, wave
from pathlib import Path
import numpy as np

OUT = Path(__file__).parent / "img"
OUT.mkdir(exist_ok=True)
SAMPLES = Path.home() / "Documents/Drummatic/Samples"
INK, DIM, LINE, AMBER, AMBER_D, MINT, SOFT = "#1d2126", "#5f6873", "#d5dae0", "#e8962a", "#b86f12", "#2fae8e", "#f4f5f7"
FONT = 'font-family="Inter, sans-serif"'


def svg(w, h, body):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">{body}</svg>'


def text(x, y, s, size=11, fill=INK, anchor="start", weight=400):
    return f'<text x="{x}" y="{y}" {FONT} font-size="{size}" fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{s}</text>'


def load(name):
    with wave.open(str(SAMPLES / name)) as w:
        n, ch = w.getnframes(), w.getnchannels()
        x = np.frombuffer(w.readframes(n), dtype="<i2").astype(float) / 32768
    return x.reshape(-1, ch)[:, 0]


def peaks(x, cols):
    idx = np.linspace(0, len(x), cols + 1).astype(int)
    return np.array([np.abs(x[a:max(a + 1, b)]).max() for a, b in zip(idx[:-1], idx[1:])])


def wave_path(p, x0, y0, w, h, fill=AMBER, opacity=0.9):
    cols = len(p); dx = w / cols; mid = y0 + h / 2
    top = " ".join(f"L{x0 + i * dx:.1f},{mid - v * h * 0.48:.1f}" for i, v in enumerate(p))
    bot = " ".join(f"L{x0 + i * dx:.1f},{mid + v * h * 0.48:.1f}" for i, v in reversed(list(enumerate(p))))
    return f'<path d="M{x0},{mid} {top} {bot} Z" fill="{fill}" fill-opacity="{opacity}"/>'


# ---------- 1. percorso del segnale ----------
def fig_flow():
    W, H = 820, 160
    b = []
    def box(x, w, title, sub, hi=False):
        b.append(f'<rect x="{x}" y="58" width="{w}" height="58" rx="7" fill="{"#fdf1e0" if hi else SOFT}" stroke="{AMBER if hi else LINE}" stroke-width="{1.6 if hi else 1}"/>')
        b.append(text(x + w / 2, 83, title, 12, INK, "middle", 700))
        b.append(text(x + w / 2, 101, sub, 9.5, DIM, "middle"))
    def arrow(x1, x2, y=87):
        b.append(f'<line x1="{x1}" y1="{y}" x2="{x2 - 6}" y2="{y}" stroke="{INK}" stroke-width="1.4"/><path d="M{x2},{y} l-7,-4 v8 z" fill="{INK}"/>')
    b.append(f'<rect x="10" y="4" width="140" height="34" rx="6" fill="#fff" stroke="{LINE}"/>')
    b.append(text(80, 19, "SEQUENCER / MIDI", 9.5, DIM, "middle", 700)); b.append(text(80, 32, "colpo + valori delle lane", 9, DIM, "middle"))
    b.append(f'<line x1="80" y1="38" x2="80" y2="52" stroke="{INK}" stroke-width="1.4"/><path d="M80,58 l-4,-7 h8 z" fill="{INK}"/>')
    box(10, 140, "Campione", "START · END · PITCH", True)
    box(180, 170, "Motore sampler", "4 motori di riproduzione", True)
    box(380, 120, "Inviluppo", "Off · Env 1 · Env 2", True)
    box(530, 100, "BLEND", "+ synth", True)
    box(660, 150, "Filtro → Distorsione", "Gain · Pan · Mandate")
    arrow(150, 180); arrow(350, 380); arrow(500, 530); arrow(630, 660)
    b.append(text(410, 145, "Il sampler è una delle due sorgenti della voce: ogni colpo del sequencer lo fa ripartire da START.", 10, DIM, "middle"))
    return svg(W, H, "".join(b))


# ---------- 2. BLEND ----------
def fig_blend():
    W, H = 360, 190
    x0, y0, w, h = 50, 20, 280, 120
    b = [f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" fill="#fff" stroke="{LINE}"/>']
    for f in (0.25, 0.5, 0.75):
        b.append(f'<line x1="{x0 + w * f}" y1="{y0}" x2="{x0 + w * f}" y2="{y0 + h}" stroke="{LINE}" stroke-dasharray="3 3"/>')
    syn = " ".join(f"{x0 + w * t:.1f},{y0 + h - h * min(1, 2 * (1 - t)):.1f}" for t in np.linspace(0, 1, 41))
    smp = " ".join(f"{x0 + w * t:.1f},{y0 + h - h * min(1, 2 * t):.1f}" for t in np.linspace(0, 1, 41))
    b.append(f'<polyline points="{syn}" fill="none" stroke="{INK}" stroke-width="2.4"/>')
    b.append(f'<polyline points="{smp}" fill="none" stroke="{AMBER}" stroke-width="2.4"/>')
    for t, lab in ((0, "0%"), (0.5, "50%"), (1, "100%")):
        b.append(text(x0 + w * t, y0 + h + 16, lab, 10, DIM, "middle"))
    b.append(text(x0 - 8, y0 + 4, "100%", 9, DIM, "end")); b.append(text(x0 - 8, y0 + h, "0", 9, DIM, "end"))
    b.append(text(x0 + w / 2, H - 16, "posizione di BLEND", 10, DIM, "middle"))
    b.append(text(x0 + 8, y0 + 30, "synth", 11, INK, weight=700)); b.append(text(x0 + w - 8, y0 + 30, "sampler", 11, AMBER_D, "end", 700))
    return svg(W, H, "".join(b))


# ---------- 3. START / END ----------
def fig_startend():
    x = load("Drummatic Break 120.wav"); p = peaks(x, 220)
    W, H = 820, 250
    b = []
    rows = [("Regione normale: suona da START verso END", 0.12, 0.62, False),
            ("END prima di START: il campione suona al contrario", 0.62, 0.12, True),
            ("START modulato: ogni colpo parte da un punto diverso", 0.0, 1.0, None)]
    for r, (lab, s, e, rev) in enumerate(rows):
        y = 10 + r * 80; x0, w, h = 20, 560, 56
        b.append(f'<rect x="{x0}" y="{y}" width="{w}" height="{h}" rx="4" fill="#15181c"/>')
        b.append(wave_path(p, x0, y, w, h, AMBER, 0.75))
        if rev is None:
            for k, f in enumerate((0.0, 0.25, 0.5, 0.75)):
                xs = x0 + f * w
                b.append(f'<line x1="{xs}" y1="{y}" x2="{xs}" y2="{y + h}" stroke="{MINT}" stroke-width="1.6" stroke-dasharray="{"none" if k == 0 else "4 3"}"/>')
                b.append(f'<circle cx="{xs}" cy="{y + 5}" r="4.5" fill="{MINT}"/>')
                b.append(text(xs + 7, y + 15, f"colpo {k + 1}", 9, "#bff0e2"))
        else:
            a, c = x0 + min(s, e) * w, x0 + max(s, e) * w
            b.append(f'<rect x="{x0}" y="{y}" width="{a - x0}" height="{h}" fill="#15181c" fill-opacity=".72"/>')
            b.append(f'<rect x="{c}" y="{y}" width="{x0 + w - c}" height="{h}" fill="#15181c" fill-opacity=".72"/>')
            xs, xe = x0 + s * w, x0 + e * w
            for xx, top in ((xs, True), (xe, False)):
                b.append(f'<line x1="{xx}" y1="{y}" x2="{xx}" y2="{y + h}" stroke="{MINT}" stroke-width="1.8"/>')
                b.append(f'<circle cx="{xx}" cy="{y + 5 if top else y + h - 5}" r="5" fill="{MINT}"/>')
            b.append(text(xs, y - 2 + (0 if s < e else 0), "START", 8.5, MINT, "middle", 700) if False else "")
            ay = y + h / 2; dirn = 1 if not rev else -1
            b.append(f'<line x1="{xs + 10 * dirn}" y1="{ay}" x2="{xe - 14 * dirn}" y2="{ay}" stroke="#fff" stroke-width="1.6"/>')
            b.append(f'<path d="M{xe - 6 * dirn},{ay} l{-9 * dirn},-5 v10 z" fill="#fff"/>')
        b.append(text(600, y + 24, lab.split(":")[0], 11, INK, weight=700))
        b.append(text(600, y + 40, lab.split(":")[1].strip() if ":" in lab else "", 10, DIM))
    return svg(W, H, "".join(b))


# ---------- 4. inviluppo che taglia il campione ----------
def fig_env():
    x = load("Drummatic Break 120.wav")[: int(44100 * 3)]; p = peaks(x, 200)
    W, H = 820, 200
    b = []
    for r, (lab, sub, dec) in enumerate((("Off", "il campione suona fino alla fine", None), ("Env 2, DECAY 800 ms", "sfumato e fermato dall'inviluppo", 0.8))):
        y = 10 + r * 92; x0, w, h = 20, 560, 70
        b.append(f'<rect x="{x0}" y="{y}" width="{w}" height="{h}" rx="4" fill="#15181c"/>')
        if dec is None:
            b.append(wave_path(p, x0, y, w, h, AMBER, 0.85))
        else:
            t = np.linspace(0, 3, len(p)); env = np.clip(1 - t / dec, 0, None) * 0 + np.where(t < dec, (np.exp(-6.9 * t / dec) - 0.001) / 0.999, 0)
            b.append(wave_path(p * np.maximum(env, 0), x0, y, w, h, AMBER, 0.85))
            b.append(wave_path(p, x0, y, w, h, "#ffffff", 0.08))
            pts = " ".join(f"{x0 + w * tt / 3:.1f},{y + h - h * max(e, 0) * 0.95:.1f}" for tt, e in zip(t, env))
            b.append(f'<polyline points="{pts}" fill="none" stroke="{MINT}" stroke-width="2"/>')
            b.append(text(x0 + w * dec / 3 + 6, y + 16, "fine dell'inviluppo", 9, "#bff0e2"))
        b.append(text(600, y + 30, lab, 11, INK, weight=700)); b.append(text(600, y + 46, sub, 10, DIM))
    b.append(text(20, H - 2, "Il menu accanto al nome del motore sceglie quale inviluppo controlla il volume del sampler.", 9.5, DIM))
    return svg(W, H, "".join(b))


# ---------- 5. Phase Warp: mappe del tempo (stesse formule del motore) ----------
def warp_map(t, mirror=0.5, shape=0.5, split=0.5, swing=0.5):
    u = t ** (3 ** ((shape - 0.5) * 2))
    k = max(-0.8, min(0.8, (split - 0.5) * 1.6)); first = 0.5 * (1 - k)
    u = u * 2 * first if u < 0.5 else first + (u - 0.5) * 2 * (1 - first)
    ts = max(0.1, min(0.9, 0.5 - (swing - 0.5) * 0.8))
    u = u / ts * 0.5 if u < ts else 0.5 + (u - ts) / (1 - ts) * 0.5
    if mirror > 0.51:
        sp = 1 + (mirror - 0.5) * 2 * 3; xx = (u * sp) % 2; u = xx if xx <= 1 else 2 - xx
    elif mirror < 0.49:
        u = u % max(0.02, mirror * 2)
    return u


def fig_warp():
    cases = [("MIRROR", dict(mirror=0.8), "rimbalza fra inizio e fine"), ("MIRROR", dict(mirror=0.2), "ripete in loop l'inizio"),
             ("SHAPE", dict(shape=0.85), "accelera verso la fine"), ("SHAPE", dict(shape=0.15), "rallenta verso la fine"),
             ("SPLIT", dict(split=0.85), "lento poi veloce"), ("SWING", dict(swing=0.85), "arriva prima a metà")]
    W, H = 820, 330
    b = []
    for i, (name, kw, desc) in enumerate(cases):
        cx, cy = 20 + (i % 3) * 250, 10 + (i // 3) * 160
        s = 118
        b.append(f'<rect x="{cx}" y="{cy}" width="{s}" height="{s}" fill="#fff" stroke="{LINE}"/>')
        b.append(f'<line x1="{cx}" y1="{cy + s}" x2="{cx + s}" y2="{cy}" stroke="{LINE}" stroke-dasharray="3 3"/>')
        ts = np.linspace(0, 0.999, 300)
        pts = " ".join(f"{cx + s * t:.1f},{cy + s - s * warp_map(t, **kw):.1f}" for t in ts)
        b.append(f'<polyline points="{pts}" fill="none" stroke="{AMBER}" stroke-width="2.2"/>')
        val = list(kw.values())[0]
        b.append(text(cx + s + 12, cy + 18, name, 12, INK, weight=700))
        b.append(text(cx + s + 12, cy + 34, f"{'sopra' if val > 0.5 else 'sotto'} il centro", 9.5, AMBER_D))
        for j, line in enumerate(desc.split(" fra ") if " fra " in desc else [desc]):
            b.append(text(cx + s + 12, cy + 52 + j * 13, (("fra " if j else "") + line), 9.5, DIM))
        b.append(text(cx + s / 2, cy + s + 13, "tempo del colpo →", 8.5, DIM, "middle"))
        b.append(f'<text x="{cx - 5}" y="{cy + s / 2}" {FONT} font-size="8.5" fill="{DIM}" text-anchor="middle" transform="rotate(-90 {cx - 5} {cy + s / 2})">posizione nel campione →</text>')
    return svg(W, H, "".join(b))


# ---------- 6. Granular ----------
def fig_gran():
    x = load("Drummatic Pad A.wav"); p = peaks(x, 240)
    W, H = 820, 250
    x0, y0, w, h = 20, 30, 560, 80
    b = [f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="4" fill="#15181c"/>', wave_path(p, x0, y0, w, h, AMBER, 0.55)]
    ph = x0 + 0.32 * w
    b.append(f'<line x1="{ph}" y1="{y0 - 8}" x2="{ph}" y2="{y0 + h}" stroke="#fff" stroke-width="2"/>')
    b.append(text(ph, y0 - 12, "testina (si muove con SPEED)", 9.5, INK, "middle", 700))
    b.append(f'<path d="M{ph + 6},{y0 - 4} h40" stroke="{INK}" stroke-width="1.4"/><path d="M{ph + 50},{y0 - 4} l-7,-4 v8 z" fill="{INK}"/>')
    rng = np.random.default_rng(5)
    gy = y0 + h + 14
    for k in range(9):
        cx = ph + rng.normal(0, 22); gw = 34; gx = cx - gw / 2 + k * 0
        yy = gy + (k % 3) * 18
        pts = " ".join(f"{gx + gw * t:.1f},{yy + 14 - 14 * (0.5 - 0.5 * math.cos(2 * math.pi * t)):.1f}" for t in np.linspace(0, 1, 30))
        b.append(f'<polyline points="{pts}" fill="{MINT}" fill-opacity=".22" stroke="{MINT}" stroke-width="1.4"/>')
    b.append(f'<line x1="{ph}" y1="{y0 + h}" x2="{ph}" y2="{gy + 50}" stroke="#fff" stroke-dasharray="3 3" opacity=".0"/>')
    b.append(text(ph + 70, gy + 22, "grani: copie brevi del suono, con finestra morbida", 9.5, DIM))
    b.append(text(ph + 70, gy + 36, "BLUR li sparpaglia attorno alla testina", 9.5, DIM))
    items = [("SIZE", "durata di ogni grano: 5–300 ms"), ("RATE", "quanti grani al secondo: 4–240"),
             ("SPEED", "centro = testina ferma su START; destra avanti, sinistra indietro (fino a 2×)"),
             ("BLUR", "sopra il centro sparpaglia la posizione, sotto inverte alcuni grani")]
    for i, (k, v) in enumerate(items):
        b.append(text(600, 18 + i * 60, k, 11.5, INK, weight=700))
        words = v.split(" "); line, lines = "", []
        for wd in words:
            if len(line) + len(wd) > 34: lines.append(line); line = wd
            else: line = (line + " " + wd).strip()
        lines.append(line)
        for j, l in enumerate(lines[:3]): b.append(text(600, 32 + i * 60 + j * 12, l, 9, DIM))
    return svg(W, H, "".join(b))


# ---------- 7. Cloner ----------
def fig_clone():
    W, H = 820, 170
    rng = np.random.default_rng(11)
    b = []
    bands = ["80", "160", "320", "640", "1,3k", "2,6k", "5k", "10k"]
    for hit in range(3):
        x0 = 20 + hit * 190
        b.append(text(x0 + 80, 14, f"colpo {hit + 1}", 10.5, INK, "middle", 700))
        gains = np.clip(1 + rng.uniform(-1, 1, 8) * 0.35, 0.3, 1.5)
        for i, g in enumerate(gains):
            hh = g * 70
            b.append(f'<rect x="{x0 + i * 20}" y="{110 - hh}" width="14" height="{hh}" rx="2" fill="{AMBER if hit == 0 else (MINT if hit == 1 else INK)}" fill-opacity=".85"/>')
            if hit == 0: b.append(text(x0 + i * 20 + 7, 124, bands[i], 7.5, DIM, "middle"))
    b.append(text(600, 40, "8 bande di frequenza", 11.5, INK, weight=700))
    b.append(text(600, 56, "VARIATION decide quanto", 9.5, DIM)); b.append(text(600, 69, "cambia ogni banda a ogni colpo", 9.5, DIM))
    b.append(text(600, 92, "TILT inclina le bande", 9.5, DIM)); b.append(text(600, 105, "dai bassi agli acuti", 9.5, DIM))
    b.append(text(600, 128, "DAMP sopra il centro smorza", 9.5, DIM)); b.append(text(600, 141, "gli acuti, sotto esalta le risonanze", 9.5, DIM))
    b.append(text(20, 150, "Stesso campione, tre colpi: il timbro cambia appena, come un musicista vero che non colpisce mai due volte uguale.", 9.5, DIM))
    return svg(W, H, "".join(b))


# ---------- 8. chop: lane MACRO 2 -> START ----------
def fig_chop():
    x = load("Drummatic Break 120.wav"); p = peaks(x, 256)
    W, H = 760, 260
    x0, y0, w, h = 20, 18, 720, 70
    b = [f'<rect x="{x0}" y="{y0}" width="{w}" height="{h}" rx="4" fill="#15181c"/>', wave_path(p, x0, y0, w, h, AMBER, 0.8)]
    for k in range(8):
        xx = x0 + w * k / 8
        if True:
            b.append(f'<line x1="{xx}" y1="{y0}" x2="{xx}" y2="{y0 + h}" stroke="{MINT}" stroke-width="1.2"/>')
            b.append(text(xx + 4, y0 + 12, chr(65 + k), 10, "#bff0e2", weight=700))
    b.append(text(x0, y0 - 5, "Il break diviso in 8 fette (A–H): MACRO 2 → START con profondità 96%, una fetta ogni 13 punti della lane", 9.5, DIM))
    lane = [0, 13, 26, 39, 52, 13, 78, 91, 0, 39, 26, 65, 52, 91, 78, 91]
    ly = 120; lh = 80; bw = w / 16
    b.append(f'<rect x="{x0}" y="{ly}" width="{w}" height="{lh}" fill="#fff" stroke="{LINE}"/>')
    for i, v in enumerate(lane):
        hh = lh * v / 100
        b.append(f'<rect x="{x0 + i * bw + 3}" y="{ly + lh - hh}" width="{bw - 6}" height="{hh}" fill="{AMBER}" fill-opacity=".85"/>')
        sl = min(7, round(0.96 * v / 100 * 8))
        b.append(text(x0 + i * bw + bw / 2, ly + lh + 15, chr(65 + sl), 10.5, INK, "middle", 700))
        b.append(text(x0 + i * bw + bw / 2, ly + lh + 28, str(v), 8, DIM, "middle"))
    b.append(text(x0, ly - 6, "Lane MACRO 2 nel sequencer (16 passi) → fetta suonata a ogni passo", 9.5, DIM))
    return svg(W, H, "".join(b))


FIGS = {"f_flow": fig_flow, "f_blend": fig_blend, "f_startend": fig_startend, "f_env": fig_env,
        "f_warp": fig_warp, "f_gran": fig_gran, "f_clone": fig_clone, "f_chop": fig_chop}
for name, fn in FIGS.items():
    (OUT / f"{name}.svg").write_text(fn(), encoding="utf-8")
print("ok", list(FIGS))
