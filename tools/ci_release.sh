#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the release build on GitHub's runner (.github/workflows/release.yml, through zvenson/fm1-build). The shared
# workflow installs a JieLi toolchain and Python of its own; this uses ESTAMPIE's, pinned and checked in
# tools/toolchain.lock (tools/setup_toolchain.sh), so the release is built exactly as it is here and anyone can
# rebuild it byte for byte. Then tools/release.sh, which leaves the files to sign and release in build/dist/.
#   tools/ci_release.sh REF     REF: the run's ref name (GITHUB_REF_NAME). A tag vX.Y.Z builds X.Y.Z, which must be
#                               the firmware's own version (core.h); a branch (a dry run) builds the version core.h
#                               has, signed but not released (fm1-build releases tags only)
set -eu
cd "$(dirname "$0")/.."
REF=${1:?usage: tools/ci_release.sh REF}
HAS=$(sed -n 's/^#define ESTAMPIE_VERSION "\([^"]*\)".*/\1/p' firmware/src/core.h)
case "$REF" in
    v[0-9]*) VER=${REF#v}
             [ "$VER" = "$HAS" ] || { echo "ci_release: tag $REF, but core.h says $HAS: tag the commit that sets it"; exit 1; } ;;
    *)       VER=$HAS; echo "ci_release: $REF is not a version tag: a dry run of $VER (signed, not released)" ;;
esac
tools/setup_toolchain.sh --only uv,jieli,ac79-sdk,python,zig,node,fm1-reader
export JIELI_TOOLCHAIN="$PWD/.toolchain/jieli/toolchain" AC79_SDK="$PWD/.toolchain/ac79-sdk"   # (not the workflow's)
export FM1_READER="${FM1_READER:-$PWD/.toolchain/fm1-reader}"
[ -f ../fm1-reader/web/words.js ] && FM1_READER=$(cd ../fm1-reader && pwd)
tools/release.sh "$VER"
