#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
"""Make the site: ESTAMPIE's folder of https://fm-1.drewr.dev/ (Drew's FM-1 projects, each in a folder of its own:
/estampie/ is this one). Every page's link is relative, so OUT_DIR works anywhere; ESTAMPIE_SITE changes the address
the guide's text shows (docs/guide/make_guide.py) and the base of llms.txt's links, which are absolute (llmstxt.org;
tools read the file with no base URL). No CNAME: the domain belongs to the site above this folder.

  index.html                  ESTAMPIE: the front page (web/home.html): what it is, the ways in, the thanks
  llms.txt, llms-full.txt     ESTAMPIE: the site for language models (llmstxt.org): the pages; the README and the guide
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
import os
import re
import shutil
import subprocess
import sys
from html import escape as html_escape
from html.parser import HTMLParser
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


README_LINKS = re.compile(r"\]\((?!https?:|#)([^)]+)\)")
SRC_URL = "https://github.com/ocean/estampie"


def guide_text(page):
    """the guide's <main> as plain text, its headings as Markdown ones (for llms-full.txt)"""
    class Text(HTMLParser):
        def __init__(self):
            super().__init__()
            self.out, self.on, self.skip, self.head = [], False, 0, False
        def handle_starttag(self, tag, attrs):
            if tag == "main":
                self.on = True
            elif tag in ("script", "style"):
                self.skip += 1
            elif self.on and tag in ("h1", "h2", "h3", "h4"):
                self.out.append("\n\n" + "#" * (int(tag[1]) + 2) + " ")    # (under "## User guide")
                self.head = True
            elif self.on and tag in ("td", "th"):
                self.out.append(" · ")
            elif self.on and tag in ("p", "li", "tr", "dt", "dd", "div", "br"):
                self.out.append("\n" + ("- " if tag == "li" else ""))
        def handle_endtag(self, tag):
            if tag == "main":
                self.on = False
            elif tag in ("script", "style"):
                self.skip -= 1
            elif tag in ("h1", "h2", "h3", "h4"):
                self.head = False
            elif self.head and tag == "span":
                self.out.append(" ")                                  # (a chapter's number, then its name)
        def handle_data(self, data):
            if self.on and not self.skip:
                self.out.append(re.sub(r"\s+", " ", data))
    t = Text()
    t.feed(page)
    lines = [ln.strip().removeprefix("· ") for ln in "".join(t.out).splitlines()]
    return re.sub(r"\n{3,}", "\n\n", "\n".join(lines)).strip()


def write_llms(out, version, product, name, sha):
    """llms.txt (llmstxt.org: what the site is and its pages) and llms-full.txt (that, the README and the guide's text)"""
    base = "https://" + os.environ.get("ESTAMPIE_SITE", "fm-1.drewr.dev/estampie").strip("/") + "/"
    head = (f"# ESTAMPIE\n\n> A mediæval groovebox: custom firmware for the M-VAVE FM-1 synthesiser. Version {version} "
            f"(firmware identity {product}). GPL-3.0-only; not affiliated with M-VAVE.\n\n"
            "Part of Drew Robinson's FM-1 projects: https://fm-1.drewr.dev/ . Source: " + SRC_URL + "\n\n"
            "## Pages\n\n"
            f"- [Front page]({base}index.html): what ESTAMPIE is, the ways in, credits\n"
            f"- [Play it in your browser]({base}webapp/emu/): the firmware compiled to WebAssembly, with its panel and sound\n"
            f"- [Install it on your FM-1]({base}webapp/installer/): the web installer (Web MIDI with SysEx; Chrome or Edge); "
            "also returns the FM-1 to official firmware V15\n"
            f"- [Editor]({base}webapp/editor/): edit sounds, patterns, songs and samples, and make backups, over USB MIDI\n"
            f"- [User guide]({base}guide/): from first notes to every page and setting\n"
            f"- [Firmware download]({base}firmware/{name}): the .fwsc package, SHA-256 {sha}\n"
            f"- [Licences]({base}firmware/LICENSING.md)\n\n")
    (out / "llms.txt").write_text(head + f"## Optional\n\n- [Full text]({base}llms-full.txt): the README and the user guide\n",
                                  encoding="utf-8")
    readme = (HERE.parent / "README.md").read_text(encoding="utf-8")
    readme = README_LINKS.sub(lambda m: f"]({SRC_URL}/blob/main/{m[1]})", readme)
    readme = re.sub(r"^#", "###", readme, flags=re.M)            # (under "## README")
    guide = guide_text((out / "guide" / "index.html").read_text(encoding="utf-8"))
    (out / "llms-full.txt").write_text(head + "## README\n\n" + readme.strip() + "\n\n## User guide\n\n" + guide + "\n",
                                       encoding="utf-8")
    for f in ("llms.txt", "llms-full.txt"):                   # (fm1-site's link check resolves these too)
        rel = [u for u in re.findall(r"\]\(([^)\s]+)\)", (out / f).read_text(encoding="utf-8")) if "://" not in u]
        if rel:
            raise SystemExit(f"{f}: relative links {rel[:3]}: every llms link must be absolute (from {base})")


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
    if not (out / "webapp/emu/index.html").exists():
        print("site: warning: no webapp/emu: the front page's 'Play it in your browser' leads nowhere")
    front = {"VERSION": html_escape(version), "PRODUCT": product, "PKG": "firmware/" + name, "PKG_NAME": name,
             "PKG_KB": str(round(len(raw) / 1024)), "PKG_SHA256": hashlib.sha256(raw).hexdigest()}
    page = (HERE / "home.html").read_text(encoding="utf-8")
    for k, v in front.items():
        page = page.replace("{{" + k + "}}", v)
    if re.search(r"\{\{[A-Z_0-9]+\}\}", page):
        raise SystemExit("home.html: a {{...}} make_site.py does not fill")
    (out / "index.html").write_text(page, encoding="utf-8")
    write_llms(out, version, product, name, front["PKG_SHA256"])
    shutil.copy(out / "webapp/art/favicon.ico", out / "favicon.ico")   # ESTAMPIE: where a browser looks for it first
    print(f"site: {out}: webapp/installer ({len(html)} B), webapp/editor, firmware/{name} ({len(raw)} B, {product})")


if __name__ == "__main__":
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    main(*sys.argv[1:4])
