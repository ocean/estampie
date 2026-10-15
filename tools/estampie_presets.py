#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: its own presets, one table -> the engines' preset arrays (firmware/src/eng_*.c) and the web editor's mirror
# (web/editor.html), in a block after GHOULBOX's (tools/ghoulbox_presets.py, whose writers this reuses). A project
# stores a preset's index: never reorder or remove, only append. Add each new preset's category letter to
# firmware/src/category.c PRESET_CAT too (tests/ui_test.c checks the counts). Run again after an edit.
#   uv run tools/estampie_presets.py
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import ghoulbox_presets as gb  # noqa: E402  (the writers and checks)

BEGIN, END = "ESTAMPIE presets (tools/estampie_presets.py)", "ESTAMPIE end"

# name, EDIT 1..8, {A D S R}, filter env, mono, FX(dist, chorus, delay, reverb), suggested pattern
PRESETS = {
    "GURDY": [  # DRN (C .. B) DLVL 5TH WHL | BUZZ COUP (HOLD 1/4 1/8 1/8T 1/16) BODY WOBL
        # The TROTTO scene's: drones on A and E for A Dorian, the trompette struck once a beat (COUP 1/4: in a 6/8 scene's
        # 8T steps, each dotted quarter), a bright wheel, little reverb: a dance, not a crypt.
        ("TROTTO GURDY", [9, 70, 55, 110, 100, 1, 55, 15], [2, 64, 127, 20], 0, 1, (8, 0, 0, 40), 21),
    ],
}


def main():
    html = gb.ROOT / "web/editor.html"
    s = html.read_text()
    for eng, presets in PRESETS.items():
        for p in presets:
            gb.check(eng, p)
        gb.write_c(eng, presets, BEGIN, END)
        s = gb.write_js(s, eng, presets, BEGIN, END)
    html.write_text(s)
    print(f"estampie presets: {sum(len(v) for v in PRESETS.values())} in {len(PRESETS)} engines")


if __name__ == "__main__":
    sys.exit(main())
