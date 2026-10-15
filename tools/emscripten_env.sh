# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: source this for the in-repo Emscripten (tools/setup_toolchain.sh): emcc on the PATH, its config, the
# pinned Node and the uv-locked Python. Use from the repository root:   . tools/emscripten_env.sh
EM_CONFIG="$PWD/.toolchain/emscripten/.emscripten"
EMSDK_PYTHON="$PWD/.venv/bin/python3"
PATH="$PWD/.toolchain/emscripten/emscripten:$PWD/.toolchain/node/bin:$PATH"
export EM_CONFIG EMSDK_PYTHON PATH
