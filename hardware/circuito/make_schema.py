"""Schema elettrico leggibile del circuito di controllo (PDF A3), generato dagli stessi dati del circuito stampato."""
import json
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import FancyBboxPatch, Rectangle
from matplotlib.font_manager import FontProperties, fontManager

HERE = Path(__file__).parent
OUT = HERE / "out"
FONT = "/home/claude/hw/fonts/Michroma-Regular.ttf"
fontManager.addfont(FONT)
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 7})
INK = "#1d1f22"; ACC = "#e8962a"; BLUE = "#2a6fdb"; GREEN = "#2e7d32"; GREY = "#666"
km = json.loads((OUT / "keymap.json").read_text())
TOTAL = 3


def sheet(title, n):
    fig = plt.figure(figsize=(16.54, 11.69))
    ax = fig.add_axes([0, 0, 1, 1]); ax.axis("off"); ax.set_xlim(0, 420); ax.set_ylim(0, 297)
    ax.add_patch(Rectangle((10, 10), 400, 277, fill=False, lw=1, ec=INK))
    ax.add_patch(Rectangle((250, 10), 160, 24, fill=False, lw=0.8, ec=INK))
    ax.text(254, 27, "DRUMMATIC", fontproperties=FontProperties(fname=FONT, size=13), color=INK)
    ax.text(254, 20, title, fontsize=9, weight="bold")
    ax.text(254, 14, "Circuito di controllo · rev. A · 2026-10 · KiCad 7", fontsize=6, color=GREY)
    ax.text(404, 27, f"{n}/{TOTAL}", fontsize=10, ha="right")
    return fig, ax


def box(ax, x, y, w, h, title, lines=(), color=INK, fs=6.5):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=2", fc="white", ec=color, lw=1.2))
    ax.text(x + 3, y + h - 4, title, fontsize=8, weight="bold", color=color, va="top")
    for i, l in enumerate(lines):
        ax.text(x + 3, y + h - 11 - i * 4.2, l, fontsize=fs, va="top", family="DejaVu Sans Mono")


def arrow(ax, a, b, label="", color=GREY):
    ax.annotate("", b, a, arrowprops=dict(arrowstyle="-|>", lw=0.9, color=color))
    if label:
        ax.text((a[0] + b[0]) / 2, (a[1] + b[1]) / 2 + 1.5, label, fontsize=6, ha="center", color=color)


def page1(pdf):
    fig, ax = sheet("Schema a blocchi e collegamenti del Pico 2", 1)
    box(ax, 160, 120, 90, 120, "U1  Raspberry Pi Pico 2", [
        "pin  GPIO   rete", " 1   GP0    ROW0", " 2   GP1    ROW1", " 4   GP2    ROW2", " 5   GP3    ROW3",
        " 6   GP4    ROW4", " 7   GP5    ROW5", " 9   GP6    ROW6", "10   GP7    COL0", "11   GP8    COL1",
        "12   GP9    COL2", "14   GP10   COL3", "15   GP11   COL4", "16   GP12   COL5", "17   GP13   COL6",
        "19   GP14   COL7", "20   GP15   LED_3V3", "21   GP16   SR_LOAD", "22   GP17   SR_CLK", "24   GP18   SR_DATA",
        "31   GP26   ADC0 (volume)", "33   AGND   AGND", "35   VREF   ADC_VREF", "36   3V3    +3V3", "40   VBUS   VBUS",
        "3,8,13,18,23,28,38  GND"], color=BLUE, fs=6)
    box(ax, 20, 190, 110, 50, "Matrice tasti 7 x 8", ["46 tasti Choc + 9 pulsanti encoder", "prese hot-swap Kailh CPG135001S30",
        "un diodo 1N4148W per tasto (anodo verso", "la colonna, catodo verso la riga)", "scansione: riga bassa, colonne con pull-up"])
    box(ax, 20, 120, 110, 55, "Encoder (9 x Alps EC11E)", ["A, B -> pull-up 10k a +3V3 -> 74HC165", "C e linguette -> GND",
        "pulsante S1/S2 -> nella matrice", "U2: ENC1-4   U3: ENC5-8   U4: DATA", "LOAD/CLK in comune, U4->U3->U2->GP18",
        "CLK INH a GND, ingressi liberi a GND"])
    box(ax, 280, 190, 115, 50, "LED (46 x SK6812MINI-E)", ["GP15 -> U5 74AHCT1G125 (3,3 -> 5 V)", "-> R2 330R -> DIN del LED 1",
        "catena a serpentina: pagine/funzioni,", "poi voci/trasporto all'indietro, poi TRIG", "100nF per LED, firmware limita al 30 %"])
    box(ax, 280, 120, 115, 55, "Alimentazione", ["USB del Pico (cavo dal Raspberry Pi 5)", "VBUS -> F1 PTC 1,5 A -> +5V",
        "+5V: LED, U5, J1 (display)", "C70 220uF + 100nF locali", "+3V3 dal Pico: 74HC165, pull-up", "AGND unita a GND con R3 0R"])
    box(ax, 20, 45, 110, 55, "Volume (RV1 Alps RK09K 10k)", ["pin 1 -> AGND", "pin 3 -> ADC_VREF", "cursore -> R1 1k -> GP26",
        "C60 100nF verso AGND (filtro)"])
    box(ax, 280, 45, 115, 55, "Connettori", ["USB micro del Pico: al Raspberry Pi 5", "J1 2 pin 2,54 mm: 1 = +5V, 2 = GND",
        "   cavo di alimentazione del display", "H1-H10: fori M3 sui prigionieri del", "   pannello, collegati a GND"])
    for a, b, l in (((130, 215), (160, 200), "ROW0-6 / COL0-7"), ((130, 147), (160, 150), "SR_LOAD/CLK/DATA"),
                    ((250, 215), (280, 215), "LED_3V3 (GP15)"), ((280, 147), (250, 147), "VBUS / +3V3"),
                    ((130, 72), (160, 125), "ADC0 (GP26)")):
        arrow(ax, a, b, l)
    pdf.savefig(fig); plt.close(fig)


def page2(pdf):
    fig, ax = sheet("Matrice tasti: righe, colonne e note MIDI", 2)
    keys = km["keys"]; grid = [["" for _ in range(8)] for _ in range(7)]
    for i, (n, r, c) in enumerate(keys):
        grid[r][c] = f"{n}\nnota {i}"
    for j, e in enumerate(km["encoders"]):
        r, c = km["enc_buttons"][e]; grid[r][c] = f"{e} (premuto)\nnota {64 + j}"
    x0, y0, cw, ch = 40, 250, 42, 26
    for c in range(8):
        ax.text(x0 + cw * c + cw / 2, y0 + 5, f"COL{c}\nGP{7 + c}", ha="center", fontsize=7, weight="bold", color=BLUE)
    for r in range(7):
        ax.text(x0 - 4, y0 - ch * r - ch / 2, f"ROW{r}\nGP{r}", ha="right", va="center", fontsize=7, weight="bold", color=BLUE)
        for c in range(8):
            ax.add_patch(Rectangle((x0 + cw * c, y0 - ch * (r + 1)), cw, ch, fc="#f7f7f5" if grid[r][c] else "white", ec="#bbb", lw=0.5))
            ax.text(x0 + cw * c + cw / 2, y0 - ch * r - ch / 2, grid[r][c] or "—", ha="center", va="center", fontsize=6.5)
    ax.text(40, 55, "Ogni incrocio: colonna -> contatto della presa hot-swap -> diodo 1N4148W (anodo) -> catodo -> riga.", fontsize=7.5)
    ax.text(40, 49, "Messaggi USB-MIDI del firmware, canale 16: tasto premuto = Note On (velocity 127), rilasciato = Note Off.", fontsize=7.5)
    ax.text(40, 43, "Encoder 1-8 = CC 16-23, DATA = CC 24 (relativi: 1-63 destra, 65-127 sinistra); volume = CC 7.", fontsize=7.5)
    ax.text(40, 37, "LED: Note On n con velocity = colore (0 spento, 1-15 tavolozza, +16 metà luce); CC 119 luminosità; CC 121 tutto spento.", fontsize=7.5)
    pdf.savefig(fig); plt.close(fig)


def page3(pdf):
    fig, ax = sheet("Registri a scorrimento, catena LED e distinta", 3)
    sr = [("U2", ["ENC1_A", "ENC1_B", "ENC2_A", "ENC2_B", "ENC3_A", "ENC3_B", "ENC4_A", "ENC4_B"], "SR_S1", "SR_DATA (GP18)"),
          ("U3", ["ENC5_A", "ENC5_B", "ENC6_A", "ENC6_B", "ENC7_A", "ENC7_B", "ENC8_A", "ENC8_B"], "SR_S2", "SR_S1 -> SER di U2"),
          ("U4", ["DATA_A", "DATA_B", "GND", "GND", "GND", "GND", "GND", "GND"], "GND", "SR_S2 -> SER di U3")]
    pins = ["A (11)", "B (12)", "C (13)", "D (14)", "E (3)", "F (4)", "G (5)", "H (6)"]
    for k, (u, ins, ser, qh) in enumerate(sr):
        lines = [f"{p:7s} {n}" for p, n in zip(pins, ins)] + [f"SER(10) {ser}", f"QH (9)  {qh}", "SH/LD(1) SR_LOAD  CLK(2) SR_CLK",
                                                            "CLK INH(15) GND  VCC(16) +3V3  GND(8)"]
        box(ax, 20 + k * 78, 165, 74, 75, f"{u}  74HC165", lines, fs=5.8)
    order = km["led_order"]
    txt = ", ".join(f"{i + 1}:{n}" for i, n in enumerate(order))
    ax.text(260, 236, "Catena LED (ordine DIN -> DOUT)", fontsize=8, weight="bold")
    import textwrap
    for i, l in enumerate(textwrap.wrap(txt, 62)):
        ax.text(260, 229 - i * 4.4, l, fontsize=6.3, family="DejaVu Sans Mono")
    parts = json.loads((OUT / "parts.json").read_text())
    from collections import Counter
    def grp(v):
        if v.startswith("Choc"): return "Choc v1"
        if v.startswith("EC11E"): return "EC11E"
        return v
    cnt = Counter((grp(p["value"]), p["fp"], p["mpn"]) for p in parts)
    ax.text(20, 150, "Distinta componenti", fontsize=9, weight="bold")
    y = 143
    for (v, fp, mpn), n in sorted(cnt.items(), key=lambda t: -t[1]):
        ax.text(20, y, f"{n:3d} x", fontsize=6.6, family="DejaVu Sans Mono"); ax.text(34, y, v, fontsize=6.6)
        ax.text(90, y, mpn, fontsize=6.6, color=GREY); ax.text(175, y, fp, fontsize=6, color=GREY); y -= 4.6
    pdf.savefig(fig); plt.close(fig)


if __name__ == "__main__":
    with PdfPages(OUT / "Drummatic-Schema-Controllo.pdf") as pdf:
        page1(pdf); page2(pdf); page3(pdf)
    print("ok")
