"""Tavole quotate (PDF A3) del contenitore Drummatic."""
import math, json
from pathlib import Path
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages
from matplotlib.patches import Polygon as MPoly, Circle, Rectangle, FancyBboxPatch
from matplotlib.font_manager import FontProperties, fontManager
import ezdxf
from ezdxf.addons.drawing import RenderContext, Frontend
from ezdxf.addons.drawing.matplotlib import MatplotlibBackend
from ezdxf.addons.drawing.config import Configuration, ColorPolicy
from geom import *

HERE = Path(__file__).parent
OUT = HERE / "out"
fontManager.addfont(str(HERE / "fonts/Michroma-Regular.ttf"))
plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 7})
INK = "#1d1f22"; DIM = "#c0392b"; ACC = "#e8962a"
A3 = (16.54, 11.69)


def sheet(title, n, total, scale="1:2"):
    fig = plt.figure(figsize=A3)
    fig.patch.set_facecolor("white")
    # cornice e cartiglio
    fr = fig.add_axes([0, 0, 1, 1]); fr.axis("off"); fr.set_xlim(0, 420); fr.set_ylim(0, 297)
    fr.add_patch(Rectangle((10, 10), 400, 277, fill=False, lw=1.0, ec=INK))
    fr.add_patch(Rectangle((250, 10), 160, 24, fill=False, lw=0.8, ec=INK))
    fr.text(254, 27, "DRUMMATIC", fontproperties=FontProperties(fname=str(HERE / "fonts/Michroma-Regular.ttf"), size=13), color=INK)
    fr.text(254, 20, title, fontsize=9, color=INK, weight="bold")
    fr.text(254, 14, f"Quote in mm · scala {scale} su A3 · tolleranze generali ISO 2768-m · rev. A · 2026-10", fontsize=6, color="#555")
    fr.text(404, 27, f"{n}/{total}", fontsize=10, ha="right", color=INK)
    return fig, fr


def dim(ax, p1, p2, off, text=None, vertical=False, fs=6.5):
    """quota lineare fra p1 e p2 spostata di off (in direzione perpendicolare)"""
    (x1, y1), (x2, y2) = p1, p2
    if vertical:
        xa = max(x1, x2) + off if off > 0 else min(x1, x2) + off
        ax.plot([x1, xa], [y1, y1], lw=0.35, c=DIM); ax.plot([x2, xa], [y2, y2], lw=0.35, c=DIM)
        ax.annotate("", (xa, y1), (xa, y2), arrowprops=dict(arrowstyle="<|-|>", lw=0.5, color=DIM, mutation_scale=5))
        t = text or f"{abs(y2 - y1):.1f}".rstrip("0").rstrip(".")
        ax.text(xa + (1.5 if off > 0 else -1.5), (y1 + y2) / 2, t, rotation=90, va="center", ha="left" if off > 0 else "right", fontsize=fs, color=DIM)
    else:
        ya = max(y1, y2) + off if off > 0 else min(y1, y2) + off
        ax.plot([x1, x1], [y1, ya], lw=0.35, c=DIM); ax.plot([x2, x2], [y2, ya], lw=0.35, c=DIM)
        ax.annotate("", (x1, ya), (x2, ya), arrowprops=dict(arrowstyle="<|-|>", lw=0.5, color=DIM, mutation_scale=5))
        t = text or f"{abs(x2 - x1):.1f}".rstrip("0").rstrip(".")
        ax.text((x1 + x2) / 2, ya + (1.2 if off > 0 else -1.2), t, ha="center", va="bottom" if off > 0 else "top", fontsize=fs, color=DIM)


def note(ax, x, y, s, fs=6.5, **kw):
    ax.text(x, y, s, fontsize=fs, color=INK, va="top", **kw)


def draw_dxf(ax, path):
    doc = ezdxf.readfile(path)
    cfg = Configuration(color_policy=ColorPolicy.BLACK, lineweight_scaling=0.5, min_lineweight=0.1)
    Frontend(RenderContext(doc), MatplotlibBackend(ax), config=cfg).draw_layout(doc.modelspace(), finalize=False)


def mm_axes(fig, x, y, w, h, xlim, ylim):
    """assi in mm sulla pagina (x, y, w, h in mm di foglio), con scala uguale"""
    ax = fig.add_axes([x / 420, y / 297, w / 420, h / 297])
    ax.set_xlim(*xlim); ax.set_ylim(*ylim); ax.set_aspect("equal"); ax.axis("off")
    return ax


TOTAL = 6


def page_assembly(pdf):
    fig, fr = sheet("Assieme e controllo ingombri", 1, TOTAL, "—")
    for i, (v, box) in enumerate((("front", (14, 140, 250, 143)), ("rear", (14, 40, 160, 100)), ("section", (176, 40, 160, 100)))):
        img = plt.imread(HERE / "render" / f"render_{v}.png")
        ax = fig.add_axes([box[0] / 420, box[1] / 297, box[2] / 420, box[3] / 297]); ax.imshow(img); ax.axis("off")
    fr.text(16, 137, "Vista anteriore", fontsize=7, color="#555"); fr.text(16, 37, "Vista posteriore (passanti formato D)", fontsize=7, color="#555")
    fr.text(178, 37, "Sezione a X = 298 mm (attraverso Raspberry Pi e Pisound)", fontsize=7, color="#555")
    rep = json.loads((OUT / "ingombri.json").read_text())
    rows = [("Ingombro (L x P)", f"{W:.0f} x {D:.0f} mm"), ("Altezza fianchi davanti / dietro", f"{CHEEK_H_FRONT+PANEL_T:.0f} / {CHEEK_H_REAR+PANEL_T:.0f} mm (+4 piedini)"),
            ("Pendenza pannello", f"{rep['pendenza_gradi']}°"), ("Tasti", "46 × Kailh Choc v1 su prese hot-swap"),
            ("Sporgenza tasti dal pannello", f"{rep['sporgenza_tasto_mm']} mm (corsa 3 mm)"),
            ("Albero encoder sopra il pannello", f"{rep['sporgenza_albero_encoder_mm']} mm")]
    d = rep["distanze_minime_mm"]
    rows += [("— Distanze minime verificate sul modello 3D —", ""),
             ("Pi + Pisound ↔ circuito di controllo", f"{d['pi_vs_circuito']} mm"),
             ("Pi ↔ connettori posteriori", f"{d['pi_vs_connettori_posteriori']} mm"),
             ("Display ↔ vassoio", f"{d['display_vs_vassoio']} mm"), ("Staffa display ↔ vassoio", f"{d['staffa_vs_vassoio']} mm"),
             ("Circuito ↔ vassoio", f"{d['circuito_vs_vassoio']} mm"), ("Connettori ↔ apertura posteriore", f"{d['connettori_vs_vassoio']} mm")]
    y = 280
    fr.text(272, y, "Dati principali", fontsize=9, weight="bold", color=INK); y -= 7
    for a, b in rows:
        fr.text(272, y, a, fontsize=7, color=INK if b else "#555", style="normal" if b else "italic"); fr.text(406, y, b, fontsize=7, ha="right", color=INK); y -= 5.2
    y -= 3
    fr.text(272, y, "Parti e fornitori", fontsize=9, weight="bold", color=INK); y -= 7
    for a, b in (("Pannello superiore 2 mm, piastra posteriore", "Schaeffer AG (DXF)"), ("Vassoio lamiera 1,5 mm con PEM", "Laserhub / Protolabs (STEP)"),
                 ("Fianchi alluminio 8 mm fresati", "Xometry / Protolabs (STEP)"), ("Staffa display 2 mm", "Laserhub (DXF)"),
                 ("Tasti stampati in resina", "JLC3DP / Craftcloud (STL)"), ("Circuito di controllo", "Eurocircuits / Aisler (DXF + Gerber)")):
        fr.text(272, y, a, fontsize=7, color=INK); fr.text(406, y, b, fontsize=7, ha="right", color="#555"); y -= 5.2
    pdf.savefig(fig); plt.close(fig)


def page_panel(pdf):
    fig, fr = sheet("Pannello superiore — alluminio 2 mm anodizzato nero, incisioni laser", 2, TOTAL, "1:1,4")
    ax = mm_axes(fig, 25, 45, 300, 230, (-30, W + 15), (-25, PANEL_L + 20))
    draw_dxf(ax, str(OUT / "pannello_superiore.dxf"))
    dim(ax, (0, 0), (W, 0), -12)
    dim(ax, (W, 0), (W, PANEL_L), 8, vertical=True, text=f"{PANEL_L:.2f}")
    cx, cy = L2P(*DISPLAY_CENTER)
    dim(ax, (cx - DISPLAY_WINDOW[0] / 2, cy + DISPLAY_WINDOW[1] / 2), (cx + DISPLAY_WINDOW[0] / 2, cy + DISPLAY_WINDOW[1] / 2), 6, text=f"{DISPLAY_WINDOW[0]:.0f} finestra")
    dim(ax, (cx - DISPLAY_WINDOW[0] / 2, cy - DISPLAY_WINDOW[1] / 2), (cx - DISPLAY_WINDOW[0] / 2, cy + DISPLAY_WINDOW[1] / 2), -6, vertical=True, text=f"{DISPLAY_WINDOW[1]:.0f}")
    t0, t1 = L2P(*TRIG_KEYS[0]), L2P(*TRIG_KEYS[1])
    dim(ax, (t0[0], t0[1] - 8), (t1[0], t1[1] - 8), -14, text="19 passo")
    k0, k1 = L2P(*PAGE_KEYS[0]), L2P(*PAGE_KEYS[1])
    dim(ax, (k0[0], k0[1] + 8), (k1[0], k1[1] + 8), 4, text="20 passo")
    dim(ax, (0, t0[1]), (t0[0], t0[1]), 0.01, text=f"{t0[0]:.1f}")
    note(ax, -28, -16, "Rosso = TAGLIO · blu = INCISIONE (profondità 0,1–0,2 mm) · verde = prigionieri PEM pressati sul retro (FHS-M3-15 circuito, FHS-M3-8 display) · magenta = svasati M3 90°", fs=6)
    # legenda a destra
    x = 333; y = 270
    fr.text(x, y, "Lavorazioni", fontsize=9, weight="bold"); y -= 7
    for s in (f"46 fori tasto {KEY_HOLE} x {KEY_HOLE} R1,5", f"8 + 1 fori encoder Ø{ENC_HOLE}", f"1 foro volume Ø{POT_HOLE}",
              f"Finestra display {DISPLAY_WINDOW[0]:.0f} x {DISPLAY_WINDOW[1]:.0f} R2", "14 fori svasati M3 (4+4 fianchi, 3+3 vassoio)",
              f"{len(PCB_STUDS)} prigionieri FHS-M3-15 + {len(DISPLAY_STUDS)} FHS-M3-8", "Logo DRUMMATIC: font Michroma (OFL),", "   altezza maiuscole 8 mm, spaziatura 1,2 mm",
              "Diciture: Michroma 2,2 mm / numeri 1,8 mm", "Finitura: anodizzazione nera opaca", "Fornitore: Schaeffer AG (Front Panel Designer",
              "   importa il DXF; layer = lavorazione)"):
        fr.text(x, y, s, fontsize=6.6); y -= 5
    pdf.savefig(fig); plt.close(fig)


def page_cheek(pdf):
    fig, fr = sheet("Fianchi sinistro e destro — alluminio 6082 da 8 mm, fresati, anodizzati neri", 3, TOTAL, "1:1")
    ax = mm_axes(fig, 25, 80, 260, 190, (-25, D + 25), (-30, CHEEK_H_REAR + 60))
    prof = [(0, 0), (D, 0), (D, CHEEK_H_REAR), (0, CHEEK_H_FRONT)]
    ax.add_patch(MPoly(prof, closed=True, fc="#eceff1", ec=INK, lw=0.9))
    for v in PANEL_SIDE_SCREWS_V:
        y = v * math.cos(SLOPE); z = CHEEK_H_FRONT + v * math.sin(SLOPE)
        nx, nz = math.sin(SLOPE), -math.cos(SLOPE)
        ax.plot([y, y + 10 * nx], [z, z + 10 * nz], lw=2.5, c="#90a4ae", solid_capstyle="butt")
        ax.text(y, z + 3, f"M3 prof. 10\nv = {v}", fontsize=5.5, ha="center", color=INK)
    for y in CHEEK_SIDE_SCREWS_Y:
        ax.add_patch(Circle((y, 6), 1.7, fill=False, ec=INK, lw=0.6)); ax.add_patch(Circle((y, 6), 3.2, fill=False, ec=INK, lw=0.4, ls="--"))
    dim(ax, (0, 0), (D, 0), -10)
    dim(ax, (0, 0), (0, CHEEK_H_FRONT), -8, vertical=True)
    dim(ax, (D, 0), (D, CHEEK_H_REAR), 8, vertical=True)
    for i, y in enumerate(CHEEK_SIDE_SCREWS_Y):
        dim(ax, (0 if i == 0 else CHEEK_SIDE_SCREWS_Y[i - 1], 6), (y, 6), -20 - 0 * i)
    dim(ax, (CHEEK_SIDE_SCREWS_Y[0], 0), (CHEEK_SIDE_SCREWS_Y[0], 6), 6, vertical=True, text="6")
    ax.text(D / 2, CHEEK_H_FRONT + 42, f"pendenza {math.degrees(SLOPE):.2f}° · spigoli R2", ha="center", fontsize=7, color=INK)
    x = 300; y = 270
    fr.text(x, y, "Note di lavorazione", fontsize=9, weight="bold"); y -= 7
    for s in ("Quantità: 1 sinistro + 1 destro (speculari: lo", "  svaso dei fori laterali è sul lato esterno).",
              "Bordo superiore: 4 fori filettati M3, profondità 10,", "  perpendicolari alla pendenza, al centro dello spessore.",
              "Lato esterno: 3 fori Ø3,4 svasati 90° Ø6,4 a Z = 6", "  per viti M3 x 8 nei dadi PEM del vassoio.",
              "Materiale: EN AW-6082 T6, 8 mm.", "Finitura: sabbiatura + anodizzazione nera.", "Spigoli esterni raggiati R2, sbavatura.",
              "File: fianco_sx.step, fianco_dx.step", "Fornitore: Xometry / Protolabs (CNC)"):
        fr.text(x, y, s, fontsize=6.6); y -= 5
    pdf.savefig(fig); plt.close(fig)


def page_tray(pdf):
    fig, fr = sheet("Vassoio — lamiera di alluminio 5754 da 1,5 mm piegata, con inserti PEM", 4, TOTAL, "1:2")
    # sezione laterale
    ax = mm_axes(fig, 20, 175, 230, 100, (-20, D + 20), (-25, CHEEK_H_REAR + 15))
    fl = FLANGE
    pts = [(fl * math.cos(SLOPE), CHEEK_H_FRONT + fl * math.sin(SLOPE)), (0, CHEEK_H_FRONT), (0, 0), (D, 0), (D, CHEEK_H_REAR),
           (D - fl * math.cos(SLOPE), CHEEK_H_REAR - fl * math.sin(SLOPE))]
    ax.plot(*zip(*pts), lw=1.6, c=INK)
    ax.add_patch(Rectangle((D - 3, REAR_OPEN_Z), 6, REAR_OPEN_H, fc="white", ec=DIM, lw=0.6, ls="--"))
    dim(ax, (0, 0), (D, 0), -10); dim(ax, (0, 0), (0, CHEEK_H_FRONT), -8, vertical=True); dim(ax, (D, 0), (D, CHEEK_H_REAR), 10, vertical=True)
    dim(ax, (D, REAR_OPEN_Z), (D, REAR_OPEN_Z + REAR_OPEN_H), 4, vertical=True)
    ax.add_patch(Rectangle((PI_Y0, SHEET_T + PI_STANDOFF), PI_L, PI_STACK_H, fc="#c8e6c9", ec="#2e7d32", lw=0.5, ls=":"))
    ax.text(PI_Y0 + PI_L / 2, SHEET_T + PI_STANDOFF + PI_STACK_H / 2, "Pi 5 + Pisound\n(ingombro)", ha="center", va="center", fontsize=6, color="#2e7d32")
    ax.text(D / 2, CHEEK_H_REAR + 8, "Sezione laterale (alette superiori inclinate come il pannello)", ha="center", fontsize=7)
    # vista dall'alto
    ax2 = mm_axes(fig, 20, 40, 230, 130, (-15, W + 10), (-15, D + 10))
    ax2.add_patch(Rectangle((TRAY_X0, 0), INNER_W, D, fill=False, ec=INK, lw=0.9))
    for (x, y) in VENT_SLOTS:
        ax2.add_patch(FancyBboxPatch((x + TRAY_X0 - 8 + 1.5, y - 1.5), 13, 3, boxstyle="round,pad=0,rounding_size=1.5", fill=False, ec=INK, lw=0.5))
    for hx, hy in PI_HOLES:
        ax2.add_patch(Circle((PI_X0 + hx, PI_Y0 + hy), 2.1, fc=ACC, ec=INK, lw=0.4))
    ax2.add_patch(Rectangle((PI_X0, PI_Y0), PI_W, PI_L, fill=False, ec="#2e7d32", lw=0.5, ls=":"))
    for fx, fy in FEET:
        ax2.add_patch(Circle((fx, fy), 1.7, fc="white", ec=INK, lw=0.5)); ax2.add_patch(Circle((fx, fy), 6, fill=False, ec="#999", lw=0.3, ls="--"))
    dim(ax2, (TRAY_X0, 0), (TRAY_X1, 0), -8); dim(ax2, (TRAY_X1, 0), (TRAY_X1, D), 6, vertical=True)
    dim(ax2, (TRAY_X0, PI_Y0), (PI_X0 + PI_HOLES[0][0], PI_Y0 + PI_HOLES[0][1]), 0.01, text=f"{PI_X0 + 3.5:.1f}")
    dim(ax2, (PI_X0 + 3.5, 0), (PI_X0 + 3.5, PI_Y0 + 3.5), -60, vertical=True, text=f"{PI_Y0 + 3.5:.1f}")
    ax2.text(W / 2, D + 4, "Fondo visto dall'alto: asole di ventilazione, distanziali Pi, fori piedini", ha="center", fontsize=7)
    x = 262; y = 270
    fr.text(x, y, "Inserti PEM (pressati dal fornitore)", fontsize=9, weight="bold"); y -= 7
    for s in ("4 × SO-M2.5-5 sul fondo (Raspberry Pi 5)", "6 × CLS-M3-1 nelle alette (viti del pannello)", "6 × CLS-M3-1 nei bordi laterali (viti dai fianchi)",
              "6 × CLS-M3-1 sulla parete posteriore (piastra)", "", "Lavorazioni", "Apertura posteriore 310 x 44 (Z 13–57), centrata",
              "11 asole 16 x 3 sul fondo davanti", "4 fori Ø3,4 per piedini adesivi o a vite", "Alette: 10 mm, piegate a 97,4° (pendenza pannello)",
              "Bordi laterali: 12 mm a 90°", "Raggio interno di piega 2 mm (R = 1,3 t)", "Finitura: verniciatura a polvere nera opaca",
              "", "File: vassoio.step (il fornitore ricava lo sviluppo)", "Fornitore: Laserhub / Protolabs sheet metal"):
        fr.text(x, y, s, fontsize=6.6, weight="bold" if s == "Lavorazioni" else "normal"); y -= 5
    pdf.savefig(fig); plt.close(fig)


def page_rear(pdf):
    fig, fr = sheet("Piastra posteriore (2 mm, Schaeffer) e staffa display (2 mm, taglio laser)", 5, TOTAL, "1:1,3")
    ax = mm_axes(fig, 20, 165, 300, 110, (-15, REAR_PLATE_W + 15), (-20, REAR_PLATE_H + 12))
    draw_dxf(ax, str(OUT / "piastra_posteriore.dxf"))
    dim(ax, (0, 0), (REAR_PLATE_W, 0), -12); dim(ax, (REAR_PLATE_W, 0), (REAR_PLATE_W, REAR_PLATE_H), 6, vertical=True)
    xs = [REAR_FAN_U] + [u for _, u in REAR_CONNECTORS]
    for a, b in zip(xs[:-1], xs[1:]):
        dim(ax, (a, REAR_CONN_V + 13), (b, REAR_CONN_V + 13), REAR_PLATE_H - REAR_CONN_V - 9)
    dim(ax, (0, 0), (0, REAR_CONN_V), -6, vertical=True)
    ax.text(REAR_PLATE_W / 2, -18, "Vista dal retro. Passanti formato D: foro Ø24 + 2 fori Ø3,2 in diagonale (±9,5 / ±12)", ha="center", fontsize=6.5)
    ax2 = mm_axes(fig, 20, 40, 190, 120, (-15, 185), (-15, 125))
    draw_dxf(ax2, str(OUT / "staffa_display.dxf"))
    dim(ax2, (0, 0), (170, 0), -8); dim(ax2, (170, 0), (170, 112), 6, vertical=True)
    ax2.text(85, 118, "Staffa display: fori Ø9 sui distanziali M3 (regolazione ±3 mm con rondelle larghe)", ha="center", fontsize=6.5)
    x = 225; y = 150
    fr.text(x, y, "Passanti sulla piastra (formato D, 2 viti M3)", fontsize=9, weight="bold"); y -= 7
    for s in ("USB-C 5V: Neutrik NAUSB-C-W (prolunga USB-C)", "USB HOST: Neutrik NAUSB-W (USB-A ↔ USB-A)", "MIDI OUT / IN: Neutrik NADIN5 o NAMIDI5",
              "OUT L/R, IN L/R: Neutrik NAJTD2 (jack 6,35 ↔ 6,35)", "Ventola: Noctua NF-A4x10 5V + griglia nella piastra",
              "Tutti i cavi interni sono spinati: nessuna saldatura.", "", "Staffa display", "Alluminio 2 mm, taglio laser, verniciata nera",
              "4 distanziali esagonali M3 F/F 16 mm sui prigionieri", "Finestra 90 x 40 per i cavi del display"):
        fr.text(x, y, s, fontsize=6.6, weight="bold" if s == "Staffa display" else "normal"); y -= 5
    pdf.savefig(fig); plt.close(fig)


def page_key(pdf):
    fig, fr = sheet("Tasto per Kailh Choc v1 — stampa 3D in resina (SLA), set di prova + 46 pezzi", 6, TOTAL, "4:1")
    h = keycap_height()
    ax = mm_axes(fig, 30, 120, 150, 150, (-12, 12), (-6, 12))
    ax.add_patch(FancyBboxPatch((-KEYCAP / 2, 0), KEYCAP, h, boxstyle="round,pad=0,rounding_size=0.5", fc="#eceff1", ec=INK, lw=0.9))
    for x in (-2.85, 2.85):
        ax.add_patch(Rectangle((x - 0.6, -3), 1.2, 3, fc="#cfd8dc", ec=INK, lw=0.7))
    dim(ax, (-KEYCAP / 2, h), (KEYCAP / 2, h), 2.5); dim(ax, (KEYCAP / 2, 0), (KEYCAP / 2, h), 1.5, vertical=True, text=f"{h:.1f}")
    dim(ax, (-2.85, -3), (2.85, -3), -1.5, text="5,70 interasse"); dim(ax, (2.85 + 0.6, -3), (2.85 + 0.6, 0), 3, vertical=True, text="3,0")
    ax.text(0, 10.5, "Vista frontale", ha="center", fontsize=7)
    ax2 = mm_axes(fig, 200, 120, 150, 150, (-12, 12), (-12, 12))
    ax2.add_patch(FancyBboxPatch((-KEYCAP / 2, -KEYCAP / 2), KEYCAP, KEYCAP, boxstyle="round,pad=0,rounding_size=1.2", fc="#eceff1", ec=INK, lw=0.9))
    for x in (-2.85, 2.85):
        ax2.add_patch(Rectangle((x - 0.6, -1.5), 1.2, 3.0, fc="#cfd8dc", ec=INK, lw=0.7, ls="--"))
    dim(ax2, (-0.6 - 2.85, 1.5), (0.6 - 2.85, 1.5), 4, text="1,20"); dim(ax2, (2.85 + 0.6, -1.5), (2.85 + 0.6, 1.5), 5, vertical=True, text="3,00")
    ax2.text(0, 10.5, "Vista da sotto", ha="center", fontsize=7)
    x = 30; y = 100
    fr.text(x, y, "Note", fontsize=9, weight="bold"); y -= 7
    for s in (f"Il tasto sporge {KEY_PROUD} mm dal pannello a riposo; con la corsa di 3 mm rientra 0,5 mm sotto il bordo del foro.",
              "Le misure dei piedini sono quelle pubblicate per Choc v1: prima di ordinare i 46 pezzi stampa il set di prova (4 tasti",
              "con piedini 1,15 / 1,20 / 1,25 / 1,30 mm, riconoscibili da 1–4 tacche sul bordo: tasti_prova_choc.stl) e scegli quello che entra senza gioco.",
              "Materiale consigliato: resina tenace (JLC3DP 8228 / Craftcloud \"Tough\"), colore bianco o grigio chiaro; leggera concavità superiore 0,35 mm.",
              "File: tasto_choc.stl (pezzo singolo) e tasto_choc.step."):
        fr.text(x, y, s, fontsize=7); y -= 6
    pdf.savefig(fig); plt.close(fig)


if __name__ == "__main__":
    with PdfPages(OUT / "Drummatic-Tavole-Contenitore.pdf") as pdf:
        page_assembly(pdf); page_panel(pdf); page_cheek(pdf); page_tray(pdf); page_rear(pdf); page_key(pdf)
    print("ok")
