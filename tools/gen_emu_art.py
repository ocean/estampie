#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the web pages' crest (the emulator, the installer, the editor, the tools manual), in the VELLUM palette
# (build/gen/ui_palettes.h):
#   initial.png  the illuminated initial (tools/gen_estampie_art.py initial: ES_INIT and ES_INIT_RULE) at 4x
#   word.png     "stampie" (ES_WORD) at 4x
#   favicon.ico  the pages' icon: the initial simplified to read at 16..48 px (mark: a vermilion frame, the lapis
#                field, a larger E), on vellum so it shows on a dark tab bar too
#   icon-180.png the full initial on a vellum square (a phone's home screen: apple-touch-icon)
# (GHOULBOX's dungeon art, the torch, wall, skull and name, went with the pages' restyle; tools/gen_ghoulbox_art.py
# still draws the firmware's.) The pages show them at 2x.
#   gen_emu_art.py build/gen OUTDIR
import re
import struct
import sys
import zlib
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent))
from gen_estampie_art import SS, initial, word  # noqa: E402


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


def mark(n, v):
    """the initial for a tab: n x n, opaque; the frame and field as the boot screen's, without the rule's second line,
    the fleurons and the dots (lost at 16 px), the E larger"""
    W = n * SS * 4
    u = W / 32.0                                        # drawn on a 32-unit grid
    im = Image.new("RGBA", (W, W), v["theme"] + (255,))
    d = ImageDraw.Draw(im)
    d.rectangle([2 * u, 2 * u, 30 * u, 30 * u], fill=v["bg"])                       # a vellum line inside the frame
    d.rectangle([3 * u, 3 * u, 29 * u, 29 * u], fill=v["accent"])                   # the lapis field
    e = word("E", round(22 * u), round(24 * u))
    im.paste(v["bg"] + (255,), ((W - e.width) // 2, (W - e.height) // 2 + round(0.5 * u)), e)
    return im.resize((n, n), Image.LANCZOS)


def on_vellum(crest, n, v):
    """the crest centred on an n x n vellum square, a tenth of it margin each side"""
    im = Image.new("RGBA", (n, n), v["bg"] + (255,))
    c = crest.resize((n * 8 // 10, n * 8 // 10), Image.LANCZOS)
    im.alpha_composite(c, ((n - c.width) // 2, (n - c.height) // 2))
    return im


# PNG and ICO written here, with Python's own zlib, not Pillow's: each platform's Pillow bundles its own zlib, which
# compresses the same pixels to other bytes, and the site zip must be the same wherever it is built (BUILDING.md)
def png_bytes(im):
    """an RGB or RGBA image as a PNG: 8 bits a channel, no filter, zlib level 9"""
    ct = {"RGB": 2, "RGBA": 6}[im.mode]
    w, h = im.size
    row = w * len(im.mode)
    px = im.tobytes()
    raw = b"".join(b"\0" + px[y * row:(y + 1) * row] for y in range(h))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, ct, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))


def save_png(im, path):
    Path(path).write_bytes(png_bytes(im))


def save_ico(images, path):
    """an ICO of square RGBA images, each held as a PNG (as Pillow writes them)"""
    pngs = [png_bytes(im) for im in images]
    head = struct.pack("<HHH", 0, 1, len(images))
    off = 6 + 16 * len(images)
    for im, p in zip(images, pngs):
        s = im.size[0]
        head += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(p), off)
        off += len(p)
    Path(path).write_bytes(head + b"".join(pngs))


def main():
    gen, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    v = vellum(gen)                                     # ESTAMPIE: the crest, 4x the boot screen's (drawn smooth)
    field, rule = initial(4)
    crest = tinted(field, v["accent"])
    crest.alpha_composite(tinted(rule, v["theme"]))
    save_png(crest, out / "initial.png")
    save_png(tinted(word("stampie", 156, 46, 4), v["theme"]), out / "word.png")
    big = initial(8)                                    # (the home-screen icon: drawn at 8x, then scaled down)
    crest8 = tinted(big[0], v["accent"])
    crest8.alpha_composite(tinted(big[1], v["theme"]))
    save_png(on_vellum(crest8, 180, v).convert("RGB"), out / "icon-180.png")
    save_ico([mark(16, v), mark(32, v), mark(48, v)], out / "favicon.ico")
    save_png(mark(32, v), out / "favicon-32.png")


if __name__ == "__main__":
    main()
