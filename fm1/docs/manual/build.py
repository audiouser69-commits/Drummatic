"""Drummatic FM-1 — Manuale dell'interfaccia: manual.html -> ../Drummatic-FM1-Manuale.pdf.
    pip install playwright pypdf pdfplumber; font Inter installato. python3 fm1/docs/manual/build.py [OUT.pdf]
Copertina e corpo resi da Chromium; numeri di pagina dell'indice in due passate; piè di pagina (capitolo + numero)
reso da Chromium in un secondo PDF e sovrapposto pagina per pagina."""
import io, os, re, sys, html as H
from pathlib import Path
from playwright.sync_api import sync_playwright
from pypdf import PdfReader, PdfWriter
import pdfplumber

HERE = Path(__file__).parent
SRC = HERE / "manual.html"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE.parent / "Drummatic-FM1-Manuale.pdf"
CHROME = os.environ.get("MANUAL_CHROME")          # (otherwise Playwright's own Chromium)
CHAPTERS = [("c1", "Introduzione"), ("c2", "Il pannello"), ("c3", "Lo schermo e i LED"), ("c4", "Navigare: gruppi e pagine"),
            ("c5", "EDIT — il suono della voce"), ("c6", "FX — filtro e distorsione"), ("c7", "ENV — gli inviluppi"),
            ("c8", "LFO — gli oscillatori lenti"), ("c9", "SCL — modulazione"), ("c10", "SEQ — il sequencer"),
            ("c11", "HOME — mixer e routing"), ("c12", "ARP — la Performance"), ("c13", "GLO — tempo, effetti e master"),
            ("c14", "SAVE — kit e preset di voce"), ("c15", "Il menu di sistema e il DADO"), ("c16", "MIDI e clock"),
            ("c17", "Installazione, aggiornamento"), ("c18", "Riferimento rapido")]
SHORT = {"c9": "SCL — modulazione, macro e lucchetti", "c17": "Installazione e problemi"}


def render(page, html, only_cover):
    tmp = HERE / "_render.html"
    tmp.write_text(html, encoding="utf-8")
    page.goto(tmp.resolve().as_uri(), wait_until="networkidle")
    tmp.unlink()
    page.evaluate("""(onlyCover) => {
        if (onlyCover) document.querySelectorAll('body > section:not(.cover)').forEach(s => s.remove());
        else document.querySelector('.cover').remove();
    }""", only_cover)
    page.evaluate("document.fonts.ready")
    page.wait_for_timeout(300)
    return page.pdf(format="A4", print_background=True, prefer_css_page_size=True)


def chapter_pages(pdf_bytes):
    found = {}
    with pdfplumber.open(io.BytesIO(pdf_bytes)) as pdf:
        for i, p in enumerate(pdf.pages):
            txt = p.extract_text() or ""
            for cid, title in CHAPTERS:
                if cid not in found and f"QQ{cid}QQ" in txt:
                    found[cid] = i
    return found


FOOT = """<!doctype html><html><head><meta charset="utf-8"><style>
@page { size: A4; margin: 0; } body { margin: 0; font-family: Inter, sans-serif; }
.p { width: 210mm; height: 297mm; position: relative; break-after: page; }
.f { position: absolute; left: 17mm; right: 17mm; bottom: 9.5mm; display: flex; justify-content: space-between; align-items: center;
     font-size: 7.2pt; color: #8a939c; letter-spacing: .03em; }
.f b { color: #15191e; font-weight: 700; letter-spacing: .12em; }
.f .n { font-weight: 700; color: #15191e; font-variant-numeric: tabular-nums; display: flex; align-items: center; gap: 2.4mm; }
.f .n i { display: inline-block; width: 5mm; height: 1.1mm; border-radius: 1mm; background: linear-gradient(90deg,#3fb0e6,#f26b3a); }
</style></head><body>%s</body></html>"""


def main():
    html = SRC.read_text(encoding="utf-8")
    with sync_playwright() as pw:
        b = pw.chromium.launch(**({"executable_path": CHROME} if CHROME else {}))
        page = b.new_page()
        cover = render(page, html, True)
        body = render(page, html, False)
        found = chapter_pages(body)
        for cid, _ in CHAPTERS:
            if cid in found:
                html = html.replace(f'data-toc="{cid}"></span>', f'data-toc="{cid}">{found[cid] + 2}</span>')
        body = render(page, html, False)
        found2 = chapter_pages(body)
        n = len(PdfReader(io.BytesIO(body)).pages)
        titles, cur = [], ""
        for i in range(n):
            for cid, t in CHAPTERS:
                if found2.get(cid) == i:
                    cur = SHORT.get(cid, t)
            titles.append(cur)
        body = render(page, re.sub(r'<span class="mark">QQc\d+QQ</span>', '', html), False)
        pages = "".join(f'<div class="p"><div class="f"><span><b>DRUMMATIC FM-1</b>&nbsp;&nbsp;·&nbsp;&nbsp;{H.escape(titles[i]) if titles[i] else "Manuale dell&#39;interfaccia"}</span>'
                        f'<span class="n">{i + 2}<i></i></span></div></div>' for i in range(n))
        page.set_content(FOOT % pages, wait_until="networkidle")
        page.evaluate("document.fonts.ready")
        foot = page.pdf(format="A4", print_background=True, prefer_css_page_size=True)
        b.close()
    missing = [c for c, _ in CHAPTERS if c not in found2]
    if missing:
        print("ATTENZIONE: capitoli non trovati:", missing)
    w = PdfWriter()
    for p in PdfReader(io.BytesIO(cover)).pages:
        w.add_page(p)
    fr = PdfReader(io.BytesIO(foot))
    for i, p in enumerate(PdfReader(io.BytesIO(body)).pages):
        p.merge_page(fr.pages[i])
        w.add_page(p)
    w.add_metadata({"/Title": "Drummatic FM-1 — Manuale dell'interfaccia", "/Author": "Emilio Falciatori",
                    "/Subject": "Firmware Drummatic per M-VAVE FM-1, v0.1"})
    with open(OUT, "wb") as f:
        w.write(f)
    print("pagine:", len(w.pages), {k: v + 2 for k, v in found2.items()}, "->", OUT)


if __name__ == "__main__":
    main()
