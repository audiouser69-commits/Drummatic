// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Drummatic FM-1 in an AudioWorklet (Felucca's web/emu/worklet.js, Leo Kuroshita, GPL-3.0): drummatic.wasm is the
// firmware's engine and panel against a simulated FM-1 (web/dm_web.c). Each render quantum asks it for 128 frames, which runs the device's clock forward;
// between quanta the page's input goes in and, about 30 times a second (4 when the page is hidden), the screen,
// the lights and any changed flash go out. After X0X's web/emu/worklet.js (charlesvestal/fm1-x0x, GPL-3.0).

// The flash the page keeps: the kits and the voice presets 0x97000..0xDFFFF, the autosave 0xE5000..0xE6FFF, the
// settings 0xFC000..0xFDFFF; only the sectors that hold something (an erased sector is all 0xFF). {offset: bytes}
export const STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE7000], [0xFC000, 0x100000]];
export function storeSectors(nor) {
  const out = {};
  for (const [a, b] of STORE_AREAS)
    for (let off = a; off < b; off += 4096) {
      const s = nor.subarray(off, off + 4096);
      if (s.some((v) => v !== 0xFF)) out[off] = s.slice();
    }
  return out;
}
export function restoreSectors(nor, sectors) {
  for (const [k, v] of Object.entries(sectors || {})) {
    const off = +k, b = v instanceof Uint8Array ? v : new Uint8Array(v);
    if (STORE_AREAS.some(([a, e]) => off >= a && off + 4096 <= e && off % 4096 === 0) && b.length === 4096)
      nor.set(b, off);
  }
}

export const HOLD_MS = 24;

if (typeof registerProcessor === "function") {
  const clock = globalThis.performance ? () => globalThis.performance.now() : () => Date.now();
  class Felucca extends AudioWorkletProcessor {
    constructor() {
      super();
      this.ex = null;
      this.sentDraws = -1;
      this.sentWrites = -1;
      this.sentLeds = "";
      this.sentH2 = "";
      this.sentGl = "";
      this.lastFrame = 0;
      this.period = 1 / 30;
      this.busy = 0;
      this.frames = 0;
      this.queue = [];                                // button / key changes, each held >= HOLD_MS of device time
      this.nextOk = 0;
      this.port.onmessage = (e) => this.onMessage(e.data);
    }

    async onMessage(m) {
      if (m.type === "load") {
        try {
          const { instance } = await WebAssembly.instantiate(m.wasm, {});
          const ex = instance.exports;
          if (ex._initialize) ex._initialize();
          ex.web_nor_erase();
          restoreSectors(new Uint8Array(ex.memory.buffer, ex.web_nor(), ex.web_nor_size()), m.sectors);
          ex.web_master(m.master ?? 700);
          ex.web_boot();
          this.ex = ex;
          this.sentWrites = ex.web_flash_writes_count();
          this.port.postMessage({ type: "ready", rate: ex.web_sample_rate() });
        } catch (err) {
          this.port.postMessage({ type: "error", message: String(err && err.message || err) });
        }
        return;
      }
      const ex = this.ex;
      if (!ex) return;
      if (m.type === "buttons" || m.type === "keys") this.queue.push(m);
      else if (m.type === "enc") ex.web_enc(m.role, m.n | 0);
      else if (m.type === "master") ex.web_master(m.value | 0);
      else if (m.type === "midi") ex.web_midi(m.data[0] | 0, m.data[1] | 0, m.data[2] | 0);
      else if (m.type === "snap") this.snap(m.id);
      else if (m.type === "visible") this.period = m.on ? 1 / 30 : 1 / 4;
      else if (m.type === "gl" && ex.web_gl_state) {   // GLITCH: the monitor's edits
        if (m.cmd) ex.web_gl_cmd(m.cmd);
        if (m.toggle) ex.web_gl_toggle(m.toggle[0], m.toggle[1]);
        if (m.lane !== undefined) ex.web_gl_lane(m.lane);
      }
      else if (m.type === "h2ab" && ex.web_h2_ab) this.port.postMessage({ type: "h2ab", off: ex.web_h2_ab(m.off ? 1 : 0) });   // the harmony: A/B
    }


    // The rating log (docs/VOICES_RATING.md): what is sounding now, named. Layout: web_snapshot in felucca_web.c.
    snap(id) {
      const ex = this.ex, mem = ex.memory.buffer;
      const head = ex.web_snap_info(0), nm = ex.web_snap_info(1), np = ex.web_snap_info(2), total = ex.web_snap_info(3);
      const w = new Int32Array(mem, ex.web_snapshot(), total).slice();
      const str = (p) => { const b = new Uint8Array(mem, p, 16); let n = 0; while (b[n]) n++; return new TextDecoder().decode(b.subarray(0, n)); };
      const tracks = [];
      let k = head + nm;
      for (let t = 0; t < 4; t++, k += 2 + np) {
        const labels = [];
        for (let j = 0; j < np; j++) labels.push(str(ex.web_param_label(t, j)));
        tracks.push({ eng: w[k], voice: w[k + 1], name: str(ex.web_voice_name(t)), art: ex.web_voice_art(t), labels, p: Array.from(w.subarray(k + 2, k + 2 + np)) });
      }
      this.port.postMessage({ type: "snap", id, data: {
        layout: w[0], catalog: w[1], seed: w[2] >>> 0, world: w[3], bpm: w[4], root: w[5], scale: w[6],
        dens: w[7], dens_eff: w[8], open: w[9], loose: w[10], focus: w[11], perf_held: w[12], flags: w[13],
        world_age_ms: w[14], macros: Array.from(w.subarray(head, head + nm)), tracks } });
    }

    publish() {
      const ex = this.ex, mem = ex.memory.buffer;
      const msg = { type: "frame" };
      const leds = [ex.web_lit_buttons(), ex.web_lit_keys(), ex.web_dim_buttons(), ex.web_dim_keys(), ex.web_dim_level(),
                    ex.web_breath_buttons(), ex.web_breath_keys(), ex.web_mid_keys ? ex.web_mid_keys() : 0];
      const an = ex.web_anim_levels ? ex.web_anim_levels() : 0;   // the power-on sweep: each LED's level
      const anim = an ? Array.from(new Uint8Array(mem, an, 41)) : null;
      const key = leds.join(",") + (anim ? ";" + anim.join(",") : "");
      if (key !== this.sentLeds) {
        this.sentLeds = key;
        msg.leds = leds;
        msg.anim = anim;
      }
      if (this.frames >= 44100) {                     // the share of real time spent running the device
        msg.load = this.busy / (this.frames / 44.1);
        this.busy = this.frames = 0;
      }
      const transfer = [];
      const draws = ex.web_screen_draws();
      if (draws !== this.sentDraws) {
        this.sentDraws = draws;
        msg.fb = new Uint16Array(mem, ex.web_screen(), 240 * 240).slice();
        transfer.push(msg.fb.buffer);
      }
      const writes = ex.web_flash_writes_count();
      if (writes !== this.sentWrites) {               // a save is done within one main-loop pass: whole here
        this.sentWrites = writes;
        msg.sectors = storeSectors(new Uint8Array(mem, ex.web_nor(), ex.web_nor_size()));
      }
      if (ex.web_h2_state) {                         // the harmony monitor (am_harm2.c): its state and its log
        const st = [];
        for (let i = 0; i < 24; i++) st.push(ex.web_h2_state(i));
        const log = [];
        for (let j = 0; j < Math.min(st[16], 16); j++) log.push([ex.web_h2_log(j, 0), ex.web_h2_log(j, 1)]);
        const page = ex.web_am_page ? ex.web_am_page() : -1, key = st.join(",") + ";" + log.join(",") + ";" + page;
        if (key !== this.sentH2) { this.sentH2 = key; msg.h2 = { st, log, page, cad: ex.web_am_cad_page ? ex.web_am_cad_page() : -1 }; }
      }
      if (ex.web_gl_state) {                         // GLITCH's monitor (am_glitch.c)
        const g = [];
        for (let i = 0; i < 64; i++) g.push(ex.web_gl_state(i));
        const key = g.join(",");
        if (key !== this.sentGl) { this.sentGl = key; msg.gl = g; }
      }
      if (msg.leds || msg.fb || msg.sectors || msg.load !== undefined || msg.h2 || msg.gl) this.port.postMessage(msg, transfer);
    }

    // A click's down and up can arrive within one render quantum, but the FM-1's contacts never close for less than
    // its debounce, and the UI reads them once a frame: each change of the buttons or the keys stays HOLD_MS of the
    // device's time before the next one goes in (a tap: down for a frame and more, then up).
    input() {
      const ex = this.ex, now = ex.web_now_ms();
      while (this.queue.length && now - this.nextOk >= 0) {
        const m = this.queue.shift();
        if (m.type === "buttons") ex.web_buttons(m.mask >>> 0);
        else ex.web_keys(m.mask >>> 0);
        this.nextOk = now + HOLD_MS;
      }
    }

    process(inputs, outputs) {
      const ex = this.ex;
      if (!ex) return true;
      if (this.queue.length) this.input();
      const out = outputs[0], n = out[0].length;
      const t0 = clock();
      ex.web_render(n);
      this.busy += clock() - t0;
      this.frames += n;
      out[0].set(new Float32Array(ex.memory.buffer, ex.web_out_l(), n));
      if (out[1]) out[1].set(new Float32Array(ex.memory.buffer, ex.web_out_r(), n));
      if (currentTime - this.lastFrame >= this.period) {
        this.lastFrame = currentTime;
        this.publish();
      }
      return true;
    }
  }
  registerProcessor("drummatic", Felucca);
}
