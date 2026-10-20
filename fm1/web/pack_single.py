#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""The emulator in one file, from a built build/emu: index.html with drummatic.wasm (base64), the worklet (a blob),
the font, the icon and the silence (data URIs) inside: one page to host anywhere (Chromium refuses a blob worklet from a file:// page, so it
still wants http(s), e.g. `python3 -m http.server -d build/web`).

    python3 web/pack_single.py build/web [--artifact]  (Felucca's web/emu/pack_single.py, adapted)
      -> build/web/drummatic-fm1-emulator.html (a whole page); --artifact also build/emu-artifact.html (without
         <html>/<head>/<body>: a page for a host that wraps it)
"""
import base64
import pathlib
import sys


def sub(text, old, new, what):
    if text.count(old) != 1:
        raise SystemExit(f"!! pack_single: {what}: {old!r} found {text.count(old)} times (index.html changed?)")
    return text.replace(old, new)


def main():
    d = pathlib.Path(sys.argv[1])
    b64 = lambda p: base64.b64encode((d / p).read_bytes()).decode()
    page = (d / "index.html").read_text(encoding="utf-8")
    work = (d / "worklet.js").read_text(encoding="utf-8")
    if "</script" in work:
        raise SystemExit("!! pack_single: worklet.js must not contain </script")
    page = sub(page, 'href="favicon.png"', 'href="data:image/png;base64,' + b64("favicon.png") + '"', "icon")
    page = sub(page, 'url("fonts/DotGothic16-subset.woff")', 'url("data:font/woff;base64,' + b64("fonts/DotGothic16-subset.woff") + '")', "font")
    page = sub(page, 'new Audio("silence.wav")', 'new Audio("data:audio/wav;base64,' + b64("silence.wav") + '")', "silence")
    page = sub(page, 'fetch("drummatic.wasm", { cache: "no-cache" }).then((r) => { if (!r.ok) throw new Error(`drummatic.wasm: ${r.status}`); return r.arrayBuffer(); }),',
               'Promise.resolve(Uint8Array.from(atob(document.getElementById("one-wasm").textContent.trim()), (c) => c.charCodeAt(0)).buffer),', "wasm")
    page = sub(page, 'ac.audioWorklet.addModule("worklet.js")]);',
               'ac.audioWorklet.addModule(URL.createObjectURL(new Blob([document.getElementById("one-worklet").textContent], { type: "text/javascript" })))]);', "worklet")
    page = sub(page, '<script type="module">', '<script id="one-worklet" type="text/plain">' + work + '</script>\n'
               '<script id="one-wasm" type="text/plain">' + b64("drummatic.wasm") + '</script>\n<script type="module">', "data")
    (d / "drummatic-fm1-emulator.html").write_text(page, encoding="utf-8")
    if "--artifact" not in sys.argv:
        print(f"  {d}/drummatic-fm1-emulator.html: {len(page)} B (one file)")
        return
    head = page.split("<head>", 1)[1].split("</head>", 1)[0]
    body = page.split("<body>", 1)[1].split("</body>", 1)[0]
    head = "\n".join(l for l in head.splitlines() if "<meta charset" not in l and 'name="viewport"' not in l)
    (d.parent / (d.name + "-artifact.html")).write_text(head + "\n" + body, encoding="utf-8")
    print(f"  {d}/drummatic-fm1-emulator.html: {len(page)} B (one file)")


if __name__ == "__main__":
    main()
