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
    "DRUM": [  # KIT (10: DANCE, eng_drum.c DK_DANCE) TUNE TONE DECY SNAP ACC KICK DRV
        # The DANCE kit as designed, in a small room; and lower, longer, roomier for a procession or a carol.
        ("DANCE KIT", [10, 64, 64, 64, 64, 100, 0, 0], [0, 100, 127, 100], 0, 0, (0, 0, 0, 30), 23),
        ("PROCESSION", [10, 52, 56, 80, 64, 100, 0, 0], [0, 100, 127, 100], 0, 0, (0, 0, 0, 60), 23),
    ],
    "TRIO": [  # WAVE (0 SAW3, 1 PLS3 ..) INT2 INT3 DTN | MODE (LP BP HP NOT) CUT RES PW
        # The sound lab's capped reeds (tests/sound_lab.c). RAUSCHPFEIFE: saws an octave and a twelfth up through a wide
        # open low-pass, round where SHAWM's band-pass bites (the ESTAMPIE scene's tune). CRUMHORN: unison squares, a
        # nasal resonance low down.
        ("RAUSCHPFEIFE", [0, 12, 19, 5, 0, 104, 22, 64], [2, 70, 120, 18], 0, 1, (10, 0, 0, 40), 19),
        ("CRUMHORN", [1, 0, 0, 3, 0, 58, 62, 64], [3, 70, 120, 20], 0, 1, (6, 0, 0, 45), 19),
    ],
    "PHYS": [  # SYMP: MODEL (3) CHRD BRIT DAMP | SYMP ACC BUZZ EXC
        # The sound lab's: a psaltery's strings an octave up ringing along, long and bright; a gothic harp's fifths with
        # its bray pins' buzz.
        ("PSALTERY", [3, 4, 100, 96, 70, 120, 0, 20], [0, 100, 127, 95], 0, 0, (0, 20, 15, 75), 3),
        ("BRAY HARP", [3, 20, 82, 86, 50, 120, 72, 10], [0, 100, 127, 90], 0, 0, (0, 15, 15, 75), 3),
    ],
    "WHEEL": [  # REG SUB BODY TOP | PERC CLICK DRV ROTR
        # The sound lab's: a small organ on the player's knee, flutes with the top up and a chiff, no rotor.
        ("PORTATIVE", [0, -4, 0, 4, 0, 70, 6, 0], [4, 64, 127, 30], 0, 0, (0, 10, 0, 60), 5),
    ],
}

# Felucca-Engines' engines are vendored unchanged (firmware/engines/VENDORED.md): their presets after the engine's own
# go in firmware/src/engine_presets.h, each engine's ESTAMPIE_<ENGINE>_PRESETS macro.
VENDORED = {"ANALOG", "LOFI", "TRIO", "WHEEL", "NOISE"}


def write_h(eng, presets):
    """the block in engine_presets.h's ESTAMPIE_<eng>_PRESETS (a macro: every line ends with a backslash)"""
    path = gb.ROOT / "firmware/src/engine_presets.h"
    s = path.read_text()
    i = s.index(f"#define ESTAMPIE_{eng}_PRESETS \\\n")
    j = s.index("\n\n", i)                              # the macro's end: its last line, then a blank one
    block = (f"\n    /* {BEGIN} */ \\\n" + "".join(gb.c_line(p) + " \\\n" for p in presets) + f"    /* {END} */ \\")
    m = gb.block_re("/*", "*/ \\", BEGIN, END).search(s, i, j)
    path.write_text(s[:m.start()] + block + s[m.end():] if m else s[:j] + block + s[j:])


def main():
    html = gb.ROOT / "web/editor.html"
    s = html.read_text()
    for eng, presets in PRESETS.items():
        for p in presets:
            gb.check(eng, p)
        if eng in VENDORED:
            write_h(eng, presets)
        else:
            gb.write_c(eng, presets, BEGIN, END)
        s = gb.write_js(s, eng, presets, BEGIN, END)
    html.write_text(s)
    print(f"estampie presets: {sum(len(v) for v in PRESETS.values())} in {len(PRESETS)} engines")


if __name__ == "__main__":
    sys.exit(main())
