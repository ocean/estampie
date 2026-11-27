#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: the image the JieLi tools run in (tools/build-image/apko.yaml), built with apko from the Wolfi packages
# tools/build-image/apko.lock.json pins. The same lock always makes the same image (apko dates it by its newest
# package), so anyone can rebuild the pinned image and check it against the registry's.
#   tools/build_image.sh                       build it into build/build-image.tar and check it is the pinned image
#   tools/build_image.sh --update              first re-resolve apko.lock.json to Wolfi's newest packages
#   tools/build_image.sh [--update] --publish  push it (docker login to the registry first) and pin its digest in
#                                              tools/toolchain.lock
# .github/workflows/build-image.yml runs --update --publish every five days, and pushes the new pins to a branch.
set -eu
cd "$(dirname "$0")/.."
UPDATE=0 PUBLISH=0
for a in "$@"; do
    case "$a" in
        --update) UPDATE=1 ;;
        --publish) PUBLISH=1 ;;
        *) echo "usage: tools/build_image.sh [--update] [--publish]"; exit 2 ;;
    esac
done
APKO=.toolchain/bin/apko
[ -x $APKO ] || { echo "build_image: no apko (tools/setup_toolchain.sh --only apko)"; exit 1; }
CFG=tools/build-image/apko.yaml LOCK=tools/build-image/apko.lock.json
# The lock's image line: its digest and its repository (the tag dropped).
DIGEST=$(awk '$1 == "build-image" { print $4 }' tools/toolchain.lock)
REPO=$(awk '$1 == "build-image" { sub(/:[^:\/]*$/, "", $5); print $5 }' tools/toolchain.lock)
TAG=$(awk '$1 == "build-image" { print $3 }' tools/toolchain.lock)
# (unset: a set SOURCE_DATE_EPOCH would date the image by it, not by its packages)
unset SOURCE_DATE_EPOCH

packages() { python3 -I -c 'import json, sys
for p in json.load(open(sys.argv[1]))["contents"]["packages"]: print("    " + p["name"], p["version"])' "$1"; }

mkdir -p build
if [ $UPDATE = 1 ]; then
    OLD=$(packages $LOCK)
    $APKO lock $CFG --output $LOCK 2>/dev/null || { echo "build_image: FAILED, apko lock"; exit 1; }
    NOW=$(packages $LOCK)
    if [ "$OLD" = "$NOW" ]; then
        echo "build_image: Wolfi has no newer packages"
    else
        echo "build_image: newer packages:"
        echo "$NOW" | grep -vxF "$OLD" || true
    fi
fi

$APKO build --lockfile $LOCK --sbom=false $CFG fm-1-firmware-build-image:local build/build-image.tar >/dev/null 2>&1 ||
    { echo "build_image: FAILED, apko build (run $APKO build $CFG by hand to see why)"; exit 1; }
# The image's manifest: what the registry's index points to for linux/amd64.
BUILT=$(tar -xOf build/build-image.tar index.json | python3 -I -c 'import json, sys
print(json.load(sys.stdin)["manifests"][0]["digest"])')

if [ $PUBLISH = 1 ]; then
    REF=$($APKO publish --lockfile $LOCK --sbom=false $CFG "$REPO:$TAG" 2>/dev/null | tail -1)
    NEW=${REF##*@sha256:}
    [ ${#NEW} = 64 ] || { echo "build_image: FAILED, apko publish printed no digest"; exit 1; }
    python3 -I - "$NEW" <<'PY'
import re, sys
from pathlib import Path
p = Path("tools/toolchain.lock")
s, n = re.subn(r"^(build-image +\S+ +\S+ +)[0-9a-f]{64}", lambda m: m.group(1) + sys.argv[1], p.read_text(), flags=re.M)
if n != 1:
    sys.exit("build_image: FAILED, no build-image line in tools/toolchain.lock")
p.write_text(s)
PY
    echo "build_image: published $REF, pinned in tools/toolchain.lock"
    DIGEST=$NEW
fi

# The pinned index's linux/amd64 manifest, from the registry.
PINNED=$(docker buildx imagetools inspect --raw "$REPO@sha256:$DIGEST" | python3 -I -c 'import json, sys
print(json.load(sys.stdin)["manifests"][0]["digest"])') || { echo "build_image: FAILED, couldn't read $REPO@sha256:$DIGEST"; exit 1; }
if [ "$BUILT" = "$PINNED" ]; then
    echo "build_image: build/build-image.tar is the pinned image ($REPO@sha256:$DIGEST)"
elif [ $UPDATE = 1 ]; then
    echo "build_image: the newer packages make a new image ($BUILT): --publish pins it"
else
    echo "build_image: FAILED, this build ($BUILT) isn't the pinned image's ($PINNED)"; exit 1
fi
