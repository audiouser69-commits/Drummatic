"""Genera Drummatic-Manuale.pdf: copertina + corpo, indice con numeri di pagina, piè di pagina."""
import io, re, sys
from pathlib import Path
from playwright.sync_api import sync_playwright
from pypdf import PdfReader, PdfWriter
import pdfplumber
from reportlab.pdfgen import canvas
from reportlab.lib.pagesizes import A4
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont

HERE = Path(__file__).parent
SRC = HERE / "manual.html"
OUT = Path(sys.argv[1]) if len(sys.argv) > 1 else HERE / "Drummatic-Manuale.pdf"
CHAPTERS = [("c1", "Introduzione"), ("c2", "Installazione"), ("c3", "L'interfaccia"), ("c4", "La voce"),
            ("c5", "Il sequencer"), ("c6", "Mixer, effetti e master"), ("c7", "Performance"),
            ("c8", "Kit, preset e randomizzazione"), ("c9", "Controllo MIDI e tastiera"),
            ("c10", "Risoluzione dei problemi"), ("c11", "Appendice")]


def render(page, html, only_cover):
    tmp = HERE / "_render.html"
    tmp.write_text(html, encoding="utf-8")
    page.goto(tmp.resolve().as_uri(), wait_until="networkidle")
    page.evaluate("""(onlyCover) => {
        const cover = document.querySelector('.cover');
        if (onlyCover) { document.querySelectorAll('body > section:not(.cover)').forEach(s => s.remove()); }
        else { cover.remove(); }
    }""", only_cover)
    page.wait_for_timeout(300)
    return page.pdf(format="A4", print_background=True, prefer_css_page_size=True)


def chapter_pages(pdf_bytes):
    """pagina (0-based nel corpo) di inizio di ogni capitolo: la prima riga contiene il numero e il titolo"""
    found = {}
    with pdfplumber.open(io.BytesIO(pdf_bytes)) as pdf:
        for i, p in enumerate(pdf.pages):
            head = " ".join((p.extract_text() or "").splitlines()[:3])
            for cid, title in CHAPTERS:
                if cid not in found and title in head and i > 0:
                    found[cid] = i
    return found


def main():
    html = SRC.read_text(encoding="utf-8")
    with sync_playwright() as pw:
        b = pw.chromium.launch()
        page = b.new_page()
        cover = render(page, html, True)
        body = render(page, html, False)
        found = chapter_pages(body)
        # numeri di pagina: copertina = 1, corpo parte da 2
        for cid, _ in CHAPTERS:
            if cid in found:
                html = html.replace(f'data-toc="{cid}"></span>', f'data-toc="{cid}">{found[cid] + 2}</span>')
        body = render(page, html, False)
        b.close()
    missing = [c for c, _ in CHAPTERS if c not in found]
    if missing:
        print("ATTENZIONE: capitoli non trovati:", missing)

    # piè di pagina
    pdfmetrics.registerFont(TTFont("Inter", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf") if True
                            else TTFont("Inter", "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"))
    reader = PdfReader(io.BytesIO(body))
    titles = {}
    cur = ""
    for i in range(len(reader.pages)):
        for cid, t in CHAPTERS:
            if found.get(cid) == i:
                cur = t
        titles[i] = cur
    writer = PdfWriter()
    for p in PdfReader(io.BytesIO(cover)).pages:
        writer.add_page(p)
    W, H = A4
    for i, p in enumerate(reader.pages):
        buf = io.BytesIO()
        c = canvas.Canvas(buf, pagesize=A4)
        c.setFont("Inter", 7.5)
        c.setFillColorRGB(0.37, 0.41, 0.45)
        c.drawString(18 * 2.8346, 10 * 2.8346, "DRUMMATIC  ·  Manuale utente" + (f"  ·  {titles[i]}" if titles[i] else ""))
        c.drawRightString(W - 18 * 2.8346, 10 * 2.8346, str(i + 2))
        c.setStrokeColorRGB(0.91, 0.59, 0.16)
        c.setLineWidth(1.2)
        c.line(W - 18 * 2.8346 - 14, 10 * 2.8346 + 9, W - 18 * 2.8346, 10 * 2.8346 + 9)
        c.save()
        p.merge_page(PdfReader(io.BytesIO(buf.getvalue())).pages[0])
        writer.add_page(p)
    writer.add_metadata({"/Title": "Drummatic — Manuale utente", "/Author": "Emilio Falciatori", "/Subject": "Drummatic 0.2.2"})
    with open(OUT, "wb") as f:
        writer.write(f)
    print("pagine:", len(writer.pages), "capitoli:", {k: v + 2 for k, v in found.items()}, "->", OUT)


if __name__ == "__main__":
    main()
