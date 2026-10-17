// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The FM-1 USB-MIDI update protocol over WebMIDI (after tools/fm1_install.py).
// Step 1: the running firmware (official
// or Felucca) reads parts of the package and stages the update loader. Step 2:
// the loader reads the whole image and writes it. Both steps are "the device
// asks, we answer": cmd 0x30 read requests on the logical image.

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const MAXDATA = 512;
const FINISHED = [...new TextEncoder().encode("success"), 0];   // the answer to a finish request
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_OTA = (id) => /^ota-/i.test(id.model);

// ESTAMPIE: how far a loader has written, after FM1Kit's WriteProgress (github.com/ocean/FM1Kit, MIT): loaders read in
// their own order (V15's backwards, Felucca's forwards, then all of it again), so neither the furthest address nor a
// count of requests says how far it is. Progress is the share of the application area (fm1pkg.js applicationRange)
// read at least once; reads outside it (headers, the boot area some loaders rewrite first) leave it where it is. Once
// half is covered, three full-size reads in a row of bytes already read start the loader's check: its fraction is the
// share read again; a read of new ground goes back to writing. The time left comes from the rate so far.
export class WriteProgress {
  constructor(range, now = () => Date.now()) {
    this.start = range.start;
    this.size = Math.max(1, range.end - range.start);
    this.seen = new Uint8Array(this.size);                // 1 written, 2 read again (one byte each: 0.6 MB)
    this.covered = 0;
    this.rechecked = 0;
    this.phase = "writing";
    this.repeats = 0;
    this.requests = 0;
    this.now = now;
    this.t0 = null;
  }

  // a read of len bytes at addr (the logical image); returns this
  read(addr, len) {
    this.requests++;
    const a = Math.max(addr, this.start) - this.start, b = Math.min(addr + len, this.start + this.size) - this.start;
    if (b <= a) return this;
    if (this.t0 === null) this.t0 = this.now();
    let fresh = 0, again = 0;
    for (let i = a; i < b; i++) {
      if (this.seen[i] === 0) { this.seen[i] = 1; fresh++; } else if (this.seen[i] === 1) { this.seen[i] = 2; again++; }
    }
    this.covered += fresh;
    this.rechecked += again;
    if (fresh) {
      this.phase = "writing";
      this.repeats = 0;
    } else if (this.phase === "writing" && len >= MAXDATA && this.covered * 2 >= this.size && ++this.repeats >= 3) {
      this.phase = "checking";
    }
    return this;
  }

  get fraction() {
    return this.phase === "checking" ? Math.min(1, this.rechecked / Math.max(1, this.covered)) : this.covered / this.size;
  }

  // seconds left in this phase from the rate so far (null until there is a rate to go on)
  get secondsLeft() {
    const f = this.fraction, t = this.t0 === null ? 0 : (this.now() - this.t0) / 1000;
    if (this.phase !== "writing" || f < 0.05 || t < 1) return null;
    return Math.max(0, Math.round(t * (1 - f) / f));
  }
}

// errors carry a code the pages translate: notfound, model, stopped, noloader, lost, noreturn,
// mismatch (detail: the identity the device reports), badreq
function fail(code, msg, detail = "") { const e = new Error(msg); e.code = code; e.detail = detail; return e; }

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

function response(addr, data, fl = 0) {
  const len = data.length;
  const body = [0x00, 0x59, 0x30, (len + 8) & 0xFF, ((len + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    len & 0xFF, (len >> 8) & 0xFF, 0, ...data];
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

function parseRequest(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

function parseIdentity(pkt) {
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
  const m = /^([^_]+)_(\d+)$/.exec(txt);
  return m ? { text: txt, model: m[1], version: parseInt(m[2], 10) } : null;
}

// one MIDI in/out pair with a SysEx queue
class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.waiter = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  // false when the port is gone (unplugged): WebMIDI throws on a disconnected output
  send(bytes) {
    try { this.output.send(bytes); return true; } catch (_) { return false; }
  }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, ms); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
    return this.q.shift() || null;
  }
  close() { this.input.onmidimessage = null; }
}

async function handshake(link, tries = 3) {
  link.drain();
  for (let t = 0; t < tries; t++) {
    if (!link.send(HS_QUERY)) return null;
    const end = Date.now() + 1000;
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const id = parseIdentity(p);
      if (id) return id;
    }
  }
  return null;
}

export class Updater {
  constructor(access, log) {
    this.access = access;
    this.log = log || (() => {});
  }

  // every in/out pair with the same port name that answers the handshake
  async find(filter) {
    const outs = [...this.access.outputs.values()];
    for (const input of this.access.inputs.values()) {
      if (input.state === "disconnected") continue;
      if (!/fm-1|felucca|ota|composite|sinco|usb-midi/i.test(input.name || "")) continue;   // never probe other gear
      const output = outs.find((o) => o.name === input.name && o.state !== "disconnected");
      if (!output) continue;
      try { await input.open(); await output.open(); } catch (_) { continue; }
      const link = new Link(input, output);
      const id = await handshake(link, 2);
      if (id && (!filter || filter(id))) return { link, id, name: input.name };
      link.close();
    }
    return null;
  }

  // halfReload (GHOULBOX): Chrome on Linux gives an open page only the output of a port that came back (the FM-1
  // rebooted into its loader); the input never appears there, though the system has it and a new page sees both.
  // When an FM-1 output stays without its input for 3 tries, stop with "reload": the page reloads and resumes.
  async waitFor(filter, ms, halfReload = false) {
    const end = Date.now() + ms;
    let half = 0;
    while (Date.now() < end) {
      const dev = await this.find(filter);
      if (dev) return dev;
      if (halfReload && this.halfPort()) {
        if (++half >= 3) throw fail("reload", "the update loader is there, but this page cannot hear it: reloading to continue");
      } else {
        half = 0;
      }
      await sleep(1000);
    }
    return null;
  }

  // an FM-1 port listed as an output with no input of its name (GHOULBOX: waitFor's halfReload)
  halfPort() {
    const ins = new Set([...this.access.inputs.values()].filter((p) => p.state !== "disconnected").map((p) => p.name));
    return [...this.access.outputs.values()].some((o) => o.state !== "disconnected" &&
      /fm-1|felucca|ota|composite|sinco|usb-midi/i.test(o.name || "") && !ins.has(o.name));
  }

  // serve read requests until the device asks for one of the finish addresses; stops early when the port disappears
  // (lost: true) instead of waiting out idleMs. ESTAMPIE (after FM1Kit): every read answered at once (the loader paces
  // itself: no pause, about twice as fast), onRead(addr, len) for each; the finish answered "success", then the port kept
  // 600 ms for a repeated finish (settle); a finish for the other step stops the update
  async serve(link, image, finishAddr, idleMs, onRead) {
    let served = 0, last = Date.now();
    for (;;) {
      const pkt = await link.read(1000);
      if (!pkt) {
        if (link.lost) return { served, finished: false, lost: true };
        if (Date.now() - last > idleMs) return { served, finished: false };
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      last = Date.now();
      if (r.addr === 0xE0000000 || r.addr === 0xF0000000) {
        link.send(response(r.addr, FINISHED));
        if (r.addr !== finishAddr) throw fail("stopped", `the device finished ${r.addr === 0xE0000000 ? "staging" : "writing"} out of turn`);
        await this.settle(link);
        return { served, finished: true };
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) throw fail("badreq", `bad request ${r.addr.toString(16)}+${r.len}`);
      if (!link.send(response(r.addr, Array.from(image.subarray(r.addr, r.addr + r.len)), r.fl))) {
        return { served, finished: false, lost: true };
      }
      served++;
      if (onRead) onRead(r.addr, r.len);
    }
  }

  // after a finish: 600 ms more answering a repeated finish (a loader can ask again), then the port is closed
  async settle(link) {
    const end = Date.now() + 600;
    while (Date.now() < end && !link.lost) {
      const pkt = await link.read(end - Date.now());
      const r = pkt && parseRequest(pkt);
      if (r && (r.addr === 0xE0000000 || r.addr === 0xF0000000)) link.send(response(r.addr, FINISHED));
    }
  }

  // full install: running firmware -> loader -> new firmware. onStep(name, detail): start (identity), prepare (requests:
  // the running firmware stages the loader, no percentage), loader (waiting for it), found (its identity), write (a
  // WriteProgress: writing or checking, fraction, seconds left), reboot, done (identity). range: the application area
  // (fm1pkg.js applicationRange; default the whole image)
  async install(image, product, onStep, range = null) {
    const step = onStep || (() => {});
    const dev = await this.find((id) => !IS_OTA(id));
    if (!dev) throw fail("notfound", "FM-1 not found (USB cable, and no other app using it?)");
    const [model] = product.split("_");
    if (dev.id.model !== model) { dev.link.close(); throw fail("model", `the device is ${dev.id.text}, the package is for ${product}`); }
    step("start", dev.id.text);
    dev.link.send(UPGRADE);
    await sleep(2000);
    let staged = 0;
    const s1 = await this.serve(dev.link, image, 0xE0000000, 8000, () => step("prepare", ++staged));
    dev.link.close();
    if (!s1.finished) throw fail(s1.lost ? "lost" : "stopped", `the device ${s1.lost ? "was disconnected" : "stopped"} after ${s1.served} requests: nothing was written`);
    step("loader");
    await sleep(3000);
    const ota = await this.waitFor((id) => id.model === "ota-" + model, 30000, true);
    if (!ota) throw fail("noloader", "the update loader did not appear. Replug the USB cable and press Install again: the device stays in update mode until it is finished.");
    step("found", ota.id.text);
    const s2 = await this.write(ota, image, step, range);
    if (!s2.finished) throw fail(s2.lost ? "lost" : "stopped", `the loader ${s2.lost ? "was disconnected" : "stopped"} after ${s2.served} requests. Replug and press Install again to resume.`);
    return this.back(model, product, step);
  }

  // the new firmware answering again, and reporting the package's identity
  async back(model, product, step) {
    step("reboot");
    await sleep(3000);
    const back = await this.waitFor((id) => !IS_OTA(id) && id.model === model, 40000);
    if (!back) throw fail("noreturn", "the device did not come back: power-cycle it");
    back.link.close();
    if (back.id.text !== product) throw fail("mismatch", `installed, but the device reports ${back.id.text}`, back.id.text);
    step("done", back.id.text);
    return back.id.text;
  }

  // step 2 with the loader found by find()/waitFor(): its reads into a WriteProgress, reported as "write"
  async write(ota, image, step, range = null) {
    const progress = new WriteProgress(range || { start: 0, end: image.length });
    ota.link.send(UPGRADE);
    await sleep(2000);
    const s2 = await this.serve(ota.link, image, 0xF0000000, 60000, (addr, len) => step("write", progress.read(addr, len)));
    ota.link.close();
    return s2;
  }

  // resume: the device is already in update mode (loader) -> true when the write finished
  // waitMs (GHOULBOX): look for the loader that long (a page reloaded to reach it can list it a moment late); 0: once
  async resume(image, onStep, product = null, waitMs = 0, range = null) {
    const model = product && product.split("_")[0];
    const isOta = (id) => model ? id.model === "ota-" + model : IS_OTA(id);
    const ota = waitMs ? await this.waitFor(isOta, waitMs) : await this.find(isOta);
    if (!ota) return false;
    const step = onStep || (() => {});
    step("found", ota.id.text);
    const result = await this.write(ota, image, step, range);
    if (!result.finished) return false;
    if (product) await this.back(model, product, step);
    return true;
  }
}
