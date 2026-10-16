#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the boot / ABOUT artwork as 4-bit alpha bitmaps (run by tools/build.py generate()), after GHOULBOX's
# tools/gen_ghoulbox_art.py. An illuminated initial and the rest of the word, as in a manuscript:
#   ES_INIT       the initial's field: a square of lapis with the "E" and four corner dots cut out (drawn in ACCENT)
#   ES_INIT_RULE  its frame: a double rule with a fleuron on each corner (drawn in THEME, vermilion)
#   ES_WORD       "stampie" in UnifrakturMaguntia (assets/fonts, SIL OFL 1.1), the boot size
#   ES_LOGO_S     "Estampie", the ABOUT size
#   ES_ORN_*      the marginal ornament beside the graphs (in GHOULBOX's torches' place), 12 x 30, the left one (the
#                 right one is it mirrored, cv_flip: both face the graph): a crozier's curl and stem (STEM, in TEXT, ink), a hanging ivy
#                 leaf with its vein cut out (LEAF, THEME, vermilion), a dot above and below (DOT, ACCENT, lapis)
# Alpha 0..15, rows back to back, Huffman-coded as the M and L faces (tools/gen_aa_font.py huff_pack: runs of 0 and 15,
# one code for all of them, ES_HC), each image's stream ending with the 2 spare bytes the decoder reads ahead; the
# firmware draws them with cv_alpha_hc in any colour. (Coded: 3.4 KB, not 10 KB as 2 px a byte.)
#   gen_estampie_art.py OUT.h
import math
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, str(Path(__file__).resolve().parent))
from aa_raster import c_array  # noqa: E402
from gen_aa_font import huff_pack, huff_unpack  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
FONT = ROOT / "assets/fonts/UnifrakturMaguntia-Book.ttf"
SS = 4                                                  # supersampling of the drawn art
INIT = 60                                               # the initial's square, px


def word(text, max_w, max_h, scale=1):
    """text as large as fits max_w x max_h (times scale), cropped to its ink"""
    max_w, max_h = max_w * scale, max_h * scale
    size = max_h * 2
    while True:
        f = ImageFont.truetype(str(FONT), size)
        x0, y0, x1, y1 = f.getbbox(text)
        if x1 - x0 <= max_w and y1 - y0 <= max_h:
            break
        size -= 1
    im = Image.new("L", (x1 - x0 + 2, y1 - y0 + 2), 0)
    ImageDraw.Draw(im).text((1 - x0, 1 - y0), text, font=f, fill=255)
    return im.crop(im.getbbox())


def initial(scale=1):
    """the field (lapis, the E cut out) and the rule (vermilion) of the initial, each INIT x INIT (times scale)"""
    n = INIT * scale
    W = n * SS
    u = W / 60.0                                        # drawn on a 60-unit grid
    field = Image.new("L", (W, W), 0)
    rule = Image.new("L", (W, W), 0)
    df, dr = ImageDraw.Draw(field), ImageDraw.Draw(rule)
    dr.rectangle([4 * u, 4 * u, 56 * u, 56 * u], outline=255, width=round(2 * u))   # the double rule
    dr.rectangle([7 * u, 7 * u, 53 * u, 53 * u], outline=255, width=round(1 * u))
    for cx, cy in ((4, 4), (56, 4), (4, 56), (56, 56)):                             # a fleuron on each corner
        for dx, dy in ((0, -1), (0, 1), (-1, 0), (1, 0)):
            x, y = (cx + dx * 2.6) * u, (cy + dy * 2.6) * u
            dr.ellipse([x - 1.9 * u, y - 1.9 * u, x + 1.9 * u, y + 1.9 * u], fill=255)
        dr.ellipse([(cx - 1.3) * u, (cy - 1.3) * u, (cx + 1.3) * u, (cy + 1.3) * u], fill=255)
    df.rectangle([9 * u, 9 * u, 51 * u, 51 * u], fill=255)                          # the lapis field
    e = word("E", 34 * SS * scale, 36 * SS * scale)                               # the letter, cut out
    ex, ey = (W - e.width) // 2, (W - e.height) // 2 + round(1 * u)
    field.paste(0, (ex, ey), e)
    for cx, cy in ((13, 13), (47, 13), (13, 47), (47, 47)):                         # white-vine dots in the corners
        df.ellipse([(cx - 1.4) * u, (cy - 1.4) * u, (cx + 1.4) * u, (cy + 1.4) * u], fill=0)
    return field.resize((n, n), Image.LANCZOS), rule.resize((n, n), Image.LANCZOS)


def hedera(d, cx, cy, r):
    """an ivy leaf (a heart), its tip down, centred on (cx, cy), r across each lobe"""
    pts = []
    for k in range(96):
        t = 2 * math.pi * k / 96
        x = 16 * math.sin(t) ** 3
        y = 13 * math.cos(t) - 5 * math.cos(2 * t) - 2 * math.cos(3 * t) - math.cos(4 * t)
        pts.append((cx + x * r / 16, cy - y * r / 16))
    d.polygon(pts, fill=255)


ORN_W, ORN_H = 12, 30


def ornament():
    """the left ornament's stem, leaf and dots, each ORN_W x ORN_H (the right one: the same, mirrored)"""
    u = SS * 2                                          # (fine lines: drawn at 8x)
    stem, leaf, dot = (Image.new("L", (ORN_W * u, ORN_H * u), 0) for _ in range(3))
    ds, dl, dd = ImageDraw.Draw(stem), ImageDraw.Draw(leaf), ImageDraw.Draw(dot)
    cx = 6 * u
    ds.arc([cx - 4 * u, 1 * u, cx + 4 * u, 9 * u], 180, 90, fill=255, width=int(1.1 * u))   # the crozier's curl
    ds.line([(cx, 9 * u), (cx, 18 * u)], fill=255, width=int(1.1 * u))                     # the stem
    hedera(dl, cx, 22 * u, 5.0 * u)                                                         # the leaf
    dl.line([(cx, 19.5 * u), (cx, 25.5 * u)], fill=0, width=int(0.6 * u))                   # its vein
    dd.ellipse([cx - 1.8 * u, 1 * u, cx + 1.8 * u, 4.6 * u], fill=255)                       # the dots
    dd.ellipse([cx - 1.6 * u, 27.4 * u, cx + 1.6 * u, 30 * u - 1], fill=255)
    return [im.resize((ORN_W, ORN_H), Image.LANCZOS) for im in (stem, leaf, dot)]


def nibbles(im):
    return [min(15, (v + 8) // 17) for v in im.tobytes()]


def main():
    out = Path(sys.argv[1])
    field, rule = initial()
    art = (("ES_INIT", field), ("ES_INIT_RULE", rule), ("ES_WORD", word("stampie", 156, 46)),
           ("ES_LOGO_S", word("Estampie", 180, 34))) + tuple(
              (f"ES_ORN_{part}", im) for part, im in zip(("STEM", "LEAF", "DOT"), ornament()))
    values = [nibbles(im) for _, im in art]
    hc, streams = huff_pack(values)
    L = ["/* generated by tools/gen_estampie_art.py: ESTAMPIE's boot / ABOUT art and the ornaments, 4-bit alpha,",
         " * Huffman-coded (cv_alpha_hc with ES_HC) */"]
    L += c_array("ES_HC", "uint8_t", hc, 24).splitlines()
    total = len(hc)
    for (name, im), v, data in zip(art, values, streams):
        assert huff_unpack(hc, data, len(v)) == v, name   # (the decoder's view of it)
        data = data + b"\0\0"                            # (cv_alpha_hc reads 2 bytes ahead)
        w, h = im.size
        L += [f"#define {name}_W {w}u", f"#define {name}_H {h}u"] + c_array(name, "uint8_t", list(data), 24).splitlines()
        total += len(data)
        print(f"estampie art: {name} {w} x {h}, {len(data)} B (raw {(w * h + 1) // 2} B)")
    out.write_text("\n".join(L) + "\n")
    print(f"estampie art: {total} B -> {out}")


if __name__ == "__main__":
    main()
