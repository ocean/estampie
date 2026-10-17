#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ESTAMPIE user guide (adapted from GHOULBOX's): docs/guide/guide.html, with the sound list from the firmware's own
tables, the site's address and the crest -> a web page or a PDF.

  python3 docs/guide/make_guide.py html OUTDIR    OUTDIR/index.html, its screenshots, controls.jpg and art/
  python3 docs/guide/make_guide.py pdf OUT.pdf    the same, printed by Chrome (A4)

Needs build/host/desc.json (tests/run_tests.sh writes it) and build/gen (./build.sh: the crest's art).
ESTAMPIE_SITE sets the site's address as the text shows it (default: ocean.github.io/estampie)."""
import html
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
SITE = os.environ.get("ESTAMPIE_SITE", "ocean.github.io/estampie")
SHOTS = ("boot", "home", "presets", "roll", "menu", "drone", "ring")   # tools/guide_shots.sh
WHAT = {"ANALOG": "virtual analog", "FM6": "6-operator FM", "PHASE": "phase distortion", "LOFI": "chiptune",
        "SAMPLE": "real recordings", "VOICE": "sung vowels", "TRIO": "three oscillators", "WHEEL": "tonewheel organ",
        "GRAIN": "granular", "PHYS": "physical models", "GURDY": "hurdy-gurdy", "NOISE": "noise and ambience",
        "DRUM": "drums"}


def own_presets(tool):
    """the preset names a generator (tools/ghoulbox_presets.py, tools/estampie_presets.py) adds"""
    return {m.group(1) for m in re.finditer(r'\("([A-Z0-9 ]+)", \[', (ROOT / "tools" / tool).read_text())}


def sound_table():
    d = json.loads((ROOT / "build/host/desc.json").read_text())
    es = own_presets("estampie_presets.py")
    gb = own_presets("ghoulbox_presets.py") | {     # GHOULBOX's own, beyond its generator: GURDY, FM6 F9.., SAMPLE 5..
        "FRENCH HORN", "VIRGINAL", "GLASS CHOIR", "DARK STRINGS", "LOFI FLUTE", "PIPE ORGAN", "TUBA", "BOWED PSALT",
        "REN ORGAN", "TENOR RECORD", "FOLK HARP", "CRYPT KIT", "TOMB DRUMS", "HURDY GURDY", "DRONE WHEEL", "DANCE GURDY",
        "TROMPETTE", "VIELLE", "BAGPIPE"}
    hidden = {}
    for e, n in d["HIDDEN"]:
        hidden.setdefault(e, set()).add(n)
    rows, total = [], 0
    for name in d["ORDER"]:
        eng = next(e for e in d["ENG"] if e["name"] == name)
        seen, cells = set(), []
        for p in eng["presets"]:
            n = p["name"]
            if n in hidden.get(name, set()) or n in seen:
                continue
            seen.add(n)
            tag = ' <span class="es">ES</span>' if n in es else ' <span class="gb">GB</span>' if n in gb else ""
            cells.append(html.escape(n) + tag)
        total += len(cells)
        rows.append(f'<tr><td><span class="k">{name}</span><br><span class="small">{WHAT.get(name, "")}</span></td>'
                    f'<td>{" · ".join(cells)}</td></tr>')
    return '<table><tr><th>Engine</th><th>Sounds (in PRESETS order)</th></tr>' + "".join(rows) + "</table>", total


def version():
    m = re.search(r'#define ESTAMPIE_VERSION "([^"]+)"', (ROOT / "firmware/src/core.h").read_text())
    return m.group(1)


def build(out, site_root="../"):
    """the guide as a page in out/ (index.html), with what it shows; site_root: the site's top, from out/"""
    out = Path(out)
    out.mkdir(parents=True, exist_ok=True)
    table, total = sound_table()
    src = (HERE / "guide.html").read_text(encoding="utf-8")
    for k, v in {"SOUNDS": table, "NSOUNDS": str(total), "SITE": SITE, "SITE_ROOT": site_root,
                 "VERSION": version()}.items():
        src = src.replace("{{" + k + "}}", v)
    left = re.findall(r"\{\{[A-Z_]+\}\}", src)
    if left:
        raise SystemExit(f"guide.html: unfilled {sorted(set(left))}")
    (out / "index.html").write_text(src, encoding="utf-8")
    for n in SHOTS:
        shutil.copy(HERE / f"{n}.png", out / f"{n}.png")
    shutil.copy(ROOT / "docs/controls.jpg", out / "controls.jpg")
    subprocess.run([sys.executable, str(ROOT / "tools/gen_emu_art.py"), str(ROOT / "build/gen"), str(out / "art")],
                   check=True, stdout=subprocess.DEVNULL)
    return total


def chrome():
    for c in ("google-chrome-stable", "google-chrome", "chromium"):
        if shutil.which(c):
            return shutil.which(c)
    mac = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
    if Path(mac).exists():
        return mac
    raise SystemExit("make_guide: no Chrome (google-chrome, chromium, or Google Chrome.app)")


def pdf(out):
    out = Path(out).resolve()
    with tempfile.TemporaryDirectory() as tmp:
        total = build(tmp)
        subprocess.run([chrome(), "--headless=new", "--disable-gpu", "--no-pdf-header-footer",
                        "--allow-file-access-from-files", "--virtual-time-budget=10000", f"--print-to-pdf={out}",
                        (Path(tmp) / "index.html").as_uri()], check=True, capture_output=True)
    return total


if __name__ == "__main__":
    if len(sys.argv) != 3 or sys.argv[1] not in ("html", "pdf"):
        sys.exit(__doc__)
    n = (build if sys.argv[1] == "html" else pdf)(sys.argv[2])
    print(f"guide: {n} sounds -> {sys.argv[2]}")
