#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""ESTAMPIE: the ABOUT screen's QR codes (firmware/src/ui_menu.c ABOUT_QR_*): ESTAMPIE's site at the top, Felucca's
beside its credit. Each address as a QR matrix, version 3 (29 x 29, what the screen's 74 px box holds at 2 px a module
with its 4-module quiet zone), correction M, written into ui_menu.c between its BEGIN / END lines as one word a row,
the leftmost module in bit 0 (the constants need no encoder or RAM on the device).

  python3 tools/gen_about_qr.py            (segno, uv's qr group)

tests/about_qr_test.py reads the rendered screen back, illumination and all."""
from pathlib import Path

import segno

ROOT = Path(__file__).resolve().parents[1]
CODES = {                                            # the table's name: its address
    "ABOUT_QR_SITE": "https://fm-1.drewr.dev/estampie/",
    "ABOUT_QR_FELUCCA": "https://hugelton.github.io/Felucca",
}
BEGIN, END = "/* BEGIN about_qr (tools/gen_about_qr.py) */", "/* END about_qr */"


def matrix(url):
    q = segno.make(url, version=3, error="m", boost_error=False, micro=False)
    return [[bool(v) for v in row] for row in q.matrix]


def table(name, url):
    m = matrix(url)
    assert len(m) == 29 and all(len(r) == 29 for r in m), "version 3: 29 x 29"
    words = [sum(1 << c for c, v in enumerate(row) if v) for row in m]
    body = "".join(("\n    " if i % 6 == 0 else " ") + f"0x{w:08X}u," for i, w in enumerate(words))
    return f"/* {url}, QR version 3, correction M */\nstatic const uint32_t {name}[29] = {{{body}\n}};"


def main():
    src = ROOT / "firmware/src/ui_menu.c"
    s = src.read_text(encoding="utf-8")
    if s.count(BEGIN) != 1 or s.count(END) != 1:
        raise SystemExit(f"ui_menu.c: needs one {BEGIN} and one {END}")
    a, b = s.index(BEGIN) + len(BEGIN), s.index(END)
    s = s[:a] + "\n" + "\n".join(table(n, u) for n, u in CODES.items()) + "\n" + s[b:]
    src.write_text(s, encoding="utf-8")
    for n, u in CODES.items():
        print(f"about qr: {n} {u}")


if __name__ == "__main__":
    main()
