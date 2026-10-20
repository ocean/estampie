#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: everything a release ships. GitHub builds the release itself: a pushed tag vX.Y.Z runs
# .github/workflows/release.yml, which builds with this script on GitHub's runner (tools/ci_release.sh) through the
# FM-1 Firmware Hub's shared workflow (zvenson/fm1-build), signs a build attestation for every file in build/dist/
# and releases them; the hub shows "Built from source". The build is reproducible: the same tag built here gives the
# same files, byte for byte (tools/verify_release.sh checks a release against this machine's build).
# fm-1.drewr.dev (github.com/ocean/fm1-site) publishes the site zip into /estampie/: its update workflow takes the
# newest release's estampie-site-X.Y.Z.zip, checks it against estampie-site-X.Y.Z.zip.sha256 (and the attestation),
# pins it and opens a pull request. Keep those two names.
#
#   tools/release.sh X.Y.Z            build: the firmware (--release), the emulator, the site, the site zip
#   tools/release.sh X.Y.Z --publish  build it here first (it must work), then tag vX.Y.Z and push the tag: GitHub
#                                     builds, signs and releases it, with docs/releases/X.Y.Z.md as its notes
#
# X.Y.Z must have its number in RELEASES (tools/build.py): build.py stops otherwise and names the next one.
# Out: build/estampie-X.Y.Z.fwsc, build/release-X.Y.Z/ (the package, the app, SHA256SUMS, licences),
#      build/site/ (web/make_site.py), build/estampie-site-X.Y.Z.zip (+ .sha256: "HASH  NAME"),
#      build/dist/: the files a release ships (the package, the app, the site zip and its .sha256).
set -e
cd "$(dirname "$0")/.."
VER="$1"
[ -n "$VER" ] || { echo "usage: tools/release.sh X.Y.Z [--publish]"; exit 2; }
PUBLISH=0
[ "$2" = "--publish" ] && PUBLISH=1
[ -x .venv/bin/python3 ] && PY="$PWD/.venv/bin/python3" || PY=python3

if [ "$PUBLISH" = 1 ]; then
    [ -z "$(git status --porcelain)" ] || { echo "release: the tree has changes: commit them first"; exit 1; }
    ! git rev-parse -q --verify "refs/tags/v$VER" >/dev/null || { echo "release: v$VER is already tagged"; exit 1; }
fi

./build.sh --release "$VER"
EMU_PREVIEW=0 web/emu/build.sh
# the firmware's parameter and engine tables (the guide's sound list: docs/guide/make_guide.py), as the tests make them
mkdir -p build/host
${CC:-cc} -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/descdump tests/descdump.c -lm   # (-O1 as the tests: unused HAL code goes)
build/host/descdump > build/host/desc.json
rm -rf build/site
"$PY" web/make_site.py "build/estampie-$VER.fwsc" "$VER" build/site

ZIP="estampie-site-$VER.zip"
rm -f "build/$ZIP" "build/$ZIP.sha256"
# reproducible: every file at the commit's time and 644 (a zip keeps both), the zip's times in UTC
EPOCH=$(git log -1 --format=%ct HEAD)
"$PY" -I -c 'import os, sys
t = int(sys.argv[2])
for d, _, fs in os.walk(sys.argv[1]):
    for f in fs:
        p = os.path.join(d, f)
        os.chmod(p, 0o644)
        os.utime(p, (t, t))' build/site "$EPOCH"
# index.html at the zip's root (no folder above it), files in a fixed order, no extra attributes (-X)
(cd build/site && find . -type f ! -name .DS_Store | LC_ALL=C sort | sed 's|^\./||' | TZ=UTC zip -q -X -@ "../$ZIP")
(cd build && shasum -a 256 "$ZIP" > "$ZIP.sha256")
unzip -l "build/$ZIP" | grep -q ' index.html$' || { echo "release: no index.html at the zip's root"; exit 1; }
echo "release: build/$ZIP ($(wc -c < "build/$ZIP" | tr -d ' ') B), sha256 $(cut -d' ' -f1 "build/$ZIP.sha256")"

rm -rf build/dist && mkdir -p build/dist           # what a release ships (fm1-build signs and releases build/dist/*)
cp "build/estampie-$VER.fwsc" "build/release-$VER/estampie-$VER-app.bin" "build/$ZIP" "build/$ZIP.sha256" build/dist/
(cd build/dist && shasum -a 256 * | sed 's/^/release: /')

if [ "$PUBLISH" = 1 ]; then
    [ -f "docs/releases/$VER.md" ] || echo "release: no docs/releases/$VER.md: the release keeps GitHub's own notes"
    git tag -a "v$VER" -m "ESTAMPIE $VER"
    git push origin "v$VER"
    echo "release: v$VER pushed. GitHub builds, signs and releases it (Actions > release); then"
    echo "         tools/verify_release.sh $VER checks it against this build. Next version: its number in RELEASES."
else
    echo "release: not published (tools/release.sh $VER --publish tags v$VER: GitHub builds, signs and releases it)"
fi
