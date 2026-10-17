#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the user guide's screenshots (docs/guide/{boot,home,presets,roll,menu,drone,ring}.png), drawn by the
# firmware's own UI code in VELLUM (tests/estampie_shots.c). Needs build/gen (./build.sh) and the venv (Pillow).
#   tools/guide_shots.sh
set -eu
cd "$(dirname "$0")/.."
OUT=build/guide_shots
mkdir -p build/host
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/host/estampie_shots tests/estampie_shots.c -lm
build/host/estampie_shots "$OUT"
python3 - "$OUT" docs/guide <<'PY'
import sys
from pathlib import Path
from PIL import Image
src, dst = Path(sys.argv[1]) / "ppm", Path(sys.argv[2])
for f in sorted(src.glob("VELLUM_*.ppm")):
    name = f.stem.split("_", 1)[1]
    Image.open(f).convert("RGB").save(dst / f"{name}.png", optimize=True)
    print(f"guide: {dst / (name + '.png')}")
PY
