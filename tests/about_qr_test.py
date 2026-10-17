#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ESTAMPIE: the ABOUT screen's two QR codes, illuminated (ui_menu.c draw_about_qr), read back from the rendered screen
in every palette (tests/ui_render.c: OUTDIR/ppm/<PALETTE>_about.ppm, at the top, and _about_felucca.ppm, scrolled to
Felucca's) as a phone would see it: sharp, then blurred and dimmed like a camera's photograph of the screen. Each
screen must give exactly one address, its own: the codes are never both whole on the screen, so each scans alone.

  python3 tests/about_qr_test.py build/ui_new        (zxing-cpp: uv's qr group)"""
import sys
from pathlib import Path

import zxingcpp
from PIL import Image, ImageEnhance, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
from gen_about_qr import CODES  # noqa: E402

SCREENS = {"about": {CODES["ABOUT_QR_SITE"]}, "about_felucca": {CODES["ABOUT_QR_FELUCCA"]}}


def reads(img):
    return {r.text for r in zxingcpp.read_barcodes(img) if r.format == zxingcpp.BarcodeFormat.QRCode}


def main(out):
    shots = [p for p in sorted((Path(out) / "ppm").glob("*_about*.ppm"))
             if p.stem.split("_", 1)[1] in SCREENS and p.stem.split("_")[0].isalpha()]   # (the palettes, not STYLE_*)
    if not shots:
        sys.exit(f"about qr: no {out}/ppm/*_about.ppm (tests/ui_render.c first)")
    bad = 0
    for p in shots:
        screen = Image.open(p).convert("RGB")
        big = screen.resize((960, 960), Image.NEAREST)                  # (the screen as a camera frames it)
        photo = ImageEnhance.Contrast(big.filter(ImageFilter.GaussianBlur(3))).enhance(0.6)
        got = {"sharp": reads(big), "photographed": reads(photo)}
        want = SCREENS[p.stem.split("_", 1)[1]]
        ok = all(v == want for v in got.values())
        bad += not ok
        print(f"about qr: {p.stem:22s} {'ok' if ok else 'FAIL'}  {got if not ok else next(iter(want))}")
    print(f"about qr: {len(shots)} screens, {bad} failed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "build/ui_new"))
