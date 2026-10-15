#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: boot a built package on emulated FM-1 hardware (two pi32v2 cores, the board's screen, keys, audio and
# flash: shcherbakov7/m-wave-fm1-browser-emulator, Rust, GPL-3.0) and save what the screen shows. This checks the
# real image (memory layout, start-up, the CPU's speed), which the browser build of the sources (web/emu/) can't.
#   tools/hw_check.sh [package.fwsc] [instructions]   (default: build/felucca.fwsc, 600000000)
# Writes build/hw/screen.png. Fails when the firmware stops or draws nothing.
set -e
cd "$(dirname "$0")/.."
PKG="${1:-build/felucca.fwsc}"
LIMIT="${2:-600000000}"
EMU=.toolchain/emulators/m-wave-fm1-browser-emulator
DIAG="$EMU/emulator/target/release/examples/diagnose"
# The emulator at its pinned commit, built --locked (tools/toolchain.lock).
[ -x "$DIAG" ] || sh tools/setup_toolchain.sh --only hw
mkdir -p build/hw
OUT=$(FM1_LCD_PPM=build/hw/screen.ppm DIAG_FAST=1 "$DIAG" "$PKG" "$LIMIT" 2>&1) || true
LCD=$(printf '%s\n' "$OUT" | sed -n 's/^LCD: \([0-9]*\) pixels.*/\1/p')
STOP=$(printf '%s\n' "$OUT" | sed -n 's/^diagnose: //p')
[ -x .venv/bin/python3 ] && PY=.venv/bin/python3 || PY=python3
[ -f build/hw/screen.ppm ] && "$PY" -c "from PIL import Image; Image.open('build/hw/screen.ppm').save('build/hw/screen.png')"
echo "hw_check: $PKG: $STOP; LCD ${LCD:-0} pixels written; screen in build/hw/screen.png"
case "$STOP" in
    "instruction limit"*) [ "${LCD:-0}" -gt 0 ] || { echo "hw_check: nothing drawn"; exit 1; } ;;
    *) printf '%s\n' "$OUT" | tail -30; exit 1 ;;
esac
