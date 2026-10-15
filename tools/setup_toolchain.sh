#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: fetch everything the build needs into the repo, so nothing is installed in $HOME, and nothing runs that
# isn't pinned. Every download is checked against tools/toolchain.lock (SHA-256 for files, the commit for git) before
# it is unpacked; a mismatch stops the script. docs/SECURITY.md has the reasoning.
#   .toolchain/jieli       JieLi's Linux toolchain (clang 4.0.1 for pi32v2), run in Docker on macOS (tools/build.py)
#   .toolchain/ac79-sdk    the JieLi AC79 SDK (three of its files go into every package)
#   .toolchain/node        Node, the binary only (no npm, npx or corepack), for Emscripten
#   .toolchain/emscripten  Emscripten without emsdk, its development-only npm packages removed (web/emu/)
#   .toolchain/bin         osv-scanner (tools/osv_scan.sh)
#   .venv                  Python 3.14 with the packages in uv.lock (uv sync --frozen)
#   .toolchain/emulators   with --hw: the pi32v2 hardware emulator for tools/hw_check.sh (needs cargo)
# A step is skipped when its folder already holds the pinned version; change a pin and run it again to update.
#   tools/setup_toolchain.sh [--hw]
set -eu
WANT_HW=0
for arg in "$@"; do
    case "$arg" in
        --hw) WANT_HW=1 ;;
        *) echo "usage: tools/setup_toolchain.sh [--hw]"; exit 2 ;;
    esac
done
cd "$(dirname "$0")/.."
ROOT=$PWD
TC=.toolchain
DL=$TC/downloads
mkdir -p "$DL"

case "$(uname -s)-$(uname -m)" in
    Darwin-arm64) PLATFORM=darwin-arm64 ;;
    Linux-x86_64) PLATFORM=linux-x64 ;;
    *) echo "setup_toolchain: no pins for $(uname -s) $(uname -m) (tools/toolchain.lock)"; exit 1 ;;
esac

die() { echo "setup_toolchain: $*" >&2; exit 1; }

# The lock line for NAME on this platform ("any" and "git" lines match every platform): version, hash, url.
pin() {
    awk -v n="$1" -v p="$PLATFORM" '$1 == n && ($2 == p || $2 == "any" || $2 == "git") \
        { print $3, $4, $5; found = 1; exit } END { if (!found) exit 1 }' tools/toolchain.lock \
        || die "no $1 for $PLATFORM in tools/toolchain.lock"
}

sha256() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }

# Download URL to $DL once, over HTTPS only, and check it against the pinned hash (also when cached).
fetch() {
    url=$1 want=$2 file=$DL/$(basename "$1")
    if [ ! -f "$file" ]; then
        curl --proto '=https' --tlsv1.2 -fL --retry 3 -o "$file.part" "$url"
        mv "$file.part" "$file"
    fi
    got=$(sha256 "$file")
    [ "$got" = "$want" ] || { rm -f "$file"; die "$(basename "$file"): SHA-256 $got, pinned $want (removed; not unpacked)"; }
    echo "$file"
}

# A folder is current when its .pin holds the lock's hash.
current() { [ -f "$1/.pin" ] && [ "$(cat "$1/.pin")" = "$2" ]; }

# Unpack a verified archive into a fresh folder, then swap it in for DEST.
unpack() {
    archive=$1 dest=$2 hash=$3
    rm -rf "$dest.new" && mkdir -p "$dest.new"
    tar -xf "$archive" -C "$dest.new"
    echo "$hash" > "$dest.new/.pin"
    rm -rf "$dest" && mv "$dest.new" "$dest"
}

# A git checkout at exactly the pinned commit (fetched by REF, a tag or branch, then checked).
checkout() {
    dest=$1 ref=$2 commit=$3 url=$4
    if [ ! -d "$dest/.git" ] || [ "$(git -C "$dest" rev-parse HEAD)" != "$commit" ]; then
        rm -rf "$dest"
        git -c advice.detachedHead=false clone --quiet --no-recurse-submodules --depth 50 --branch "$ref" "$url" "$dest"
        git -C "$dest" -c advice.detachedHead=false checkout --quiet "$commit" 2>/dev/null \
            || { rm -rf "$dest"; die "$url: $ref doesn't reach the pinned commit $commit"; }
    fi
    [ "$(git -C "$dest" rev-parse HEAD)" = "$commit" ] || die "$dest: not at $commit"
}

# JieLi toolchain.
set -- $(pin jieli)
if ! current $TC/jieli "$2"; then
    unpack "$(fetch "$3" "$2")" $TC/jieli "$2"
    ln -sfn "jieli-linux-toolchains-$1" $TC/jieli/toolchain
fi
[ -x $TC/jieli/toolchain/pi32v2/bin/clang ] || die "$TC/jieli/toolchain: no pi32v2/bin/clang"

# AC79 SDK.
set -- $(pin ac79-sdk)
checkout $TC/ac79-sdk "$1" "$2" "$3"

# Node: emcc runs its JavaScript passes on it. npm, npx and corepack are deleted: nothing here installs packages
# with them, and npm's own bundled dependencies are where most of a Node release's known vulnerabilities are.
set -- $(pin node)
if ! current $TC/node "$2"; then
    unpack "$(fetch "$3" "$2")" $TC/node "$2"
    inner=$(ls -d $TC/node/node-v*)
    for d in "$inner"/*; do mv "$d" $TC/node/; done
    rmdir "$inner"
    rm -rf $TC/node/lib/node_modules $TC/node/bin/npm $TC/node/bin/npx $TC/node/bin/corepack $TC/node/include
fi
$TC/node/bin/node --version >/dev/null || die "$TC/node: node doesn't run"

# Python 3.14 and the packages in uv.lock (hash-pinned; pyproject.toml says why these versions). Before Emscripten:
# it runs on this Python.
command -v uv >/dev/null 2>&1 || die "needs uv (https://docs.astral.sh/uv/)"
uv sync --frozen

# Emscripten: the release archive emsdk would fetch (emsdk checks no hashes), configured for the Node above and the
# .venv Python, without the npm packages only Emscripten's own development uses (tools/prune_npm_dev.py).
set -- $(pin emscripten)
if ! current $TC/emscripten "$2"; then
    unpack "$(fetch "$3" "$2")" $TC/emscripten "$2"
    for d in $TC/emscripten/install/*; do mv "$d" $TC/emscripten/; done
    rmdir $TC/emscripten/install
    .venv/bin/python3 -I tools/prune_npm_dev.py $TC/emscripten/emscripten
    rm -f $TC/emscripten/emscripten/requirements-dev.txt   # (a list of Emscripten's test tools; none installed)
    cat > $TC/emscripten/.emscripten <<EOF
# ESTAMPIE: written by tools/setup_toolchain.sh; \$CFGDIR is this folder.
NODE_JS = '\$CFGDIR/../node/bin/node'
LLVM_ROOT = '\$CFGDIR/bin'
BINARYEN_ROOT = '\$CFGDIR'
EMSCRIPTEN_ROOT = '\$CFGDIR/emscripten'
EOF
fi
(. tools/emscripten_env.sh && emcc --version >/dev/null) || die "$TC/emscripten: emcc doesn't run"

# osv-scanner.
set -- $(pin osv-scanner)
if [ "$(cat $TC/bin/osv-scanner.pin 2>/dev/null)" != "$2" ]; then
    mkdir -p $TC/bin
    cp "$(fetch "$3" "$2")" $TC/bin/osv-scanner
    chmod 755 $TC/bin/osv-scanner
    echo "$2" > $TC/bin/osv-scanner.pin
fi

# The hardware emulator (optional: Rust; cargo builds only what its Cargo.lock pins).
if [ "$WANT_HW" = 1 ]; then
    set -- $(pin fm1-hw-emu)
    EMU=$TC/emulators/m-wave-fm1-browser-emulator
    checkout "$EMU" "$1" "$2" "$3"
    command -v cargo >/dev/null 2>&1 || die "--hw needs cargo (https://rustup.rs)"
    cargo build --locked --release --manifest-path "$EMU/emulator/Cargo.toml" --example diagnose
fi

echo "setup_toolchain: done in $ROOT (./build.sh, web/emu/build.sh, tools/osv_scan.sh)"
