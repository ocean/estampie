#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site: ESTAMPIE's folder of https://fm-1.drewr.dev/ (Drew's FM-1 projects, each in a folder of its own:
/estampie/ is this one). Every link is relative, so OUT_DIR works anywhere; ESTAMPIE_SITE changes only the address
the guide's text shows (docs/guide/make_guide.py). No CNAME: the domain belongs to the site above this folder.

  index.html                  redirect to the emulator (GHOULBOX; the installer when there is none)
  favicon.ico                 ESTAMPIE: the pages' icon (also in webapp/art/, with the home-screen icons)
  firmware/estampie-VER.fwsc   the package (+ LICENSE, LICENSING.md, LICENSES/: the package holds
                              JieLi SDK files under Apache-2.0, see LICENSING.md)
  firmware/stock/             ESTAMPIE: official V15 for "Return to official V15", M-VAVE's, unmodified (Apache-2.0,
                              with the licence and assets/m-vave/README.md)
  webapp/installer/index.html index_pkg.html, self-contained (fm1pkg.js, fm1ota.js, metadata inlined)
  webapp/editor/index.html    editor.html (+ fukiai.ttf, FUKIAI-LICENSE.txt)
  webapp/art/                 ESTAMPIE: the crest the installer and editor show, and the icons (tools/gen_emu_art.py)
  webapp/emu/                 GHOULBOX: the browser emulator, build/emu as web/emu/build.sh made it (when it is there)
  guide/                      ESTAMPIE: the user guide as a page (docs/guide/make_guide.py html; needs build/host/desc.json)
  src/                        not touched

  web/make_site.py build/estampie-X.Y.fwsc X.Y OUT_DIR

The package must be one made by tools/fm1pkg_make.py (Felucca's own loader, no vendor files).
Its identity (FM-1_378NNN) is read from the package; the device must report it after
the install.
"""
import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
BLOCKS, BLK, KEEP = 20, 0x30, 0x2F
STOCK_V15_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"   # (fm1pkg.js STOCK_V15_SHA256)


def strip_module(src):
    src = re.sub(r"^export\s+", "", src, flags=re.M)
    return re.sub(r"^import .*?;\n", "", src, flags=re.M)


def product_of(raw):
    """the package identity: one marker byte after each of the first 20 blocks (fm1pkg.js productOf)"""
    return "".join(chr((m - i - 1) & 0xFF) for i in range(BLOCKS) if (m := raw[i * BLK + KEEP]) != 0x7D)


def main(pkg, version, out):
    pkg, out = Path(pkg), Path(out)
    raw = pkg.read_bytes()
    product = product_of(raw)
    if not re.fullmatch(r"FM-1_378\d{3}", product):
        raise SystemExit(f"{pkg}: identity {product!r} is not an ESTAMPIE package (FM-1_378NNN)")
    if b"FELUCCA-LOADER-1" not in raw:              # Felucca loader marker: never publish a package with vendor files
        raise SystemExit(f"{pkg}: no Felucca loader in it; the site ships only fm1pkg_make.py packages "
                         "(a package patched from an official one carries vendor files)")
    html = (HERE / "index_pkg.html").read_text(encoding="utf-8")
    lib = strip_module((HERE / "fm1pkg.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1ota.js").read_text(encoding="utf-8")) + "\n" + \
        strip_module((HERE / "fm1backup.js").read_text(encoding="utf-8"))
    name = f"estampie-{re.sub(r'[^A-Za-z0-9.-]', '-', version)}.fwsc"
    stock = HERE.parent / "assets/m-vave/FM-1_015.fwsc"
    if hashlib.sha256(stock.read_bytes()).hexdigest() != STOCK_V15_SHA256:
        raise SystemExit(f"{stock}: not official V15 (assets/m-vave/README.md has its SHA-256)")
    meta = json.dumps({"version": version, "product": product, "pkg": "../../firmware/" + name,
                       "stock": "../../firmware/stock/FM-1_015.fwsc"})
    for mark in ("/*LIB*/", "/*META*/"):
        if html.count(mark) != 1:
            raise SystemExit(f"index_pkg.html must contain {mark} once; update make_site.py")
    html = html.replace("/*LIB*/", lib).replace("/*META*/", meta)
    inst, ed, fw = out / "webapp" / "installer", out / "webapp" / "editor", out / "firmware"
    for d in (inst, ed, fw):
        d.mkdir(parents=True, exist_ok=True)
    for old in fw.glob("estampie-*.fwsc"):          # one package: the current one
        old.unlink()
    (inst / "index.html").write_text(html, encoding="utf-8")
    shutil.copy(pkg, fw / name)
    lic = HERE.parent / "LICENSES"                  # the package holds JieLi SDK files (Apache-2.0): their
    (fw / "LICENSES").mkdir(exist_ok=True)          # licence travels next to it, with Felucca's own
    names = sorted(f.name for f in lic.glob("*.txt"))
    for n in names:
        shutil.copy(lic / n, fw / "LICENSES" / n)
    (fw / "LICENSES" / "index.html").write_text(    # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Felucca licences</title><h1>Licence texts</h1><ul>'
        + "".join(f'<li><a href="{n}">{n}</a></li>' for n in names)
        + '</ul><p><a href="../LICENSING.md">LICENSING.md</a> · <a href="../LICENSE">LICENSE (GPL-3.0)</a></p>\n',
        encoding="utf-8")
    for doc in ("LICENSE", "LICENSING.md"):
        shutil.copy(HERE.parent / doc, fw / doc)
    st = fw / "stock"                               # ESTAMPIE: official V15, as M-VAVE publish it (Apache-2.0)
    shutil.rmtree(st, ignore_errors=True)
    st.mkdir()
    shutil.copy(stock, st / stock.name)
    shutil.copy(lic / "Apache-2.0.txt", st / "LICENSE-Apache-2.0.txt")
    shutil.copy(stock.with_name("README.md"), st / "README.md")
    (st / "index.html").write_text(                 # the installer links this folder: Pages lists no folders
        '<!doctype html><meta charset="utf-8"><title>Official FM-1 V15</title><h1>Official FM-1 V15 (M-VAVE)</h1>'
        f'<ul><li><a href="{stock.name}">{stock.name}</a> (SHA-256 {STOCK_V15_SHA256})</li>'
        '<li><a href="README.md">README.md</a>: where it comes from, its terms</li>'
        '<li><a href="LICENSE-Apache-2.0.txt">Apache License, Version 2.0</a></li></ul>\n', encoding="utf-8")
    shutil.copy(HERE / "editor.html", ed / "index.html")
    root = HERE.parent                              # GHOULBOX: the art, and the emulator when it was built
    subprocess.run([sys.executable, str(root / "tools/gen_emu_art.py"), str(root / "build/gen"), str(out / "webapp/art")],
                   check=True)
    emu = root / "build/emu"
    if (emu / "felucca.wasm").exists():
        shutil.rmtree(out / "webapp/emu", ignore_errors=True)
        shutil.copytree(emu, out / "webapp/emu", ignore=shutil.ignore_patterns("native*"))
    else:
        print("site: no build/emu (web/emu/build.sh): webapp/emu left as it was")
    for f in ("fukiai.ttf", "FUKIAI-LICENSE.txt", "fm1backup.js"):
        if (HERE / f).exists():
            shutil.copy(HERE / f, ed / f)
    if (out / "CNAME").exists():
        (out / "CNAME").unlink()                    # (an older build's GHOULBOX domain: the folder's site owns its own)
    sys.path.insert(0, str(root / "docs/guide"))   # ESTAMPIE: the user guide
    import make_guide
    shutil.rmtree(out / "guide", ignore_errors=True)
    make_guide.build(out / "guide")
    home = "webapp/emu/" if (out / "webapp/emu/index.html").exists() else "webapp/installer/"   # GHOULBOX: the emulator
    (out / "index.html").write_text(                                                          # first, when it is there
        '<!doctype html><meta charset="utf-8"><title>ESTAMPIE</title>'
        '<link rel="icon" href="favicon.ico" sizes="16x16 32x32 48x48">'
        f'<meta http-equiv="refresh" content="0; url={home}">'
        f'<a href="{home}">ESTAMPIE</a>\n', encoding="utf-8")
    shutil.copy(out / "webapp/art/favicon.ico", out / "favicon.ico")   # ESTAMPIE: where a browser looks for it first
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
