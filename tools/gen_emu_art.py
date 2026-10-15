#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the emulator page's crest, in the VELLUM palette (build/gen/ui_palettes.h):
#   initial.png  the illuminated initial (tools/gen_estampie_art.py initial: ES_INIT and ES_INIT_RULE) at 4x
#   word.png     "stampie" (ES_WORD) at 4x
# GHOULBOX: the installer's, editor's and tools pages' pixel art (until they are restyled), in the CRYPT palette:
#   torch.png  the wall torch, the firmware's own (src/ui_graph.c torch_draw): its 4 frames side by side, 12 x 30 each
#   logo.png   "GHOULBOX" and skull.png, the boot screen's art drawn large (tools/gen_ghoulbox_art.py logo, skull: the
#              same word and skull as GB_LOGO, GB_SKULL, at 4x their size: smooth, the page shows them at 2x) in its
#              colours (THEME, TEXT)
#   wall.png   a tile of grey dungeon blocks, 64 x 32 (16 x 8 blocks, every row half a block along), its own seed
# The page scales them by whole numbers (image-rendering: pixelated).
#   gen_emu_art.py build/gen OUTDIR
import random
import re
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_ghoulbox_art import logo, skull  # noqa: E402
from gen_estampie_art import initial, word  # noqa: E402


def rgb565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def mix(a, b, pct):                                     # gfx.c ux_mix: a towards b, pct %
    return tuple(x + (y - x) * pct // 100 for x, y in zip(a, b))


def crypt(gen):
    m = re.search(r'\{"CRYPT",\s*(0x\w+),\s*(0x\w+),\s*(0x\w+),\s*(0x\w+),\s*(0x\w+)\}',
                  (gen / "ui_palettes.h").read_text())
    bg, surf, text, theme, accent = (rgb565(int(v, 16)) for v in m.groups())
    return {"bg": bg, "surf": surf, "text": text, "theme": theme, "accent": accent,
            "stone": mix(surf, bg, 45), "edge": mix(bg, theme, 45)}


def torch(c):
    """torch_draw(x = 0, y = 0, f) for f = 0..3, the frames side by side"""
    outer = [2, 2, 4, 4, 6, 6, 8, 8, 10, 10, 8, 6]
    midw = [0, 0, 0, 2, 2, 4, 4, 6, 6, 6, 6, 4]
    core = [0, 0, 0, 0, 0, 2, 2, 2, 4, 4, 2, 2]
    sway = [-1, 0, 1, 0]
    im = Image.new("RGBA", (48, 30), (0, 0, 0, 0))

    def rect(x, y, w, h, col):
        for j in range(h):
            for i in range(w):
                im.putpixel((x + i, y + j), col + (255,))
    for f in range(4):
        x0 = 12 * f
        for j in range(f & 1, 12):                      # (odd frames: one row shorter)
            cx = x0 + 6 + (sway[f] if j < 5 else 0)
            rect(cx - outer[j] // 2, j, outer[j], 1, c["accent"])
            if midw[j]:
                rect(cx - midw[j] // 2, j, midw[j], 1, c["theme"])
            if core[j] and j + (f & 2) // 2 < 12:
                rect(cx - core[j] // 2, j, core[j], 1, c["text"])
        for j in range(6):                              # the cup: narrowing
            rect(x0 + j // 2, 12 + j, 12 - (j // 2) * 2, 1, c["theme"] if j == 0 else c["edge"])
        rect(x0 + 4, 18, 4, 12, c["edge"])              # the handle
        rect(x0 + 5, 18, 1, 12, c["stone"])
    return im


def tinted(alpha, col):
    im = Image.new("RGBA", alpha.size, col + (0,))
    im.putalpha(alpha)
    return im


def wall():
    """grey blocks: 16 x 8 with a 1 px mortar line, lit from the top left (a light top and left edge, a dark
    bottom and right), each block a shade of its own and a few specks; rows offset by half a block"""
    rnd = random.Random(1137)
    mortar = (24, 24, 26)
    shades = [(70, 70, 74), (78, 78, 82), (64, 64, 68), (84, 83, 86)]
    im = Image.new("RGB", (64, 32), mortar)
    for row in range(4):
        off = 8 if row & 1 else 0
        for b in range(-1, 4):
            x0, y0 = b * 16 + off, row * 8
            base = rnd.choice(shades)
            hi = tuple(min(255, v + 16) for v in base)
            lo = tuple(max(0, v - 18) for v in base)
            dark = tuple(max(0, v - 10) for v in base)
            for y in range(7):                          # (row 7: the mortar under it)
                for x in range(15):                     # (column 15: the mortar beside it)
                    col = hi if (y == 0 or x == 0) else lo if (y == 6 or x == 14) else base
                    im.putpixel(((x0 + x) % 64, y0 + y), col)
            for _ in range(rnd.randint(3, 6)):          # specks, inside the edges
                im.putpixel(((x0 + rnd.randint(2, 12)) % 64, y0 + rnd.randint(1, 5)),
                            dark if rnd.random() < 0.7 else hi)
            if rnd.random() < 0.3:                      # now and then a crack
                x, y = rnd.randint(3, 10), rnd.randint(1, 3)
                for k in range(rnd.randint(2, 4)):
                    im.putpixel(((x0 + x + k) % 64, y0 + min(5, y + k // 2)), lo)
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
    c = crypt(gen)
    torch(c).save(out / "torch.png")
    tinted(logo(896, 208), c["theme"]).save(out / "logo.png")      # (GB_LOGO is logo(224, 52))
    tinted(skull(192, 224), c["text"]).save(out / "skull.png")      # (GB_SKULL is skull(48, 56))
    wall().save(out / "wall.png")


if __name__ == "__main__":
    main()
