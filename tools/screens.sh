#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE's screens for the README and the FM-1 Firmware Hub: docs/screens/<look>/NN-name.png (720 x 720, the
# 240 x 240 screen at 3x, no smoothing) and a contact sheet of each look. The simulator (tests/estampie_host.c)
# plays the TROTTO and takes each page (`colour`, `shot`); the boot screen and ABOUT come from the UI render test's
# build/ui_new (tests/run_tests.sh), which draws them with the same code.
set -euo pipefail
cd "$(dirname "$0")/.."
OUT=docs/screens
RAW=build/screens
[ -f build/ui_new/VELLUM/splash.png ] || { echo "screens: no build/ui_new; run tests/run_tests.sh first"; exit 1; }
mkdir -p build/host "$RAW"
cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/estampie_host tests/estampie_host.c -lm
for LOOK in VELLUM TAVERN; do
    look=$(echo "$LOOK" | tr A-Z a-z)
    # Each page with the TROTTO playing; track 4 (the DANCE kit) for the drum grid.
    cat > "$RAW/$look.txt" <<SCRIPT
colour $LOOK
wait 300
press PLAY
wait 3000
shot ${look}_02.ppm
press HOME
wait 2500
shot ${look}_03.ppm
press SEQ
wait 3000
shot ${look}_04.ppm
turn ALGO 3
wait 3000
shot ${look}_05.ppm
turn ALGO -3
wait 300
press FX
wait 3000
shot ${look}_06.ppm
press SCL
wait 3000
shot ${look}_07.ppm
press ENV
wait 1500
shot ${look}_08.ppm
press LFO
wait 1500
shot ${look}_09.ppm
hold HOME
wait 1500
release HOME
wait 500
shot ${look}_10.ppm
SCRIPT
    build/host/estampie_host "$RAW/$look.txt" "$RAW" >/dev/null
    cp "build/ui_new/$LOOK/splash.png" "$RAW/${look}_01.png"
    cp "build/ui_new/$LOOK/about.png" "$RAW/${look}_11.png"
done
uv run --no-sync python - "$RAW" "$OUT" <<'PY'
import sys
from pathlib import Path
from PIL import Image
raw, out = Path(sys.argv[1]), Path(sys.argv[2])
NAMES = ["boot", "home-trotto-playing", "mixer", "sequencer", "drum-grid", "fx", "scale", "envelope", "lfo", "menu",
         "about"]
for look in ("vellum", "tavern"):
    (out / look).mkdir(parents=True, exist_ok=True)
    ims = []
    for i, name in enumerate(NAMES, 1):
        src = raw / f"{look}_{i:02d}.ppm"
        im = Image.open(src if src.exists() else raw / f"{look}_{i:02d}.png").convert("RGB")
        assert im.size == (240, 240), (look, name, im.size)
        im.resize((720, 720), Image.NEAREST).save(out / look / f"{i:02d}-{name}.png", optimize=True)
        ims.append(im)
    # The sheet: four across at 2x, on the look's own background.
    bg, W, S, G = ims[1].getpixel((120, 4)), 4, 480, 24
    rows = (len(ims) + W - 1) // W
    sheet = Image.new("RGB", (W * S + (W + 1) * G, rows * S + (rows + 1) * G), bg)
    for i, im in enumerate(ims):
        sheet.paste(im.resize((S, S), Image.NEAREST), (G + i % W * (S + G), G + i // W * (S + G)))
    sheet.save(out / f"{look}.png", optimize=True)
PY
echo "screens: $OUT/vellum/, $OUT/tavern/ (11 each) and $OUT/vellum.png, $OUT/tavern.png"
