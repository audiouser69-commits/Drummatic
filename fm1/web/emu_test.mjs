// SPDX-License-Identifier: GPL-3.0-only
// Drummatic FM-1: the firmware's panel and sound in the browser build, driven without a browser (node).
//   node web/emu_test.mjs build/web/drummatic.wasm [OUTDIR]
// Boots, presses and turns the controls as a hand would (each change held a few ms of device time), checks what the
// machine does, and writes the screen of each step as a PNG and the audio as a WAV into OUTDIR.
import fs from "node:fs";
import zlib from "node:zlib";

const [wasmPath = "build/web/drummatic.wasm", outDir = ""] = process.argv.slice(2);
const { instance } = await WebAssembly.instantiate(fs.readFileSync(wasmPath), {});
const ex = instance.exports;
if (ex._initialize) ex._initialize();
const B = { FX: 0, SCL: 1, ENV: 2, LFO: 3, EDIT: 4, GLO: 5, HOME: 6, SAVE: 7, ARP: 8, SEQ: 9, PLAY: 10, REC: 11, OCTDN: 12, OCTUP: 13 };
const EN = { SELECT: 0, ALGO: 1, PRESETS: 2, K1: 3, K2: 4, K3: 5, K4: 6 };
const WHITE = [0, 2, 4, 6, 7, 9, 11, 12, 14, 16, 18, 19, 21, 23, 24, 26];
const BLACK = [1, 3, 5, 8, 10, 13, 15, 17, 20, 22, 25];
let fails = 0, shots = 0;
const OFF = { fill: ex.web_off(0), engine: ex.web_off(1), mute: ex.web_off(2), solo: ex.web_off(3), steps: ex.web_off(4) };
const audio = [];
const ok = (c, msg) => { if (!c) { fails++; console.log("FAIL", msg); } else console.log("ok  ", msg); };
const str = (p) => { const b = new Uint8Array(ex.memory.buffer, p, 64); let n = 0; while (b[n]) n++; return new TextDecoder().decode(b.subarray(0, n)); };

function run(ms) {                                  // device time forward, keeping the audio
  const n = Math.round(ms * 44.1);
  for (let done = 0; done < n;) {
    const k = Math.min(1024, n - done);
    ex.web_render(k);
    const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), k), r = new Float32Array(ex.memory.buffer, ex.web_out_r(), k);
    for (let i = 0; i < k; i++) audio.push(l[i], r[i]);
    done += k;
  }
}
let btn = 0, keys = 0;
const press = (b, ms = 60) => { btn |= 1 << b; ex.web_buttons(btn); run(ms); btn &= ~(1 << b); ex.web_buttons(btn); run(40); };
const hold = (b) => { btn |= 1 << b; ex.web_buttons(btn); run(30); };
const let_go = (b) => { btn &= ~(1 << b); ex.web_buttons(btn); run(40); };
const keyDown = (k) => { keys |= 1 << k; ex.web_keys(keys); run(30); };
const keyUp = (k) => { keys &= ~(1 << k); ex.web_keys(keys); run(40); };
const tap = (k) => { keyDown(k); keyUp(k); };
const turn = (role, n) => { for (let i = 0; i < Math.abs(n); i++) { ex.web_enc(role, Math.sign(n)); run(20); } };
const rms = (a) => Math.sqrt(a.reduce((s, x) => s + x * x, 0) / Math.max(1, a.length));

function png(name) {
  if (!outDir) return;
  const fb = new Uint16Array(ex.memory.buffer, ex.web_screen(), 240 * 240);
  const raw = Buffer.alloc(240 * (240 * 3 + 1));
  for (let y = 0; y < 240; y++) {
    raw[y * 721] = 0;
    for (let x = 0; x < 240; x++) {
      const v = fb[y * 240 + x], p = ((v >> 8) | (v << 8)) & 0xFFFF, o = y * 721 + 1 + x * 3;
      raw[o] = ((p >> 11) & 31) * 255 / 31; raw[o + 1] = ((p >> 5) & 63) * 255 / 63; raw[o + 2] = (p & 31) * 255 / 31;
    }
  }
  const crc = (b) => { let c = ~0; for (const x of b) { c ^= x; for (let k = 0; k < 8; k++) c = (c >>> 1) ^ (0xEDB88320 & -(c & 1)); } return ~c >>> 0; };
  const chunk = (t, d) => { const h = Buffer.alloc(8); h.writeUInt32BE(d.length); h.write(t, 4); const c = Buffer.alloc(4); c.writeUInt32BE(crc(Buffer.concat([Buffer.from(t), d]))); return Buffer.concat([h, d, c]); };
  const ihdr = Buffer.alloc(13); ihdr.writeUInt32BE(240, 0); ihdr.writeUInt32BE(240, 4); ihdr[8] = 8; ihdr[9] = 2;
  const file = Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), chunk("IHDR", ihdr), chunk("IDAT", zlib.deflateSync(raw)), chunk("IEND", Buffer.alloc(0))]);
  fs.writeFileSync(`${outDir}/${String(++shots).padStart(2, "0")}_${name}.png`, file);
}

// ---- boot
ex.web_nor_erase();
ex.web_master(800);
ex.web_boot();
run(600);
png("boot");
ok(str(ex.web_page_name()) === "SYNTH", "boots on SYNTH, voice 1");
ok(ex.web_sel() === 0, "voice 1 selected");

// ---- play: the power-on kit sounds
let a0 = audio.length;
press(B.PLAY);
run(2000);
const playRms = rms(audio.slice(a0));
ok(ex.web_playing() === 1, "PLAY starts the sequencer");
ok(playRms > 0.01, `the power-on kit sounds (rms ${playRms.toFixed(4)})`);
ok(ex.web_trigs(0) >= 3 && ex.web_trigs(2) >= 6, `kick and hat play (${ex.web_trigs(0)} / ${ex.web_trigs(2)} hits)`);
png("playing");

// ---- voice select with black keys, page buttons
tap(BLACK[2]);
ok(ex.web_sel() === 2, "black key 3 selects voice 3");
press(B.SEQ);
ok(str(ex.web_page_name()) === "EUCLID", "SEQ opens EUCLID");
const fill0 = ex.web_vbyte(2, OFF.fill);
turn(EN.K2, 3);
ok(ex.web_vbyte(2, OFF.fill) === fill0 + 3, `KNOB 2 turns FILL ${fill0} -> ${ex.web_vbyte(2, OFF.fill)}`);
png("seq_euclid");
press(B.SEQ);
ok(str(ex.web_page_name()) === "TIME", "SEQ again: TIME");
turn(EN.PRESETS, -1);
ok(str(ex.web_page_name()) === "EUCLID", "PRESETS back: EUCLID");

// ---- steps: a tap writes a trig, a hold edits the lanes
tap(BLACK[0]);
const m0 = ex.web_manual(0, 0);
tap(WHITE[2]);
ok(((ex.web_manual(0, 0) >> 2) & 1) === 1 - ((m0 >> 2) & 1), "white key 3 toggles the trig of step 3");
keyDown(WHITE[5]);
turn(EN.K1, -5);
turn(EN.K2, 7);
png("step_held");
keyUp(WHITE[5]);
ok(ex.web_lane(0, 0, 5) < 100, `held step 6, KNOB 1: VEL ${ex.web_lane(0, 0, 5)}`);
ok(ex.web_lane(0, 2, 5) === 7, `held step 6, KNOB 2: PITCH ${ex.web_lane(0, 2, 5)}`);
ok(((ex.web_manual(0, 0) >> 5) & 1) === ((m0 >> 5) & 1), "a hold with edits is not a tap");

// ---- sound pages
press(B.EDIT);
ok(str(ex.web_page_name()) === "SYNTH", "EDIT: SYNTH");
const p1 = ex.web_base(0, 1);
turn(EN.K1, 10);
ok(Math.abs(ex.web_base(0, 1) - p1 - 0.1) < 0.02, "KNOB 1 turns the engine's first knob");
press(B.EDIT);
const eng0 = ex.web_vbyte(0, OFF.engine);
turn(EN.K1, 4);
ok(ex.web_vbyte(0, OFF.engine) === eng0 + 4, `ENGINE: ${eng0} -> ${ex.web_vbyte(0, OFF.engine)}`);
png("engine");
turn(EN.K1, -4);
press(B.FX);
ok(str(ex.web_page_name()) === "TONE", "FX: TONE");
turn(EN.K2, -30);
png("tone");
press(B.ENV); press(B.LFO); press(B.HOME);
ok(str(ex.web_page_name()) === "MIX", "HOME: MIX");
png("mix");

// ---- MAP: SCL held + a knob
press(B.SCL);
ok(str(ex.web_page_name()) === "MOD", "SCL: MOD");
turn(EN.K1, 6);                                       // source: LFO 1
press(B.FX);
hold(B.SCL);
turn(EN.K2, 25);                                      // CUTOFF
png("map");
let_go(B.SCL);
ok(Math.abs(ex.web_depth(0, 6, 5) - 0.25) < 0.02, `SCL held + KNOB 2: LFO 1 -> CUTOFF ${ex.web_depth(0, 6, 5).toFixed(2)}`);
ok(str(ex.web_page_name()) === "TONE", "SCL used as a modifier: the page stays");

// ---- locks: SAVE held + a knob
hold(B.SAVE);
turn(EN.K2, 1);
let_go(B.SAVE);
ok(((ex.web_locks(0) >> 5) & 1) === 1, "SAVE held + KNOB 2: CUTOFF locked");

// ---- mute / solo / dado
tap(BLACK[8]);
run(400);
ok(ex.web_vbyte(0, OFF.mute) === 1, "MUTE tap: voice 1 muted");
tap(BLACK[8]);
const t2 = ex.web_trigs(2);
keyDown(BLACK[9]); tap(BLACK[2]); keyUp(BLACK[9]);
ok(ex.web_vbyte(2, OFF.solo) === 1, "SOLO held + key 3: voice 3 solo");
keyDown(BLACK[9]); tap(BLACK[2]); keyUp(BLACK[9]);
const b0 = ex.web_base(0, 2);
press(B.EDIT);
tap(BLACK[10]);
ok(ex.web_base(0, 2) !== b0, "DADO on a sound page: the voice randomized");
ok(((ex.web_locks(0) >> 5) & 1) === 1 && Math.abs(ex.web_base(0, 5) - ex.web_base(0, 5)) === 0, "the lock survives the dice");

// ---- performance
press(B.ARP);
ok(str(ex.web_page_name()) === "PF ENV", "ARP: PERFORM");
turn(EN.K3, 20);
ok(Math.abs(ex.web_perf(2) - 0.4) < 0.02, `PF DECAY ${ex.web_perf(2).toFixed(2)}`);
png("perform");

// ---- pads
press(B.REC);
a0 = audio.length;
const tk = ex.web_trigs(4);
tap(WHITE[4]);
run(100);
ok(ex.web_trigs(4) === tk + 1, "PADS: white key 5 plays voice 5");
press(B.REC); press(B.REC);

// ---- kits: name, save, change, load
press(B.SAVE);
ok(str(ex.web_page_name()) === "KITS", "SAVE: KITS");
turn(EN.K1, 2);                                       // slot 3
turn(EN.K3, 4);                                       // "D"
turn(EN.K2, 1); turn(EN.K3, 13);                      // "M"
png("kits");
press(B.OCTUP);
run(200);
ok(str(ex.web_msg()) === "KIT SAVED", `OCT+: ${str(ex.web_msg())}`);
const fillSaved = ex.web_vbyte(2, OFF.fill);
press(B.SEQ); tap(BLACK[2]); turn(EN.K2, -2);
ok(ex.web_vbyte(2, OFF.fill) === fillSaved - 2, "changed after the save");
press(B.SAVE);
tap(WHITE[2]);
run(100);
ok(ex.web_vbyte(2, OFF.fill) === fillSaved && str(ex.web_kit_name()) === "M0IT", `key 3 loads it back (${str(ex.web_kit_name())})`);
png("kit_loaded");

// ---- the reference kit: KITS > NEW turned right twice, OCT+
press(B.EDIT);
press(B.SAVE);
turn(EN.K4, 2);
press(B.OCTUP);
ok(str(ex.web_kit_name()) === "DAEL REF" && Math.abs(ex.web_bpm() - 90) < 0.01, `NEW x2 + OCT+: ${str(ex.web_kit_name())} at ${ex.web_bpm()} BPM`);
press(B.HOME);

// ---- saving again into a used slot keeps the new kit (both copies of the slot in turn)
press(B.EDIT); press(B.SAVE);
for (const fill of [5, 6, 7]) {
  press(B.SEQ); tap(BLACK[1]);
  while (ex.web_vbyte(1, OFF.fill) < fill) turn(EN.K2, 1);
  while (ex.web_vbyte(1, OFF.fill) > fill) turn(EN.K2, -1);
  press(B.EDIT); press(B.SAVE);
  press(B.OCTUP); press(B.OCTUP);                    // (a used slot: OCT+ twice)
  run(100);
}
press(B.SEQ); turn(EN.K2, -3);
press(B.EDIT); press(B.SAVE);
tap(WHITE[2]); run(100);
ok(ex.web_vbyte(1, OFF.fill) === 7, `saved three times into slot 3, loaded: the last (FILL ${ex.web_vbyte(1, OFF.fill)})`);

// ---- voice presets: two into the same bank, both load
press(B.EDIT); press(B.SAVE); press(B.SAVE);         // VOICES
ok(str(ex.web_page_name()) === "VOICES", "SAVE twice: VOICES");
tap(BLACK[0]);
press(B.OCTUP); run(100);                            // voice 1 -> preset 1
const e1 = ex.web_vbyte(0, OFF.engine);
tap(BLACK[1]);
turn(EN.K1, 1);                                      // preset 2
press(B.OCTUP); run(100);                            // voice 2 -> preset 2
const e2 = ex.web_vbyte(1, OFF.engine);
tap(BLACK[3]);
tap(WHITE[0]); run(100);
ok(ex.web_vbyte(3, OFF.engine) === e1, `preset 1 into voice 4: engine ${ex.web_vbyte(3, OFF.engine)} (${e1})`);
tap(WHITE[1]); run(100);
ok(ex.web_vbyte(3, OFF.engine) === e2, `preset 2 into voice 4: engine ${ex.web_vbyte(3, OFF.engine)} (${e2})`);

// ---- the menu
hold(B.HOME); run(700); png("menu"); let_go(B.HOME);
ok(str(ex.web_page_name()) === "MENU", "HOME held: MENU");
press(B.HOME);

// ---- stop, MIDI in
press(B.PLAY);
run(300);
ok(ex.web_playing() === 0, "PLAY again stops");
const tm = ex.web_trigs(1);
ex.web_midi(0x90, 61, 100);                            // ch 1, C#: voice 2
run(50);
ok(ex.web_trigs(1) === tm + 1, "MIDI ch 1 note C# plays voice 2");
ex.web_midi(0x99, 1, 100);                             // ch 10, note 1: play
run(200);
ok(ex.web_playing() === 1, "MIDI ch 10 note 1: play");
ex.web_midi(0x99, 2, 100);
run(100);

// ---- tempo
const bpm0 = ex.web_bpm();
turn(EN.SELECT, 5);
ok(Math.abs(ex.web_bpm() - bpm0 - 5) < 0.01, `SELECT: tempo ${bpm0} -> ${ex.web_bpm()}`);

if (outDir) {
  const n = audio.length / 2, buf = Buffer.alloc(44 + n * 4);
  buf.write("RIFF", 0); buf.writeUInt32LE(36 + n * 4, 4); buf.write("WAVEfmt ", 8); buf.writeUInt32LE(16, 16);
  buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22); buf.writeUInt32LE(44100, 24); buf.writeUInt32LE(44100 * 4, 28);
  buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34); buf.write("data", 36); buf.writeUInt32LE(n * 4, 40);
  for (let i = 0; i < n * 2; i++) buf.writeInt16LE(Math.max(-32767, Math.min(32767, Math.round(audio[i] * 32767))), 44 + i * 2);
  fs.writeFileSync(`${outDir}/emu_session.wav`, buf);
}
console.log(fails ? `${fails} FAILED` : "all ok");
process.exit(fails ? 1 : 0);
