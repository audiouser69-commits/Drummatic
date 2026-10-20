"""Genera il circuito di controllo Drummatic (KiCad 7) dalle posizioni del pannello (hw/geom.py).
Lato superiore (verso il pannello): switch Choc (non saldati), encoder e potenziometro THT.
Lato inferiore: prese hot-swap, LED SK6812MINI-E inversi, diodi, Pico 2, logica, passivi.
Coordinate: X = x del layout, Y = y del layout (dal bordo posteriore) + OFFSET.
"""
import sys, json, math
from pathlib import Path
sys.path.insert(0, "/home/claude/hw")
import pcbnew
from geom import *

HERE = Path(__file__).parent
KLIB = "/usr/share/kicad/footprints/"
MYLIB = str(HERE / "Drummatic.pretty")
PICOLIB = str(HERE / "lib/pico/RP-Pico Libraries/MCU_RaspberryPi_and_Boards.pretty")
OFF = (20.0, 20.0)
MM = pcbnew.FromMM

board = pcbnew.BOARD()
nets = {}
parts = []          # per BOM / CPL


def P(x, y):
    return pcbnew.VECTOR2I(MM(x + OFF[0]), MM(y + OFF[1]))


def net(name):
    if name not in nets:
        n = pcbnew.NETINFO_ITEM(board, name); board.Add(n); nets[name] = n
    return nets[name]


def place(lib, name, ref, value, x, y, rot=0.0, bottom=False, conn=None, mpn="", supplier="", dnp=False):
    fp = pcbnew.FootprintLoad(lib, name)
    assert fp is not None, (lib, name)
    fp.SetReference(ref); fp.SetValue(value)
    board.Add(fp)
    fp.SetPosition(P(x, y))
    if bottom:
        fp.Flip(P(x, y), False)          # ribalta sul lato inferiore (specchio in Y)
    fp.SetOrientationDegrees(rot)
    if conn:
        for pad in fp.Pads():
            n = pad.GetNumber()
            if n in conn and conn[n]:
                pad.SetNet(net(conn[n]))
    parts.append(dict(ref=ref, value=value, fp=name, x=x, y=y, rot=rot, side="bottom" if bottom else "top",
                      mpn=mpn, supplier=supplier, dnp=dnp))
    return fp


def keepout(x0, y0, x1, y1, layers=None):
    """area senza piste né via (attorno ai fori dei LED: il router non vede i ritagli interni)"""
    z = pcbnew.ZONE(board)
    z.SetIsRuleArea(True); z.SetDoNotAllowTracks(True); z.SetDoNotAllowVias(True); z.SetDoNotAllowCopperPour(False)
    z.SetDoNotAllowPads(False); z.SetDoNotAllowFootprints(False)
    if layers is None:
        layers = pcbnew.LSET(); layers.AddLayer(pcbnew.F_Cu); layers.AddLayer(pcbnew.B_Cu)
    z.SetLayerSet(layers)
    ol = z.Outline(); ol.NewOutline()
    for px, py in ((x0, y0), (x1, y0), (x1, y1), (x0, y1)):
        ol.Append(MM(px + OFF[0]), MM(py + OFF[1]))
    board.Add(z)


# ------------------------------------------------------------------ contorno
def outline():
    x0, y0, x1, y1 = PCB_RECT; nx0, ny0, nx1, ny1 = PCB_NOTCH
    pts = [(x0, y0), (nx0, y0), (nx0, ny1), (nx1, ny1), (nx1, y0), (x1, y0), (x1, y1), (x0, y1)]
    # fascia di 0,5 mm lungo i bordi senza piste: rispetta la distanza rame-bordo di fresatura
    e = 0.5
    for (ax, ay), (bx, by) in zip(pts, pts[1:] + pts[:1]):
        keepout(min(ax, bx) - e, min(ay, by) - e, max(ax, bx) + e, max(ay, by) + e)
    for a, b in zip(pts, pts[1:] + pts[:1]):
        s = pcbnew.PCB_SHAPE(board); s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(P(*a)); s.SetEnd(P(*b)); s.SetLayer(pcbnew.Edge_Cuts); s.SetWidth(MM(0.1)); board.Add(s)


# ------------------------------------------------------------------ matrice tasti
ROWS = [f"ROW{i}" for i in range(7)]
COLS = [f"COL{i}" for i in range(8)]
KEYS = []   # (nome, posizione, riga, colonna)
for i, k in enumerate(PAGE_KEYS): KEYS.append((PAGE_NAMES[i], k, 0, i))
for i, k in enumerate(FUNC_KEYS): KEYS.append((FUNC_NAMES[i], k, 1, i))
for i, k in enumerate(VOICE_KEYS): KEYS.append((f"VOICE{i+1}", k, 2, i))
for i, k in enumerate(TRANSPORT_KEYS): KEYS.append((TRANSPORT_NAMES[i], k, 3, i))
for i, k in enumerate(ARROW_KEYS): KEYS.append((["LEFT", "UP", "DOWN", "RIGHT"][i], k, 3, 3 + i))
for i, k in enumerate(TRIG_KEYS): KEYS.append((f"TRIG{i+1}", k, 4 + i // 8, i % 8))
ENCS = [(f"ENC{i+1}", e) for i, e in enumerate(ENCODERS)] + [("DATA", DATA)]
ENC_MATRIX = {name: (6, i) for i, (name, _) in enumerate(ENCS[:8])}
ENC_MATRIX["DATA"] = (1, 7)

# ordine della catena LED: serpentina
LED_ORDER = ([n for n, *_ in KEYS if n in PAGE_NAMES + FUNC_NAMES]
             + list(reversed(["VOICE%d" % (i + 1) for i in range(8)] + TRANSPORT_NAMES + ["LEFT", "UP", "DOWN", "RIGHT"]))
             + ["TRIG%d" % (i + 1) for i in range(16)])
assert len(LED_ORDER) == 46 and len(set(LED_ORDER)) == 46


def build():
    outline()
    keypos = {n: k for n, k, *_ in KEYS}
    # --- tasti: presa hot-swap, diodo, LED, condensatore
    for idx, (name, (x, y), r, c) in enumerate(KEYS):
        n = idx + 1
        place(MYLIB, "Choc_v1_Hotswap", f"SW{n}", f"Choc v1 {name}", x, y, 0, True,
              {"1": COLS[c], "2": f"K{n}"}, "Kailh CPG135001S30 (presa hot-swap)", "splitkb / TME")
        place(KLIB + "Diode_SMD.pretty", "D_SOD-123", f"D{n}", "1N4148W", x - 6.6, y + 4.7, 90, True,
              {"1": ROWS[r], "2": f"K{n}"}, "1N4148W-7-F (Diodes Inc.)", "TME")
    for i, name in enumerate(LED_ORDER):
        x, y = keypos[name]
        din = "LED_DIN0" if i == 0 else f"LED_D{i}"
        dout = f"LED_D{i+1}" if i < 45 else ""
        place(MYLIB, "SK6812MINI-E_Reverse", f"LED{i+1}", "SK6812MINI-E", x, y + 4.7, 0, True,
              {"1": "+5V", "2": dout, "3": din, "4": "GND"}, "SK6812MINI-E (Opsco)", "splitkb / LCSC")
        keepout(x - 2.0, y + 4.7 - 1.95, x + 2.0, y + 4.7 + 1.95)
        place(KLIB + "Capacitor_SMD.pretty", "C_0603_1608Metric", f"C{i+1}", "100nF", x + 5.9, y + 4.7, 90, True,
              {"1": "+5V", "2": "GND"}, "GRM188R71H104KA93D", "TME")
    # --- encoder (lato superiore): A/B al 74HC165 con pull-up, C a massa, pulsante nella matrice
    for i, (name, (x, y)) in enumerate(ENCS):
        rot = 90 if name == "DATA" else 0
        # l'origine del footprint è il pin A, il centro dell'albero è a (+7.5, +2.5)
        if rot == 0:
            ox, oy = x - 7.5, y - 2.5
        else:   # 90°: (dx, dy) -> (dy, -dx)
            ox, oy = x - 2.5, y + 7.5
        r, c = ENC_MATRIX[name]
        place(KLIB + "Rotary_Encoder.pretty", "RotaryEncoder_Alps_EC11E-Switch_Vertical_H20mm", f"ENC{i+1}",
              f"EC11E {name}", ox, oy, rot, False,
              {"A": f"{name}_A", "B": f"{name}_B", "C": "GND", "S1": COLS[c], "S2": f"KE{i+1}", "MP": "GND"},
              "Alps EC11E15244G1 (o EC11E15244B2)", "TME")
        dx, dy = (x - 9.0, y + 9.5) if rot == 0 else (x - 9.5, y + 8.0)
        place(KLIB + "Diode_SMD.pretty", "D_SOD-123", f"D{60+i}", "1N4148W", dx, dy, 0, True,
              {"1": ROWS[r], "2": f"KE{i+1}"}, "1N4148W-7-F (Diodes Inc.)", "TME")
        # pull-up 10k
        if rot == 0:
            ra, rb = (x - 11.5, y - 3.0), (x - 11.5, y + 3.0)
        else:
            ra, rb = (x - 3.0, y + 11.5), (x + 3.0, y + 11.5)
        place(KLIB + "Resistor_SMD.pretty", "R_0603_1608Metric", f"R{10+2*i}", "10k", *ra, 0, True,
              {"1": "+3V3", "2": f"{name}_A"}, "RC0603FR-0710KL", "TME")
        place(KLIB + "Resistor_SMD.pretty", "R_0603_1608Metric", f"R{11+2*i}", "10k", *rb, 0, True,
              {"1": "+3V3", "2": f"{name}_B"}, "RC0603FR-0710KL", "TME")
        # filtro RC consigliato da Alps: 10 nF verso massa su A e B
        if rot == 0:
            ca, cb = (x - 11.5, y - 6.0), (x - 11.5, y + 6.0)
        else:
            ca, cb = (x - 3.0, y + 14.0), (x + 3.0, y + 14.0)
        place(KLIB + "Capacitor_SMD.pretty", "C_0603_1608Metric", f"C{80+2*i}", "10nF", *ca, 0, True,
              {"1": f"{name}_A", "2": "GND"}, "GRM188R71H103KA01D", "TME")
        place(KLIB + "Capacitor_SMD.pretty", "C_0603_1608Metric", f"C{81+2*i}", "10nF", *cb, 0, True,
              {"1": f"{name}_B", "2": "GND"}, "GRM188R71H103KA01D", "TME")
    # --- potenziometro volume (lato superiore)
    vx, vy = VOLUME
    place(KLIB + "Potentiometer_THT.pretty", "Potentiometer_Alps_RK09K_Single_Vertical", "RV1", "10k lin VOLUME",
          vx - 7.5, vy - 2.5, 0, False, {"1": "GND", "2": "POT_W", "3": "ADC_VREF", "MP": "GND"},
          "Alps RK09K1130A5R (10k B)", "TME")
    place(KLIB + "Resistor_SMD.pretty", "R_0603_1608Metric", "R1", "1k", vx - 7.5, vy + 9.0, 0, True,
          {"1": "POT_W", "2": "ADC0"}, "RC0603FR-071KL", "TME")
    place(KLIB + "Capacitor_SMD.pretty", "C_0603_1608Metric", "C60", "100nF", vx - 3.0, vy + 9.0, 0, True,
          {"1": "ADC0", "2": "GND"}, "GRM188R71H104KA93D", "TME")

    # --- Raspberry Pi Pico 2 sul lato inferiore, fra i tasti VOICE e TRIG (lontano dal Raspberry Pi), USB verso destra
    px, py = 145.0, 185.0
    pico = {1: "ROW0", 2: "ROW1", 3: "GND", 4: "ROW2", 5: "ROW3", 6: "ROW4", 7: "ROW5", 8: "GND", 9: "ROW6",
            10: "COL0", 11: "COL1", 12: "COL2", 13: "GND", 14: "COL3", 15: "COL4", 16: "COL5", 17: "COL6",
            18: "GND", 19: "COL7", 20: "LED_3V3", 21: "SR_LOAD", 22: "SR_CLK", 23: "GND", 24: "SR_DATA",
            28: "GND", 31: "ADC0", 33: "GND", 35: "ADC_VREF", 36: "+3V3", 38: "GND", 40: "VBUS"}
    bl = pcbnew.LSET(); bl.AddLayer(pcbnew.B_Cu)
    keepout(px - 26.0, py - 6.9, px + 26.0, py + 6.9, bl)      # sotto il Pico: niente piste sul lato inferiore
    keepout(px + 19.5, py - 4.0, px + 25.5, py + 4.0)          # fori di fissaggio del connettore USB del Pico
    place(PICOLIB, "RPi_Pico_SMD_TH", "U1", "Raspberry Pi Pico 2", px, py, 90, True,
          {str(k): v for k, v in pico.items()}, "Raspberry Pi Pico 2 (SC1631)", "Kiwi / TME")
    # --- 3 x 74HC165: U2 legge ENC1-4, U3 ENC5-8, U4 DATA
    sr = {"U2": ["ENC1_A", "ENC1_B", "ENC2_A", "ENC2_B", "ENC3_A", "ENC3_B", "ENC4_A", "ENC4_B"],
          "U3": ["ENC5_A", "ENC5_B", "ENC6_A", "ENC6_B", "ENC7_A", "ENC7_B", "ENC8_A", "ENC8_B"],
          "U4": ["DATA_A", "DATA_B", "GND", "GND", "GND", "GND", "GND", "GND"]}
    ser = {"U2": "SR_S1", "U3": "SR_S2", "U4": "GND"}
    qh = {"U2": "SR_DATA", "U3": "SR_S1", "U4": "SR_S2"}
    pos = {"U2": (222.0, 66.0), "U3": (241.0, 66.0), "U4": (222.0, 81.0)}
    inpins = ["11", "12", "13", "14", "3", "4", "5", "6"]   # A..H
    for u, ins in sr.items():
        conn = {"1": "SR_LOAD", "2": "SR_CLK", "15": "GND", "8": "GND", "16": "+3V3", "10": ser[u], "9": qh[u], "7": ""}
        conn.update({p: n for p, n in zip(inpins, ins)})
        x, y = pos[u]
        place(KLIB + "Package_SO.pretty", "SOIC-16_3.9x9.9mm_P1.27mm", u, "74HC165", x, y, 90, True, conn,
              "SN74HC165DR (TI)", "TME")
        place(KLIB + "Capacitor_SMD.pretty", "C_0603_1608Metric", f"C{61+int(u[1])}", "100nF", x, y + 5.0, 0, True,
              {"1": "+3V3", "2": "GND"}, "GRM188R71H104KA93D", "TME")
    # --- alimentazione 5 V per LED e display: dal VBUS del Pico tramite fusibile ripristinabile
    place(KLIB + "Fuse.pretty", "Fuse_1206_3216Metric", "F1", "PTC 1.5A", 106.0, 178.0, 0, True,
          {"1": "VBUS", "2": "+5V"}, "Littelfuse 1206L150THYR", "TME")
    place(KLIB + "Capacitor_SMD.pretty", "CP_Elec_6.3x7.7", "C70", "220uF 10V", 106.0, 190.0, 0, True,
          {"1": "+5V", "2": "GND"}, "Panasonic EEE-FK1A221P", "TME")
    # --- traslatore di livello 3,3 -> 5 V per i dati dei LED
    place(KLIB + "Package_TO_SOT_SMD.pretty", "SOT-23-5", "U5", "74AHCT1G125", 113.0, 185.0, 0, True,
          {"1": "GND", "2": "LED_3V3", "3": "GND", "4": "LED_5V", "5": "+5V"}, "SN74AHCT1G125DBVR (TI)", "TME")
    place(KLIB + "Capacitor_SMD.pretty", "C_0603_1608Metric", "C71", "100nF", 113.0, 189.5, 0, True,
          {"1": "+5V", "2": "GND"}, "GRM188R71H104KA93D", "TME")
    place(KLIB + "Resistor_SMD.pretty", "R_0603_1608Metric", "R2", "330R", 113.0, 181.0, 0, True,
          {"1": "LED_5V", "2": "LED_DIN0"}, "RC0603FR-07330RL", "TME")
    # --- connettore alimentazione display (cavo in dotazione al Touch Display 2)
    place(KLIB + "Connector_PinHeader_2.54mm.pretty", "PinHeader_1x02_P2.54mm_Vertical", "J1", "DISPLAY 5V",
          205.0, 112.0, 90, True, {"1": "+5V", "2": "GND"}, "Header 2 pin 2,54 mm", "TME")
    # --- 3 fiduciali sul lato inferiore (montaggio automatico)
    for i, (x, y) in enumerate([(6.0, 14.0), (334.0, 14.0), (6.0, 218.0)]):
        place(KLIB + "Fiducial.pretty", "Fiducial_1mm_Mask2mm", f"FID{i+1}", "Fiducial", x, y, 0, True, {})
    # --- origine per il file di piazzamento: angolo in basso a sinistra del circuito
    board.GetDesignSettings().SetAuxOrigin(P(PCB_RECT[0], PCB_RECT[3]))
    # --- fori di fissaggio sui prigionieri M3 del pannello
    for i, (x, y) in enumerate(PCB_STUDS):
        place(KLIB + "MountingHole.pretty", "MountingHole_3.2mm_M3_Pad", f"H{i+1}", "M3", x, y, 0, False, {"1": "GND"})


def zones():
    x0, y0, x1, y1 = PCB_RECT; nx0, ny0, nx1, ny1 = PCB_NOTCH
    pts = [(x0, y0), (nx0, y0), (nx0, ny1), (nx1, ny1), (nx1, y0), (x1, y0), (x1, y1), (x0, y1)]
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer); z.SetNet(net("GND"))
        ol = z.Outline(); ol.NewOutline()
        for p in pts:
            ol.Append(MM(p[0] + OFF[0]), MM(p[1] + OFF[1]))
        z.SetLocalClearance(MM(0.3)); z.SetMinThickness(MM(0.25))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        z.SetThermalReliefGap(MM(0.3)); z.SetThermalReliefSpokeWidth(MM(0.4))
        board.Add(z)


def rules():
    ds = board.GetDesignSettings()
    ds.m_TrackMinWidth = MM(0.2); ds.m_MinClearance = MM(0.2); ds.m_ViasMinSize = MM(0.6); ds.m_MinThroughDrill = MM(0.3)
    ds.SetCopperLayerCount(2)


if __name__ == "__main__":
    build()
    zones()
    rules()
    out = HERE / "out"; out.mkdir(exist_ok=True)
    board.SetFileName(str(out / "drummatic_controllo.kicad_pcb"))
    pcbnew.SaveBoard(str(out / "drummatic_controllo_unrouted.kicad_pcb"), board)
    (out / "parts.json").write_text(json.dumps(parts, indent=1))
    (out / "keymap.json").write_text(json.dumps({"keys": [(n, r, c) for n, _, r, c in KEYS],
                                                  "enc_buttons": ENC_MATRIX, "led_order": LED_ORDER,
                                                  "encoders": [n for n, _ in ENCS]}, indent=1))
    print("footprint:", len(board.GetFootprints()), "net:", len(nets))
