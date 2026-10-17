// SPDX-License-Identifier: GPL-3.0-only
// ESTAMPIE sound lab: felucca.wasm (the whole firmware, web/emu/felucca_web.c) in an AudioWorklet, driven by the lab
// page (lab.html) instead of the panel. The page sends batches of operations (an engine, a parameter, a step, the
// transport, a part of the TROTTO); each batch is applied between two render quanta and answered with the track's
// controls as the firmware describes them (label, range, value, the value as the screen shows it).

const PID_NAMES = ["LEVEL", "ATK", "DEC", "SUS", "REL", "ED_FLT", "VOICE", "PAN", "MUTE", "DIST", "CHOR", "DLY", "REV",
                   "SLEN", "SDIV", "TEMP", "RING", "E0"];   // felucca_web.c WEB_PIDS, in its order

class Lab extends AudioWorkletProcessor {
  constructor() {
    super();
    this.ex = null;
    this.port.onmessage = (e) => this.onMessage(e.data);
  }

  str(p) {
    if (!p) return null;
    const m = new Uint8Array(this.ex.memory.buffer);
    let e = p;
    while (m[e]) e++;
    return String.fromCharCode(...m.subarray(p, e));
  }

  control(k, id) {
    const ex = this.ex, v = ex.web_track_get(k, id);
    return { id, label: this.str(ex.web_desc_label(k, id)), min: ex.web_desc_min(k, id), max: ex.web_desc_max(k, id),
             value: v, text: this.str(ex.web_desc_format(k, id, v)) };
  }

  state(k) {
    const P = this.pid, eng = this.ex.web_track_engine_of(k);
    const ids = (names) => names.map((n) => this.control(k, P[n]));
    return {
      type: "state", k, eng,
      engine: Array.from({ length: 8 }, (_, i) => this.control(k, P.E0 + i)),
      env: ids(["ATK", "DEC", "SUS", "REL", "ED_FLT"]),
      sends: ids(["DIST", "CHOR", "DLY", "REV"]),
      mix: ids(["LEVEL", "PAN", "VOICE", "TEMP", "RING"]),
      mute: this.ex.web_track_get(k, P.MUTE),
    };
  }

  apply(op) {
    const ex = this.ex, [name, ...a] = op;
    if (name === "engine") ex.web_track_engine(a[0], a[1]);
    else if (name === "set") ex.web_track_set(a[0], typeof a[1] === "string" ? this.pid[a[1]] : a[1], a[2]);
    else if (name === "setE") ex.web_track_set(a[0], this.pid.E0 + a[1], a[2]);   // the engine's control a[1] (0..7)
    else if (name === "step") ex.web_track_step(a[0], a[1], a[2], a[3] || 0);
    else if (name === "part") ex.web_scene_part(a[0]);
    else if (name === "scene") ex.web_scene(a[0]);
    else if (name === "transport") ex.web_transport(a[0]);
  }

  async onMessage(m) {
    if (m.type === "load") {
      try {
        const { instance } = await WebAssembly.instantiate(m.wasm, {});
        const ex = instance.exports;
        ex._initialize();
        ex.web_nor_erase();                          // a fresh FM-1: the TROTTO at power-on
        ex.web_master(m.master ?? 760);
        ex.web_boot();
        this.ex = ex;
        this.pid = {};
        PID_NAMES.forEach((n, i) => { this.pid[n] = ex.web_pid(i); });
        const engines = [];
        for (let e = 0; e < ex.web_engine_count(); e++) {
          const n = this.str(ex.web_engine_name(e));
          if (n) engines.push({ e, name: n });
        }
        this.port.postMessage({ type: "ready", engines, states: [0, 1, 2, 3].map((k) => this.state(k)) });
      } catch (err) {
        this.port.postMessage({ type: "error", message: String((err && err.message) || err) });
      }
      return;
    }
    if (!this.ex) return;
    if (m.type === "ops") {
      for (const op of m.ops) this.apply(op);
      if (m.want !== undefined) this.port.postMessage(this.state(m.want));
    } else if (m.type === "master") {
      this.ex.web_master(m.value);
    }
  }

  process(_in, outputs) {
    const out = outputs[0], ex = this.ex;
    if (!ex || !out || !out[0]) return true;
    const n = out[0].length;
    ex.web_render(n);
    const l = new Float32Array(ex.memory.buffer, ex.web_out_l(), n), r = new Float32Array(ex.memory.buffer, ex.web_out_r(), n);
    out[0].set(l);
    (out[1] || out[0]).set(out[1] ? r : l);
    return true;
  }
}

registerProcessor("estampie-lab", Lab);
