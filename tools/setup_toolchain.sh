#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: fetch everything the build needs into the repo, so nothing is installed in $HOME.
#   .toolchain/jieli     JieLi's Linux toolchain (clang 4.0.1 for pi32v2), run in Docker on macOS
#   .toolchain/ac79-sdk  the JieLi AC79 SDK (three of its files go into every package)
#   .toolchain/emsdk     Emscripten, for the browser emulator (web/emu/)
#   .venv                Python with the build's pinned packages
# Each step is skipped when its folder is already there.
#   tools/setup_toolchain.sh
set -e
cd "$(dirname "$0")/.."
mkdir -p .toolchain
[ -d .toolchain/jieli/toolchain ] || sh tools/get_toolchain.sh "$PWD/.toolchain/jieli"
[ -d .toolchain/ac79-sdk ] || git clone --depth 1 --branch AC79NN_SDK_V1.2.1_2023-12-13 \
    https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK.git .toolchain/ac79-sdk
if [ ! -d .toolchain/emsdk ]; then
    git clone --depth 1 https://github.com/emscripten-core/emsdk.git .toolchain/emsdk
    (cd .toolchain/emsdk && ./emsdk install latest && ./emsdk activate latest)
fi
if [ ! -x .venv/bin/python3 ]; then
    # Python 3.14, as CI: tools/gen_samples.py (and so the golden renders) follows its float summation.
    "$(command -v python3.14 || command -v python3)" -m venv .venv
    .venv/bin/pip install pillow==12.3.0 fonttools==4.61.1 numpy==2.5.3 mido python-rtmidi
fi
echo "setup_toolchain: done (./build.sh, then web/emu/build.sh)"
