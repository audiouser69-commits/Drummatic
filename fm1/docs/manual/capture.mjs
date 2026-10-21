// SPDX-License-Identifier: GPL-3.0-only
// Drummatic FM-1 — the manual's pictures, from the browser emulator: every screen (240x240, saved 3x without smoothing)
// and the panel. Serve the emulator first:
//   sh fm1/web/build.sh && python3 -m http.server 8766 -d fm1/build/web &
//   npm i playwright && node fm1/docs/manual/capture.mjs [fm1/docs/manual/img] [http://localhost:8766/index.html]
// (MANUAL_CHROME: a Chromium to use instead of Playwright's own.) The names are those manual.html uses.
import { chromium } from "playwright";
import fs from "fs";
import path from "path";
import { fileURLToPath } from "url";

const OUT = process.argv[2] || path.join(path.dirname(fileURLToPath(import.meta.url)), "img");
const URL = process.argv[3] || "http://localhost:8766/index.html";
fs.mkdirSync(OUT, { recursive: true });
const b = await chromium.launch({ ...(process.env.MANUAL_CHROME ? { executablePath: process.env.MANUAL_CHROME } : {}),
                                  args: ["--autoplay-policy=no-user-gesture-required"] });
const W = (ms) => p.waitForTimeout(ms);
const BTN = { FX: "Digit1", SCL: "Digit2", ENV: "Digit3", LFO: "Digit4", EDIT: "Digit5", GLO: "Digit6",
              HOME: "Digit7", SAVE: "Digit8", ARP: "Digit9", SEQ: "Digit0", PLAY: "Minus", REC: "Equal" };
let p;

async function start() {
  p = await b.newPage({ viewport: { width: 1200, height: 900 }, deviceScaleFactor: 2.5 });
  await p.goto(URL);
  await p.click("#go");
  await W(4500);
}
async function shot(name, wait = 450) {
  await W(wait);
  const d = await p.evaluate(() => {
    const s = document.getElementById("screen"), c = document.createElement("canvas");
    c.width = c.height = 720;
    const g = c.getContext("2d");
    g.imageSmoothingEnabled = false;
    g.drawImage(s, 0, 0, 720, 720);
    return c.toDataURL("image/png");
  });
  fs.writeFileSync(`${OUT}/${name}.png`, Buffer.from(d.split(",")[1], "base64"));
}
async function tap(code, hold = 60) { await p.keyboard.down(code); await W(hold); await p.keyboard.up(code); }
async function knob(name, n) {
  await p.locator(`.hit.knob[aria-label="${name}"]`).focus();
  for (let i = 0; i < Math.abs(n); i++) { await p.keyboard.press(n > 0 ? "ArrowUp" : "ArrowDown"); await W(25); }
  await p.evaluate(() => document.activeElement && document.activeElement.blur());
}

// 1. the power-on kit playing: EDIT > SYNTH and the panel
await start();
await shot("clean_home", 300);
await p.locator("#device").screenshot({ path: `${OUT}/panel_clean.png`, omitBackground: true });

// 2. every page of every group (a press on the group's button: the next page; from EDIT > SYNTH, EDIT gives ENGINE)
const GROUPS = [["EDIT", 3], ["FX", 2], ["ENV", 3], ["LFO", 4], ["SCL", 3], ["SEQ", 6], ["HOME", 2], ["ARP", 7], ["GLO", 9]];
for (const [g, n] of GROUPS)
  for (let i = 0; i < n; i++) { await tap(BTN[g]); await shot(`${g.toLowerCase()}_${i + 1}`); }
await tap(BTN.SAVE); await shot("save_1");
await tap(BTN.SAVE); await shot("save_2");
await tap("BracketLeft");
await p.keyboard.down(BTN.HOME); await W(800); await p.keyboard.up(BTN.HOME); await shot("menu");
await tap(BTN.EDIT); await W(200);

// 3. steps held: the three lane groups
await tap(BTN.SEQ); await W(200);
await p.keyboard.down("KeyZ"); await p.keyboard.down("KeyV"); await W(300); await shot("lanes_1", 200);
await knob("PRESETS", 1); await shot("lanes_2", 200);
await knob("PRESETS", 1); await shot("lanes_3", 200);
await knob("PRESETS", -2);
await p.keyboard.up("KeyZ"); await p.keyboard.up("KeyV"); await W(300);

// 4. REC: pads, rec
await tap(BTN.REC); await shot("pads");
await tap(BTN.REC); await shot("rec");
await tap(BTN.REC);
await p.close();

// 5. a fresh start: MAP (SCL > MOD source LFO 1, SCL held + KNOB 2 on TONE) and a lock (SAVE held + KNOB 3)
await start();
await tap(BTN.SCL); await W(200);
await knob("KNOB 1", 6); await shot("mod_lfo1");
await tap(BTN.FX); await W(200);
await p.keyboard.down(BTN.SCL); await W(150); await knob("KNOB 2", -12); await shot("map_held", 250); await p.keyboard.up(BTN.SCL);
await shot("tone_mapped");
await p.keyboard.down(BTN.SAVE); await W(150); await knob("KNOB 3", 1); await W(100); await p.keyboard.up(BTN.SAVE);
await shot("tone_locked");
await b.close();
console.log("pictures in", OUT);
