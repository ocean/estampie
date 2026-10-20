#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: check a published release: each file's build attestation (signed by GitHub for zvenson/fm1-build, from
# this repository, at the tag), and that each is byte for byte the file this machine builds from the same tag
# (tools/release.sh X.Y.Z first, at that tag, so build/dist/ holds this build).
#   tools/verify_release.sh X.Y.Z
set -eu
cd "$(dirname "$0")/.."
VER=${1:?usage: tools/verify_release.sh X.Y.Z}
REPO=ocean/estampie
D=build/verify-$VER
rm -rf "$D" && mkdir -p "$D"
gh release download "v$VER" -R "$REPO" -D "$D" -p 'estampie-*'
fail=0
for f in "$D"/*; do
    n=$(basename "$f")
    if gh attestation verify "$f" --repo "$REPO" --signer-workflow zvenson/fm1-build/.github/workflows/build.yml \
            --source-ref "refs/tags/v$VER" >/dev/null 2>&1; then
        sig=signed
    else
        sig="NOT SIGNED"; fail=1
    fi
    if [ -f "build/dist/$n" ] && cmp -s "$f" "build/dist/$n"; then
        same="the same as this build"
    else
        same="DIFFERS from this build (build/dist/$n)"; fail=1
    fi
    echo "verify: $n: $sig, $same"
done
[ "$fail" = 0 ] && echo "verify: v$VER: every file signed, and reproduced here" || { echo "verify: v$VER: FAILED"; exit 1; }
