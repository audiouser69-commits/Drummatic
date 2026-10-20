"""Drummatic hardware — geometria condivisa (mm).
Assi globali: X sinistra→destra (0 = faccia esterna del fianco sinistro), Y fronte→retro (0 = bordo anteriore),
Z alto (0 = fondo del vassoio). Il pannello superiore ha coordinate proprie (u, v): u lungo X, v lungo la
pendenza a partire dal bordo ANTERIORE.
"""
import math

# ---------- ingombri generali ----------
CHEEK_T = 8.0                 # spessore fianchi in alluminio
INNER_W = 340.0               # larghezza interna fra i fianchi
W = INNER_W + 2 * CHEEK_T     # 356
D = 230.0                     # profondità
CHEEK_H_FRONT = 43.0          # quota del bordo superiore del fianco, davanti
CHEEK_H_REAR = 73.0           # e dietro
PANEL_T = 2.0                 # spessore pannello superiore (alluminio anodizzato)
SLOPE = math.atan((CHEEK_H_REAR - CHEEK_H_FRONT) / D)       # ~7.4°
PANEL_L = D / math.cos(SLOPE)                                # lunghezza lungo la pendenza
SHEET_T = 1.5                 # lamiera del vassoio
FEET_H = 4.0

# ---------- pila sotto il pannello ----------
PCB_T = 1.6
PCB_GAP = 7.5                 # distanziali pannello→circuito (lato inferiore pannello → lato superiore circuito)
ENC_SHAFT = 20.0              # EC11E: albero dal piano di montaggio
SWITCH_STEM_TOP = 8.0         # Choc v1: sommità dello stelo sopra il circuito (da verificare sul campione)
KEY_PROUD = 2.5               # quanto il tasto sporge dal pannello a riposo
KEYCAP = 15.0                 # tasto quadrato 15 x 15
KEY_HOLE = 15.8               # foro nel pannello
ENC_HOLE = 7.0
POT_HOLE = 7.5

def keycap_height():
    """spessore del tasto sopra lo stelo, perché sporga KEY_PROUD dal pannello"""
    panel_top = PCB_GAP + PANEL_T
    return panel_top + KEY_PROUD - SWITCH_STEM_TOP


# ---------- layout del pannello (coordinate del disegno approvato) ----------
# x_l da sinistra della zona 340 mm, y_l dal bordo POSTERIORE (in alto nel disegno)
def L2P(x_l, y_l):
    """layout → coordinate del pannello (u, v): v dal bordo anteriore"""
    return (x_l + CHEEK_T, PANEL_L - (y_l + 1.0))


VOLUME = (25, 38)
DATA = (25, 88)
DISPLAY_CENTER = (115, 63)
DISPLAY_OUTLINE = (143.5, 91.5)        # Touch Display 2 da 5", in orizzontale
DISPLAY_ACTIVE = (110.4, 62.1)
DISPLAY_WINDOW = (113.0, 65.0)         # finestra = area attiva + 1,3 mm per lato
ENCODERS = [(x, y) for y in (50, 96) for x in (213, 246, 279, 312)]

PAGE_KEYS = [(25 + 20 * i, 135) for i in range(8)]
PAGE_NAMES = ["VOICE", "SMPL", "TONE", "MOD", "SEQ", "LANE", "MIX", "PERF"]
FUNC_KEYS = [(190 + 20 * i, 135) for i in range(7)]
FUNC_NAMES = ["FUNC", "MAP", "LOCK", "DADO", "COMMIT", "KIT", "COPY"]
VOICE_KEYS = [(25 + 20 * i, 165) for i in range(8)]
TRANSPORT_KEYS = [(190, 165), (210, 165), (230, 165)]
TRANSPORT_NAMES = ["REC", "PLAY", "STOP"]
ARROW_KEYS = [(260, 165), (280, 165), (300, 165), (320, 165)]
TRIG_KEYS = [(27.5 + 19 * i, 205) for i in range(16)]

ALL_KEYS = PAGE_KEYS + FUNC_KEYS + VOICE_KEYS + TRANSPORT_KEYS + ARROW_KEYS + TRIG_KEYS   # 46

# distanziali filettati M3 sotto il pannello per il circuito di controllo (posizioni libere fra i tasti)
PCB_STUDS = [(10, 63), (203, 69), (329, 69), (10, 150), (177.5, 150), (245, 165), (334, 150),
             (10, 205), (330, 216), (177.5, 216)]
# distanziali per la staffa del display, attorno al suo ingombro
DISPLAY_STUDS = [(36, 16.5), (194, 16.5), (36, 114), (194, 114)]
# zona del circuito di controllo (coordinate layout): rettangolo meno la tacca del display
PCB_RECT = (2.0, 10.0, 338.0, 219.0)
PCB_NOTCH = (30.0, 10.0, 199.0, 120.0)
# logo
LOGO_RIGHT, LOGO_BASE, LOGO_CAP = 330.0, 30.0, 8.0

# viti del pannello: 4 per fianco (nel fianco), 3 davanti e 3 dietro (nelle alette del vassoio)
PANEL_SIDE_SCREWS_V = [16, 80, 150, 214]          # v dal bordo anteriore
PANEL_FRONT_SCREWS_U = [70, 178, 286]
PANEL_REAR_SCREWS_U = [70, 178, 286]
PANEL_EDGE_SCREW_INSET = 5.0

# ---------- vassoio ----------
FLANGE = 10.0                 # alette superiori anteriore/posteriore
SIDE_LIP_H = 12.0
TRAY_X0, TRAY_X1 = CHEEK_T, CHEEK_T + INNER_W
FRONT_WALL_H = CHEEK_H_FRONT          # la parete arriva sotto il pannello
REAR_WALL_H = CHEEK_H_REAR

# apertura nella parete posteriore per la piastra connettori
REAR_OPEN_W, REAR_OPEN_H, REAR_OPEN_Z = 310.0, 44.0, 13.0
REAR_PLATE_W, REAR_PLATE_H = 326.0, 66.0
REAR_PLATE_Z = 3.0
# connettori visti DAL RETRO, da sinistra a destra; u misurato sulla piastra (vista da dietro)
REAR_FAN_U = 32.0
REAR_CONNECTORS = [("USB-C 5V", 72), ("USB HOST", 104), ("MIDI OUT", 136), ("MIDI IN", 168),
                   ("OUT R", 200), ("OUT L", 232), ("IN R", 264), ("IN L", 296)]
REAR_CONN_V = 31.0            # altezza del centro dei connettori sulla piastra

# Raspberry Pi 5 sul fondo (asse lungo lungo Y, porte USB verso il retro)
PI_X0, PI_Y0 = 270.0, 115.0   # angolo anteriore sinistro della scheda (porte USB verso il retro, 20 mm liberi per le spine a 90°)
PI_W, PI_L = 56.0, 85.0
PI_HOLES = [(3.5, 3.5), (52.5, 3.5), (3.5, 61.5), (52.5, 61.5)]    # (x lungo 56, y lungo 85)
PI_STANDOFF = 5.0

FEET = [(30, 25), (326, 25), (30, 205), (326, 205)]

# prese d'aria: asole sul fondo davanti, ventola 40 mm sulla piastra posteriore
VENT_SLOTS = [(60 + 22 * i, 14) for i in range(11)]       # centri, asole 16 x 3 mm

# ---------- aggiunte per i modelli 3D ----------
CHEEK_SIDE_SCREWS_Y = [30.0, 115.0, 200.0]   # viti svasate dai fianchi nei bordi del vassoio (Z = 6)
DISPLAY_DEPTH = 16.0                          # Touch Display 2: spessore con elettronica
PI_STACK_H = 35.0                             # Pi 5 + dissipatore attivo + Pisound su header alto (stima prudente)
