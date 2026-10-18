#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: everything a release ships, built locally (the JieLi toolchain cannot run in CI), ready to attach to the
# GitHub release vX.Y. fm-1.drewr.dev (github.com/ocean/fm1-site) publishes the site zip into /estampie/: its update
# workflow takes the newest release's estampie-site-X.Y.zip, checks it against estampie-site-X.Y.zip.sha256, pins it
# and opens a pull request. Keep those two names.
#
#   tools/release.sh X.Y            build: the firmware (--release), the emulator, the site, the site zip
#   tools/release.sh X.Y --publish  and then tag vX.Y and create the GitHub release with them (gh)
#
# X.Y must have its number in RELEASES (tools/build.py): build.py stops otherwise and names the next one.
# Out: build/estampie-X.Y.fwsc, build/release-X.Y/ (the package, the app, SHA256SUMS, licences),
#      build/site/ (web/make_site.py), build/estampie-site-X.Y.zip (+ .sha256: "HASH  NAME").
set -e
cd "$(dirname "$0")/.."
VER="$1"
[ -n "$VER" ] || { echo "usage: tools/release.sh X.Y [--publish]"; exit 2; }
PUBLISH=0
[ "$2" = "--publish" ] && PUBLISH=1
[ -x .venv/bin/python3 ] && PY="$PWD/.venv/bin/python3" || PY=python3

if [ "$PUBLISH" = 1 ]; then
    [ -z "$(git status --porcelain)" ] || { echo "release: the tree has changes: commit them first"; exit 1; }
    ! git rev-parse -q --verify "refs/tags/v$VER" >/dev/null || { echo "release: v$VER is already tagged"; exit 1; }
fi

./build.sh --release "$VER"
EMU_PREVIEW=0 web/emu/build.sh
rm -rf build/site
"$PY" web/make_site.py "build/estampie-$VER.fwsc" "$VER" build/site

ZIP="estampie-site-$VER.zip"
rm -f "build/$ZIP" "build/$ZIP.sha256"
# index.html at the zip's root (no folder above it), files in a fixed order, no extra attributes (-X)
(cd build/site && find . -type f ! -name .DS_Store | LC_ALL=C sort | sed 's|^\./||' | zip -q -X -@ "../$ZIP")
(cd build && shasum -a 256 "$ZIP" > "$ZIP.sha256")
unzip -l "build/$ZIP" | grep -q ' index.html$' || { echo "release: no index.html at the zip's root"; exit 1; }
echo "release: build/$ZIP ($(wc -c < "build/$ZIP" | tr -d ' ') B), sha256 $(cut -d' ' -f1 "build/$ZIP.sha256")"

if [ "$PUBLISH" = 1 ]; then
    git tag -a "v$VER" -m "ESTAMPIE $VER"
    git push origin "v$VER"
    gh release create "v$VER" --title "ESTAMPIE $VER" --notes "ESTAMPIE $VER. Install it from https://fm-1.drewr.dev/estampie/webapp/installer/ (or the .fwsc with FM1Kit / FM-SWITCH).

Site zip SHA-256: $(cut -d' ' -f1 "build/$ZIP.sha256")" \
        "build/estampie-$VER.fwsc" "build/release-$VER/estampie-$VER-app.bin" "build/release-$VER/SHA256SUMS" \
        "build/$ZIP" "build/$ZIP.sha256"
    echo "release: v$VER published. Next version: add it to RELEASES in tools/build.py (its own number)."
else
    echo "release: not published (tools/release.sh $VER --publish tags v$VER and creates the GitHub release)"
fi
