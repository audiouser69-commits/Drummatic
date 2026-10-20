#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Drummatic FM-1 Flasher (the Autechre Machine Flasher, adapted): a window around tools/fm1_install.py (the same USB-MIDI update as Felucca's installer)
that makes the update as hard to get wrong as it can:

  - only two kinds of package are written: a Felucca package (it carries the Felucca update loader, so a cut-off write
    can always be finished) and the unmodified official V15 (FM-1.fwsc, by its SHA-256) - never anything else;
  - the package is checked before anything is sent: its identity, the model of the FM-1 connected, its SHA-256;
  - step 1 (the running firmware reads the package and starts the loader) writes nothing; only step 2 writes;
  - during the write the computer does not sleep (Windows), the window cannot be closed by mistake, and a write that
    is cut off (cable, a USB hub, a stall) is resumed by itself when the loader comes back (three times), or as soon as
    the program sees the FM-1 in update mode again, even after a restart of the program (the job is kept on disk);
  - on a laptop on battery below 30 % it asks first; the log of every update is kept next to the job.

The Drummatic package is built into the program (drummatic-fm1-*.fwsc beside it, or inside the .exe); another
.fwsc (the official V15 to go back) can be chosen. Windows .exe: PyInstaller (see .github/workflows/build.yml).

  python3 tools/am_flasher.py            the window (needs tkinter, mido, python-rtmidi)
  python3 tools/am_flasher.py --check PACKAGE.fwsc   checks a package and prints what it is, no MIDI
"""
import ctypes
import hashlib
import json
import os
import queue
import sys
import threading
import time
from pathlib import Path

HERE = Path(getattr(sys, "_MEIPASS", Path(__file__).resolve().parent))
sys.path.insert(0, str(HERE))
import fm1_install as I  # noqa: E402

APP = "Drummatic FM-1 Flasher"
# the Drummatic package shipped with the program (drummatic-fm1-<version>.fwsc beside it, or inside the .exe)
BUILTIN = next(iter(sorted(HERE.glob("drummatic-fm1-*.fwsc"))), HERE / "drummatic-fm1.fwsc")
WRITE_TRIES = 3                                 # a cut-off write is resumed this many times by itself
RESUME_WAIT = 90.0                              # seconds to wait for the loader to come back


def data_dir():
    base = os.environ.get("APPDATA") or os.path.join(Path.home(), ".config")
    d = Path(base) / "DrummaticFlasher"
    d.mkdir(parents=True, exist_ok=True)
    return d


# --------------------------------------------------------------- the package ---

class Package:
    """a .fwsc checked: what it is, the image the device reads"""

    def __init__(self, path):
        self.path = str(path)
        try:
            raw = Path(path).read_bytes()
        except OSError as e:
            raise I.InstallError("badpkg", f"impossibile leggere {path}: {e.strerror}")
        self.sha256 = hashlib.sha256(raw).hexdigest()
        self.official = self.sha256 == I.STOCK_V15_SHA256
        self.felucca = I.LOADER_MARK in raw
        self.product = I.product_of(raw)
        if not I.re.fullmatch(r"[^_]+_\d+", self.product):
            raise I.InstallError("badpkg", f"{Path(path).name}: non è un pacchetto per FM-1 (identità {self.product!r})")
        if not (self.felucca or self.official):
            raise I.InstallError("badpkg", f"{Path(path).name}: né un pacchetto Felucca (manca il loader di aggiornamento) "
                                           "né il firmware ufficiale V15 intatto: non viene installato")
        self.image = I.logical_image(raw)
        self.size = len(raw)

    @property
    def kind(self):
        return "firmware ufficiale M-VAVE V15" if self.official else "firmware Felucca (Drummatic o un altro firmware Felucca)"

    def describe(self):
        return f"{Path(self.path).name}: {self.kind}, identità {self.product}, {self.size // 1024} KB, SHA-256 {self.sha256[:16]}…"


# ------------------------------------------------------------- the job on disk ---

class Job:
    """the update in progress, on disk: after a crash or a restart the program knows what to finish"""

    def __init__(self, folder=None):
        self.file = Path(folder or data_dir()) / "job.json"

    def save(self, pkg, phase):
        self.file.write_text(json.dumps({"package": pkg.path, "sha256": pkg.sha256, "product": pkg.product,
                                         "phase": phase, "time": time.time()}))

    def load(self):
        try:
            return json.loads(self.file.read_text())
        except (OSError, ValueError):
            return None

    def clear(self):
        try:
            self.file.unlink()
        except OSError:
            pass

    def package(self):
        """the package of the job left over, if it is still there and the same file"""
        j = self.load()
        if not j:
            return None
        for p in (j.get("package"), str(BUILTIN)):
            try:
                pkg = Package(p)
            except Exception:
                continue
            if pkg.sha256 == j.get("sha256"):
                return pkg
        return None


# ------------------------------------------------------------ the computer ---

class KeepAwake:
    """Windows: the computer does not sleep while this is held (SetThreadExecutionState); elsewhere nothing"""
    ES_CONTINUOUS, ES_SYSTEM_REQUIRED = 0x80000000, 0x00000001

    def __enter__(self):
        if sys.platform == "win32":
            ctypes.windll.kernel32.SetThreadExecutionState(self.ES_CONTINUOUS | self.ES_SYSTEM_REQUIRED)
        return self

    def __exit__(self, *a):
        if sys.platform == "win32":
            ctypes.windll.kernel32.SetThreadExecutionState(self.ES_CONTINUOUS)


def battery():
    """(on battery, percent) on Windows; (False, None) elsewhere or without a battery"""
    if sys.platform != "win32":
        return False, None

    class SPS(ctypes.Structure):
        _fields_ = [("ACLineStatus", ctypes.c_ubyte), ("BatteryFlag", ctypes.c_ubyte), ("BatteryLifePercent", ctypes.c_ubyte),
                    ("SystemStatusFlag", ctypes.c_ubyte), ("BatteryLifeTime", ctypes.c_ulong), ("BatteryFullLifeTime", ctypes.c_ulong)]
    s = SPS()
    if not ctypes.windll.kernel32.GetSystemPowerStatus(ctypes.byref(s)) or s.BatteryFlag == 128:
        return False, None
    return s.ACLineStatus == 0, (None if s.BatteryLifePercent == 255 else int(s.BatteryLifePercent))


# --------------------------------------------------------------- the update ---

class Flasher:
    """the update with its safety: step(kind, value) reports, log(text) writes the log"""

    def __init__(self, backend, job=None, log=print, step=lambda *a: None):
        self.up = I.Updater(backend)
        self.job = job or Job()
        self.log, self.step = log, step

    def find(self):
        return self.up.find()

    def flash(self, dev, pkg):
        if I.model_of(dev.id.text) != I.model_of(pkg.product):
            dev.link.close()
            raise I.InstallError("model", f"lo strumento è {dev.id.text}, il pacchetto è per {pkg.product}: niente è stato scritto")
        self.log(f"pacchetto: {pkg.describe()}")
        self.log(f"strumento: {dev.id.text} ({dev.name})" + (", in modalità aggiornamento" if dev.id.loader else ""))
        with KeepAwake():
            if dev.id.loader:
                ota = dev
            else:
                self.log("passo 1: il firmware attuale legge il pacchetto e avvia il loader (non scrive nulla)")
                self.job.save(pkg, "check")
                ota = self.up.check(dev, pkg.image, self.step)
            self.job.save(pkg, "write")
            for attempt in range(1, WRITE_TRIES + 1):
                try:
                    self.log("passo 2: il loader scrive il firmware" + (f" (ripresa {attempt - 1})" if attempt > 1 else ""))
                    self.up.write(ota, pkg.image, self.step)
                    break
                except I.InstallError as e:
                    if e.code not in ("lost", "stopped") or attempt == WRITE_TRIES:
                        raise I.InstallError(e.code, f"{e}. L'FM-1 resta in modalità aggiornamento: ricollega il cavo e "
                                                     "riapri il Flasher, completerà la scrittura")
                    self.log(f"interrotto: {e}")
                    self.log("attendo che il loader torni (ricollega il cavo se serve)...")
                    self.step("wait")
                    ota = self.up.wait_for(lambda i: i.loader, RESUME_WAIT)
                    if not ota:
                        raise I.InstallError("noloader", "il loader non è tornato. Ricollega il cavo USB e riapri il Flasher: "
                                                         "l'FM-1 resta in modalità aggiornamento finché la scrittura non è completa")
            self.job.save(pkg, "verify")
            ident = self.up.verify(pkg.product, self.step)
        self.job.clear()
        self.log(f"fatto: l'FM-1 esegue {ident}")
        return ident


# ------------------------------------------------------------------ the window ---

def gui():
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    try:
        backend = I.MidoBackend()
    except ImportError:
        backend = None
    logfile = data_dir() / "flasher.log"
    root = tk.Tk()
    root.title(APP)
    root.geometry("720x560")
    root.minsize(640, 520)
    events = queue.Queue()
    state = {"busy": False, "pkg": None, "dev": None}
    job = Job()

    def log(s):
        events.put(("log", s))

    def step(kind, value=None):
        events.put(("step", (kind, value)))

    # ---- layout
    pad = {"padx": 12, "pady": 4}
    tk.Label(root, text="Drummatic FM-1 Flasher", font=("Segoe UI", 16, "bold")).pack(anchor="w", **pad)
    tk.Label(root, text="Installa il firmware sull'M-VAVE FM-1 via USB, con controlli e ripresa automatica.",
             fg="#555").pack(anchor="w", padx=12)

    box = ttk.LabelFrame(root, text="1. Firmware da installare")
    box.pack(fill="x", padx=12, pady=8)
    choice = tk.StringVar(value="builtin" if BUILTIN.exists() else "file")
    pkg_info = tk.StringVar()
    ttk.Radiobutton(box, text="Drummatic " + BUILTIN.stem.replace("drummatic-fm1-", "") + " (incluso nel Flasher)",
                    variable=choice, value="builtin",
                    state="normal" if BUILTIN.exists() else "disabled", command=lambda: pick()).pack(anchor="w", padx=8)
    row = tk.Frame(box)
    row.pack(fill="x", padx=8)
    ttk.Radiobutton(row, text="Un altro file .fwsc (per esempio il firmware ufficiale V15 per tornare indietro)",
                    variable=choice, value="file", command=lambda: pick(ask=True)).pack(side="left")
    tk.Label(box, textvariable=pkg_info, fg="#245", wraplength=660, justify="left").pack(anchor="w", padx=8, pady=4)

    dbox = ttk.LabelFrame(root, text="2. FM-1 collegato")
    dbox.pack(fill="x", padx=12, pady=4)
    dev_info = tk.StringVar(value="cerco l'FM-1...")
    tk.Label(dbox, textvariable=dev_info, wraplength=560, justify="left").pack(side="left", padx=8, pady=6)
    find_btn = ttk.Button(dbox, text="Cerca", command=lambda: start_find())
    find_btn.pack(side="right", padx=8)

    cbox = ttk.LabelFrame(root, text="3. Prima di iniziare")
    cbox.pack(fill="x", padx=12, pady=4)
    checks = []
    for t in ("Il cavo USB è un cavo dati, collegato direttamente al computer (meglio senza hub)",
              "Ableton Live, l'editor M-VAVE e le altre app che usano il MIDI sono chiuse",
              "Il portatile è collegato alla corrente"):
        v = tk.BooleanVar()
        ttk.Checkbutton(cbox, text=t, variable=v, command=lambda: refresh()).pack(anchor="w", padx=8)
        checks.append(v)

    go = ttk.Button(root, text="Installa", command=lambda: start_flash())
    go.pack(pady=6)
    bar = ttk.Progressbar(root, maximum=100)
    bar.pack(fill="x", padx=12)
    status = tk.StringVar(value="")
    tk.Label(root, textvariable=status, font=("Segoe UI", 10, "bold")).pack(anchor="w", padx=12, pady=2)
    text = tk.Text(root, height=8, wrap="word", state="disabled", bg="#f6f6f6")
    text.pack(fill="both", expand=True, padx=12, pady=(0, 10))

    # ---- behaviour
    def write_log(s):
        line = time.strftime("%H:%M:%S ") + s
        text.configure(state="normal")
        text.insert("end", line + "\n")
        text.see("end")
        text.configure(state="disabled")
        try:
            with open(logfile, "a", encoding="utf-8") as f:
                f.write(time.strftime("%Y-%m-%d ") + line + "\n")
        except OSError:
            pass

    def pick(ask=False):
        path = str(BUILTIN)
        if choice.get() == "file":
            if ask or state["pkg"] is None or state["pkg"].path == str(BUILTIN):
                path = filedialog.askopenfilename(title="Scegli un firmware .fwsc", filetypes=[("Firmware FM-1", "*.fwsc")])
                if not path:
                    if BUILTIN.exists():
                        choice.set("builtin")
                        return pick()
                    state["pkg"] = None
                    pkg_info.set("nessun file scelto")
                    return refresh()
            else:
                path = state["pkg"].path
        try:
            state["pkg"] = Package(path)
            pkg_info.set(state["pkg"].describe())
        except I.InstallError as e:
            state["pkg"] = None
            pkg_info.set(f"NON INSTALLABILE: {e}")
        refresh()

    def refresh():
        ok = (not state["busy"] and state["pkg"] is not None and state["dev"] is not None and all(v.get() for v in checks))
        go.configure(state="normal" if ok else "disabled")
        find_btn.configure(state="disabled" if state["busy"] else "normal")

    def start_find():
        if state["busy"] or backend is None:
            if backend is None:
                dev_info.set("manca il supporto MIDI (mido, python-rtmidi)")
            return
        state["busy"] = True
        dev_info.set("cerco l'FM-1...")
        refresh()

        def work():
            try:
                dev = I.Updater(backend).find()
                if dev:
                    dev.link.close()
                events.put(("found", dev))
            except Exception as e:
                events.put(("found", e))
        threading.Thread(target=work, daemon=True).start()

    def found(dev):
        state["busy"] = False
        state["dev"] = dev if isinstance(dev, I.Device) else None
        if isinstance(dev, Exception):
            dev_info.set(f"errore MIDI: {dev}")
        elif dev is None:
            names = backend.input_names() if backend else []
            dev_info.set("FM-1 non trovato. Cavo dati? Un'altra app usa il MIDI?\nPorte MIDI: " + (", ".join(names) or "nessuna"))
        elif dev.id.loader:
            dev_info.set(f"{dev.id.text} in MODALITÀ AGGIORNAMENTO: un aggiornamento precedente non è finito. "
                         "Installa per completarlo.")
            left = job.package()
            if left:
                state["pkg"] = left
                choice.set("builtin" if left.path == str(BUILTIN) else "file")
                pkg_info.set(left.describe() + "  (il pacchetto dell'aggiornamento interrotto)")
                write_log("trovato un aggiornamento interrotto: si completa con lo stesso pacchetto")
        else:
            dev_info.set(f"{dev.id.text} (porta {dev.name})")
        refresh()

    def start_flash():
        pkg, dev = state["pkg"], state["dev"]
        on_batt, pct = battery()
        if on_batt and pct is not None and pct < 30 and not messagebox.askyesno(
                APP, f"Il portatile è a batteria ({pct} %). Se si spegne durante la scrittura l'FM-1 resta in modalità "
                     "aggiornamento. Continuare lo stesso?"):
            return
        what = "completare l'aggiornamento interrotto" if dev.id.loader else f"installare {pkg.kind} ({pkg.product})"
        if not messagebox.askokcancel(APP, f"Stai per {what} sull'FM-1 {dev.id.text}.\n\nDurante la scrittura non "
                                           "scollegare il cavo e non spegnere l'FM-1. Ci vogliono un paio di minuti."):
            return
        state["busy"] = True
        bar["value"] = 0
        refresh()

        def work():
            try:
                f = Flasher(backend, job, log, step)
                d = f.find()
                if not d:
                    raise I.InstallError("notfound", "l'FM-1 non risponde più: ricollegalo e premi Cerca")
                events.put(("done", f.flash(d, pkg)))
            except I.InstallError as e:
                events.put(("fail", str(e)))
            except Exception as e:                  # never leave the window stuck
                events.put(("fail", f"errore inatteso: {e!r}"))
        threading.Thread(target=work, daemon=True).start()

    def pump():
        try:
            while True:
                kind, v = events.get_nowait()
                if kind == "log":
                    write_log(v)
                elif kind == "found":
                    found(v)
                elif kind == "step":
                    k, val = v
                    if k == "check":
                        status.set(f"passo 1: verifica del pacchetto ({val} letture)")
                        bar["value"] = min(10, (val or 0) // 2)
                    elif k == "loader":
                        status.set("avvio del loader di aggiornamento...")
                        bar["value"] = 10
                    elif k == "write":
                        status.set(f"passo 2: scrittura {val}%  -  NON scollegare")
                        bar["value"] = 10 + int(val) * 85 // 100
                    elif k == "wait":
                        status.set("scrittura interrotta: attendo il loader per riprendere...")
                    elif k == "reboot":
                        status.set("riavvio dell'FM-1...")
                        bar["value"] = 97
                    elif k == "start":
                        status.set(f"avvio dell'aggiornamento su {val}")
                elif kind == "done":
                    state["busy"] = False
                    bar["value"] = 100
                    status.set(f"Fatto: l'FM-1 esegue {v}")
                    state["dev"] = None
                    refresh()
                    messagebox.showinfo(APP, f"Installazione completata e verificata: l'FM-1 esegue {v}.")
                    start_find()
                elif kind == "fail":
                    state["busy"] = False
                    status.set("Non completato")
                    write_log(f"ERRORE: {v}")
                    state["dev"] = None
                    refresh()
                    messagebox.showerror(APP, f"{v}\n\nSe l'FM-1 resta nero: verifica se il computer vede un dispositivo "
                                              "USB 'WL80UBOOT'; vedi il manuale, capitolo 2.")
                    start_find()
        except queue.Empty:
            pass
        root.after(100, pump)

    def on_close():
        if state["busy"] and not messagebox.askyesno(APP, "Un'operazione è in corso. Chiudendo ora la scrittura si interrompe "
                                                          "(si potrà completare riaprendo il Flasher). Chiudere lo stesso?"):
            return
        root.destroy()

    root.protocol("WM_DELETE_WINDOW", on_close)
    if BUILTIN.exists():
        pick()
    else:
        pkg_info.set("scegli un file .fwsc")
    left = job.load()
    if left:
        write_log(f"l'ultimo aggiornamento non risulta finito (fase: {left.get('phase')}): collega l'FM-1, verrà completato")
    root.after(100, pump)
    root.after(300, start_find)
    root.mainloop()


def main(argv=None):
    argv = sys.argv[1:] if argv is None else argv
    if len(argv) == 2 and argv[0] == "--check":
        try:
            print(Package(argv[1]).describe())
            return 0
        except I.InstallError as e:
            print(f"error: {e}", file=sys.stderr)
            return 2
    gui()
    return 0


if __name__ == "__main__":
    sys.exit(main())
