#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# ESTAMPIE: check everything the build runs against the OSV vulnerability database (osv.dev), with osv-scanner.
#   python       uv.lock (the generators, the installer tools)
#   node         whether a newer release of the pinned Node line is a security release (OSV doesn't cover Node)
#   hw-emu       the Rust hardware emulator's Cargo.lock (when tools/setup_toolchain.sh --hw fetched it)
#   build-image  the pinned container the JieLi tools run in (needs Docker); accepted findings, each with a reason and
#                an expiry, are in tools/osv-build-image.toml
# Zig has no package dependencies to scan (its wasi-libc is built from the source it ships); its pin is a signed
# release. Parts whose tools aren't fetched are skipped (CI has only uv.lock). Reports (JSON) in build/osv/. Exits 1 on any finding.
#   tools/osv_scan.sh [part ...]       (default: every part)
set -u
cd "$(dirname "$0")/.."
OSV=.toolchain/bin/osv-scanner
[ -x "$OSV" ] || OSV=$(command -v osv-scanner) || { echo "osv_scan: no osv-scanner (tools/setup_toolchain.sh)"; exit 1; }
mkdir -p build/osv
PARTS=${*:-python node hw-emu build-image}
fail=0

# Scan with the given arguments; the report goes to build/osv/NAME.json, a summary to the terminal. A scan that sees
# no packages at all fails too: it would otherwise pass without checking anything.
scan() {
    name=$1; shift
    "$OSV" scan "$@" --format json --all-packages > "build/osv/$name.json" 2> "build/osv/$name.log"
    python3 -I - "build/osv/$name.json" "$name" <<'PY' || fail=1
import json, sys
path, name = sys.argv[1], sys.argv[2]
try:
    results = json.load(open(path)).get("results") or []
except ValueError:
    sys.exit(f"osv_scan: {name}: no report (build/osv/{name}.log)")
packages = [p for r in results for p in r["packages"]]
found = [(v["id"], p["package"]["name"], p["package"].get("version", "?")) for p in packages for v in p.get("vulnerabilities", [])]
if not packages:
    sys.exit(f"osv_scan: {name}: FAILED, no packages seen (build/osv/{name}.log)")
if found:
    print(f"osv_scan: {name}: {len(found)} FINDINGS in {len(packages)} packages (build/osv/{name}.json):")
    for f in sorted(set(found)):
        print("    %s  %s %s" % f)
    sys.exit(1)
print(f"osv_scan: {name}: {len(packages)} packages, no known vulnerabilities")
PY
}
skip() { echo "osv_scan: $1: skipped ($2)"; }

for part in $PARTS; do
    case $part in
    python)
        scan python source --lockfile uv.lock ;;
    node)
        NODE=.toolchain/node/bin/node
        [ -x $NODE ] || NODE=$(command -v node) || NODE=
        if [ -n "$NODE" ]; then
            have=$($NODE --version)
            newer=$(curl --proto '=https' -fsSL https://nodejs.org/dist/index.json | python3 -I -c '
import json, sys
have = sys.argv[1].lstrip("v").split(".")
rel = [r for r in json.load(sys.stdin) if r["version"].lstrip("v").split(".")[0] == have[0]]
newer = [r for r in rel if [int(x) for x in r["version"].lstrip("v").split(".")] > [int(x) for x in have]]
print(" ".join(r["version"] for r in newer if r["security"]))' "$have") || newer="?"
            case $newer in
                "") echo "osv_scan: node: $have, no newer security release in its line" ;;
                "?") echo "osv_scan: node: couldn't read nodejs.org/dist/index.json"; fail=1 ;;
                *) echo "osv_scan: node: $have; security releases since: $newer (update tools/toolchain.lock)"; fail=1 ;;
            esac
        else skip node "no node"; fi ;;
    hw-emu)
        EMU=.toolchain/emulators/m-wave-fm1-browser-emulator
        if [ -d $EMU ]; then
            scan hw-emu source --no-ignore --recursive $EMU
        else skip hw-emu "not fetched (tools/setup_toolchain.sh --hw)"; fi ;;
    build-image)
        if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
            IMG=$(awk '$1 == "build-image" { print $5 "@sha256:" $4 }' tools/toolchain.lock)
            docker pull -q --platform linux/amd64 "$IMG" >/dev/null &&
                docker save --platform linux/amd64 -o build/osv/build-image.tar "$IMG" &&
                scan build-image image --config tools/osv-build-image.toml --archive build/osv/build-image.tar
            rm -f build/osv/build-image.tar
            latest=$(docker buildx imagetools inspect "${IMG%@*}" 2>/dev/null | awk '/^Digest:/ { print $2; exit }')
            [ -z "$latest" ] || [ "${IMG#*@}" = "$latest" ] ||
                echo "osv_scan: build-image: ${IMG%@*} has a newer digest, $latest (update tools/toolchain.lock)"
        else skip build-image "no Docker"; fi ;;
    *) echo "osv_scan: no part $part"; exit 2 ;;
    esac
done
[ $fail = 0 ] && echo "osv_scan: clean" || echo "osv_scan: FAILED"
exit $fail
