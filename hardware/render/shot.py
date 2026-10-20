import sys, subprocess, time
from playwright.sync_api import sync_playwright
srv = subprocess.Popen([sys.executable, "-m", "http.server", "8765"], cwd=".", stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
time.sleep(1)
try:
    with sync_playwright() as pw:
        b = pw.chromium.launch(args=["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"])
        for v in sys.argv[1:]:
            p = b.new_page(viewport={"width": 2400, "height": 1500})
            p.on("console", lambda m: print("console:", m.text))
            p.goto(f"http://localhost:8765/index.html?view={v}")
            p.wait_for_function("document.title==='done'", timeout=180000)
            p.wait_for_timeout(500)
            p.screenshot(path=f"render_{v}.png")
            print("ok", v)
        b.close()
finally:
    srv.terminate()
