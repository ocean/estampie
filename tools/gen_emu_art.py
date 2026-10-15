#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the web pages' crest (the emulator, the installer, the editor, the tools manual), in the VELLUM palette
# (build/gen/ui_palettes.h):
#   initial.png  the illuminated initial (tools/gen_estampie_art.py initial: ES_INIT and ES_INIT_RULE) at 4x
#   word.png     "stampie" (ES_WORD) at 4x
# (GHOULBOX's dungeon art, the torch, wall, skull and name, went with the pages' restyle; tools/gen_ghoulbox_art.py
# still draws the firmware's.) The pages show them at 2x.
#   gen_emu_art.py build/gen OUTDIR
import re
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_estampie_art import initial, word  # noqa: E402


def rgb565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def tinted(alpha, col):
    im = Image.new("RGBA", alpha.size, col + (0,))
    im.putalpha(alpha)
    return im


def vellum(gen):
    """ESTAMPIE: the VELLUM palette's colours (build/gen/ui_palettes.h)"""
    m = re.search(r'\{"VELLUM",\s*(0x\w+),\s*(0x\w+),\s*(0x\w+),\s*(0x\w+),\s*(0x\w+)\}',
                  (gen / "ui_palettes.h").read_text())
    bg, surf, text, theme, accent = (rgb565(int(v, 16)) for v in m.groups())
    return {"bg": bg, "surf": surf, "text": text, "theme": theme, "accent": accent}


def main():
    gen, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    v = vellum(gen)                                     # ESTAMPIE: the crest, 4x the boot screen's (drawn smooth)
    field, rule = initial(4)
    crest = tinted(field, v["accent"])
    crest.alpha_composite(tinted(rule, v["theme"]))
    crest.save(out / "initial.png")
    tinted(word("stampie", 156, 46, 4), v["theme"]).save(out / "word.png")


if __name__ == "__main__":
    main()
