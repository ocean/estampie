#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: fetch everything the build needs into the repo, so nothing is installed in $HOME.
#   .toolchain/jieli     JieLi's Linux toolchain (clang 4.0.1 for pi32v2), run in Docker on macOS
#   .toolchain/ac79-sdk  the JieLi AC79 SDK (three of its files go into every package)
#   .toolchain/emsdk     Emscripten, for the browser emulator (web/emu/)
#   .venv                Python 3.14 with the packages in uv.lock (uv sync)
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
# Python 3.14 and the packages in uv.lock (hash-pinned; pyproject.toml says why these versions). Always run: a
# no-op when .venv already matches the lock.
command -v uv >/dev/null 2>&1 || { echo "setup_toolchain: needs uv (https://docs.astral.sh/uv/)"; exit 1; }
uv sync --frozen
echo "setup_toolchain: done (./build.sh, then web/emu/build.sh)"
