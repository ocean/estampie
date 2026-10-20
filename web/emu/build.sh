#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Felucca in the browser: the firmware's sources as the host tests build them (web/emu/felucca_web.c), compiled
# to WebAssembly, plus the page. After X0X's web/emu/build.sh (charlesvestal/fm1-x0x, GPL-3.0).
# ESTAMPIE: compiled with zig cc (clang with a bundled wasi-libc, one signed download; tools/setup_toolchain.sh)
# rather than Emscripten: no Node, Python or npm in the build. A WASI reactor with no imports: the worklet calls
# _initialize, then the exports. ZIG=path uses another zig; its caches stay in the repo.
#   web/emu/build.sh   ->  build/emu/{index.html, tools.html, lab.html, worklet.js, felucca.wasm, fonts/}
# Run after one ./build.sh (build/gen: the tables, the fonts, the CC0 samples).
# -ffp-contract=off: no fused multiply-adds, as the host build. EMU_OPT replaces -O2, EMU_CFLAGS adds flags
# (EMU_CFLAGS=-msimd128: let the compiler vectorise; every browser with AudioWorklet has wasm SIMD).
# The page shows the version (FELUCCA_VERSION) marked "(preview)" before its release; EMU_PREVIEW=0 drops the mark.
set -e
cd "$(dirname "$0")/../.."
[ -f build/gen/felucca_tables.h ] || { echo "emu: run ./build.sh once first (build/gen)"; exit 1; }
ZIG="${ZIG:-.toolchain/zig/zig}"
[ -x "$ZIG" ] || { echo "emu: no zig in $ZIG (run tools/setup_toolchain.sh)"; exit 1; }
export ZIG_GLOBAL_CACHE_DIR="$PWD/.toolchain/zig-cache" ZIG_LOCAL_CACHE_DIR="$PWD/build/zig-cache"
VERSION=$(sed -n 's/^#define FELUCCA_VERSION "\([^"]*\)".*/\1/p' firmware/src/felucca.c)
GBVERSION=$(sed -n 's/^#define GHOULBOX_VERSION "\([^"]*\)".*/\1/p' firmware/src/core.h)   # GHOULBOX: the page shows both
ESVERSION=$(sed -n 's/^#define ESTAMPIE_VERSION "\([^"]*\)".*/\1/p' firmware/src/core.h)   # ESTAMPIE: and its own
mkdir -p build/emu/fonts build/emu/vendor/libflac build/emu/vendor/mp3 build/emu/licenses
# shellcheck disable=SC2086
"$ZIG" cc -target wasm32-wasi -mexec-model=reactor ${EMU_OPT:--O2} -g0 -s -ffp-contract=off -std=gnu99 -w ${EMU_CFLAGS:-} \
    "-DFELUCCA_VERSION=\"$VERSION\"" -Iweb/emu/wasi -Ibuild/gen -Ifirmware/src \
    -Wl,--export-dynamic -Wl,-z,stack-size=1048576 -Wl,--initial-memory=33554432 \
    -o build/emu/felucca.wasm web/emu/felucca_web.c -lm
[ "${EMU_PREVIEW:-1}" = 0 ] && PREVIEW='' || PREVIEW=' (preview)'
sed -e "s/@ESVERSION@/$ESVERSION/g" -e "s/@GBVERSION@/$GBVERSION/g" -e "s/@VERSION@/$VERSION/g" -e "s/@PREVIEW@/$PREVIEW/g" web/emu/index.html > build/emu/index.html
cp web/emu/worklet.js build/emu/
cp web/emu/emu_io.js build/emu/
cp web/emu/audio_export.js build/emu/
cp web/emu/audio_encode.js web/emu/audio_encode_worker.js build/emu/
cp web/emu/audio_session.js build/emu/
cp web/emu/tools.html build/emu/
cp web/emu/lab.html web/emu/lab_worklet.js web/emu/lab_sounds.js build/emu/   # ESTAMPIE: the sound lab
# ESTAMPIE: the screen reader's words and voice (fm1-reader, pinned in tools/toolchain.lock; FM1_READER: its checkout)
# in reader/, the words files in words/ (the site has them at its root: make_site.py)
R="${FM1_READER:-../fm1-reader}"
rm -rf build/emu/reader build/emu/words
if [ -f "$R/web/words.js" ]; then
    PIN=$(awk '$1 == "fm1-reader" { print $4 }' tools/toolchain.lock)
    HEAD=$(git -C "$R" rev-parse HEAD 2>/dev/null || echo none)
    [ "$HEAD" = "$PIN" ] || { echo "emu: $R is at $HEAD, tools/toolchain.lock pins fm1-reader $PIN"; exit 1; }
    mkdir -p build/emu/reader/espeak
    cp "$R/web/words.js" "$R/web/tts.js" "$R/web/tts_worker.js" build/emu/reader/
    cp "$R/web/espeak/espeak.js" "$R/web/espeak/espeak.wasm" "$R/web/espeak/LICENSE" "$R/web/espeak/README.md" \
        "$R/web/espeak/tts_wrap.c" build/emu/reader/espeak/
    cp "$R/LICENSE" build/emu/reader/LICENSE-MIT
    cp -R web/words build/emu/words
else
    echo "emu: no fm1-reader at $R: the page's screen reader has no words or voice (the site refuses this build)"
fi
cp web/emu/vendor/libflac/libflac.min.wasm.js web/emu/vendor/libflac/libflac.min.wasm.wasm build/emu/vendor/libflac/
cp web/emu/vendor/mp3/WasmMediaEncoder.min.js web/emu/vendor/mp3/mp3.wasm build/emu/vendor/mp3/
cp web/emu/vendor/README.md build/emu/vendor/
cp LICENSES/MIT-libflac.js.txt LICENSES/BSD-3-Clause-libFLAC.txt LICENSES/MIT-wasm-media-encoders.txt LICENSES/LGPL-2.0-LAME.txt LICENSES/LAME-NOTICE.txt build/emu/licenses/
[ -x .venv/bin/python3 ] && PY=.venv/bin/python3 || PY=python3   # ESTAMPIE: the locked Python (uv sync)
rm -rf build/emu/art                                # (no art an older build left behind is published)
"$PY" tools/gen_emu_art.py build/gen build/emu/art   # ESTAMPIE: the crest and the icons
cp web/emu/fonts/DotGothic16-subset.woff web/emu/fonts/OFL.txt build/emu/fonts/
echo "emu: build/emu ($(wc -c < build/emu/felucca.wasm | tr -d ' ') B wasm, ESTAMPIE $ESVERSION on GHOULBOX $GBVERSION on Felucca $VERSION)"
