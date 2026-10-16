#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: fetch everything the build needs into the repo, so nothing is installed in $HOME, and nothing runs that
# isn't pinned. Every download is checked against tools/toolchain.lock (SHA-256 for files, the commit for git) before
# it is unpacked; a mismatch stops the script. docs/SECURITY.md has the reasoning.
#   .toolchain/jieli       JieLi's Linux toolchain (clang 4.0.1 for pi32v2), run in Docker on macOS (tools/build.py)
#   .toolchain/ac79-sdk    the JieLi AC79 SDK (three of its files go into every package)
#   .toolchain/node        Node, the binary only (no npm, npx or corepack), for the web tests
#   .toolchain/zig         Zig, whose zig cc builds the browser emulator (web/emu/); its cache in .toolchain/zig-cache
#   .toolchain/bin         osv-scanner (tools/osv_scan.sh)
#   .venv                  Python 3.14 with the packages in uv.lock (uv sync --frozen)
#   .toolchain/emulators   with --hw: the pi32v2 hardware emulator for tools/hw_check.sh (needs cargo)
# A step is skipped when its folder already holds the pinned version; change a pin and run it again to update.
# --only takes a comma-separated list of steps (jieli, ac79-sdk, node, python, zig, osv-scanner, hw): CI fetches
# just what a job needs through the same checks.
#   tools/setup_toolchain.sh [--hw] [--only STEP,...]
set -eu
STEPS=jieli,ac79-sdk,node,python,zig,osv-scanner
USAGE="usage: tools/setup_toolchain.sh [--hw] [--only STEP,...]"
while [ $# -gt 0 ]; do
    case "$1" in
        --hw) STEPS="$STEPS,hw" ;;
        --only) [ $# -ge 2 ] || { echo "$USAGE"; exit 2; }; STEPS=$2; shift ;;
        *) echo "$USAGE"; exit 2 ;;
    esac
    shift
done
want() { case ",$STEPS," in *",$1,"*) true ;; *) false ;; esac; }
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

# Download URL to $DL once, over HTTPS only, and check it against the pinned hash (also when cached). Prints the
# file's path; callers stop when it fails (file=$(fetch ...) || exit 1).
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
if want jieli; then
    set -- $(pin jieli)
    if ! current $TC/jieli "$2"; then
        file=$(fetch "$3" "$2") || exit 1
        unpack "$file" $TC/jieli "$2"
        ln -sfn "jieli-linux-toolchains-$1" $TC/jieli/toolchain
    fi
    [ -x $TC/jieli/toolchain/pi32v2/bin/clang ] || die "$TC/jieli/toolchain: no pi32v2/bin/clang"
fi

# AC79 SDK.
if want ac79-sdk; then
    set -- $(pin ac79-sdk)
    checkout $TC/ac79-sdk "$1" "$2" "$3"
fi

# Node: the web page and browser emulator tests run on it. npm, npx and corepack are deleted: nothing here installs
# packages with them, and npm's own bundled dependencies are where most of a Node release's known vulnerabilities are.
if want node; then
    set -- $(pin node)
    if ! current $TC/node "$2"; then
        file=$(fetch "$3" "$2") || exit 1
        unpack "$file" $TC/node "$2"
        inner=$(ls -d $TC/node/node-v*)
        for d in "$inner"/*; do mv "$d" $TC/node/; done
        rmdir "$inner"
        rm -rf $TC/node/lib/node_modules $TC/node/bin/npm $TC/node/bin/npx $TC/node/bin/corepack $TC/node/include
    fi
    $TC/node/bin/node --version >/dev/null || die "$TC/node: node doesn't run"
fi

# Python 3.14 and the packages in uv.lock (hash-pinned; pyproject.toml says why these versions).
if want python; then
    command -v uv >/dev/null 2>&1 || die "needs uv (https://docs.astral.sh/uv/)"
    uv sync --frozen
fi

# Zig: one archive, minisign-signed by its maintainers (the pin's hash was taken after checking that signature,
# tools/minisign_verify.py). zig cc builds the wasi-libc it needs from the source it ships, once, into
# .toolchain/zig-cache (keyed by Zig's own version and inputs, so it survives a Zig update safely).
if want zig; then
    set -- $(pin zig)
    if ! current $TC/zig "$2"; then
        file=$(fetch "$3" "$2") || exit 1
        unpack "$file" $TC/zig "$2"
        inner=$(ls -d $TC/zig/zig-*)
        for d in "$inner"/*; do mv "$d" $TC/zig/; done
        rmdir "$inner"
    fi
    [ "$($TC/zig/zig version)" = "$1" ] || die "$TC/zig: zig doesn't run, or isn't $1"
fi

# osv-scanner.
if want osv-scanner; then
    set -- $(pin osv-scanner)
    if [ "$(cat $TC/bin/osv-scanner.pin 2>/dev/null)" != "$2" ]; then
        mkdir -p $TC/bin
        file=$(fetch "$3" "$2") || exit 1
        cp "$file" $TC/bin/osv-scanner
        chmod 755 $TC/bin/osv-scanner
        echo "$2" > $TC/bin/osv-scanner.pin
    fi
fi

# The hardware emulator (optional: Rust; cargo builds only what its Cargo.lock pins).
if want hw; then
    set -- $(pin fm1-hw-emu)
    EMU=$TC/emulators/m-wave-fm1-browser-emulator
    checkout "$EMU" "$1" "$2" "$3"
    command -v cargo >/dev/null 2>&1 || die "--hw needs cargo (https://rustup.rs)"
    cargo build --locked --release --manifest-path "$EMU/emulator/Cargo.toml" --example diagnose
fi

echo "setup_toolchain: done in $ROOT (./build.sh, web/emu/build.sh, tools/osv_scan.sh)"
