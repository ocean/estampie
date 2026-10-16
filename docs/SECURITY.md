<!-- SPDX-License-Identifier: GPL-3.0-only -->

# Supply-chain security

Building FM-1 firmware means running a lot of other people's code on your own machine: a closed-source compiler from
the chip vendor, an SDK, a WebAssembly compiler, Python libraries, a container image and, in CI, GitHub Actions. Any of them could be tampered with on the way to you, ship a hijacked release, or carry a known
vulnerability. ESTAMPIE treats that as part of the build. The approach is meant to be copied: nothing in it is
specific to this firmware.

## Principles

1. **Pin by content, not by name.** A version number or a tag can be moved to point at different bytes; a SHA-256 or
   a git commit can't. Every download is checked against `tools/toolchain.lock` before it is unpacked, every Python
   package against the hashes in `uv.lock`, every GitHub Action by commit, the container by digest.
2. **Verify the publisher where it signs.** Zig's releases are minisign-signed (`tools/minisign_verify.py` checks
   them without installing minisign), Node's checksums are GPG-signed by its releasers, and osv-scanner publishes
   SHA-256 sums and SLSA provenance. Where a project signs nothing (JieLi), the pin records what was fetched and
   checked the first time (trust on first use), and any later change is refused.
3. **Wait before trusting something new.** Hijacked releases are usually found and pulled within days. uv won't lock a
   Python package less than 7 days old (`exclude-newer = "7 days"` in `pyproject.toml`); Dependabot waits 7 days too.
4. **Install less.** The less that's installed, the less can go wrong. The browser emulator was first built with
   Emscripten, through emsdk: that brought its own Python, Node, npm and 336 npm packages, with 72 known
   vulnerabilities between them, and emsdk checks no hashes. It is now built with `zig cc`: one signed archive with
   no package dependencies, which compiles the C library it needs from the source it ships. The output is tested
   bit for bit against the native build. Node remains only to run the web tests, without npm.
5. **Contain what you can't inspect.** JieLi's compiler is a closed-source Linux binary. On macOS it runs in a
   distroless container (no shell, no package manager), as you rather than root, with no network, no capabilities, no
   privilege escalation, a read-only root, a process limit and the sources mounted read-only. Only `build/` is
   writable.
6. **Scan continuously, the same way everywhere.** `tools/osv_scan.sh` checks everything against the OSV database.
   CI runs the same script on every push and weekly, so a vulnerability published after a dependency was locked still
   turns up.
7. **Exceptions expire.** An accepted finding needs a written reason and an `ignoreUntil` date. When the date passes,
   the scan fails again and the finding has to be reviewed.

## What runs, and how each part is checked

| Component | Pinned in | Verified by | Contained by | Scanned by |
| --- | --- | --- | --- | --- |
| JieLi toolchain (clang 4.0.1, pi32v2) | `tools/toolchain.lock` | SHA-256 (first use) | distroless container, offline, read-only | closed source: not scannable |
| AC79 SDK (3 files used) | `tools/toolchain.lock` | git commit | data only (not run) | – |
| Felucca-Engines (5 engines, MIT) | `firmware/engines/VENDORED.md` | git commit; copied into the repository (not a submodule), so every built line is reviewed in git | compiled into the firmware | source only: no package dependencies |
| Build container `cc-debian13:nonroot` | `tools/toolchain.lock` | image digest | see above | `osv_scan.sh build-image` |
| Python 3.14 and packages | `uv.lock` | SHA-256 per file (uv) | `.venv` in the repo | `osv_scan.sh python` |
| Node (binary only, for the tests) | `tools/toolchain.lock` | SHA-256 from GPG-signed `SHASUMS256.txt` | npm, npx and corepack deleted | `osv_scan.sh node` (Node security releases) |
| Zig 0.17.0 (`zig cc` to WebAssembly) | `tools/toolchain.lock` | SHA-256, taken after checking its minisign signature | – | no package dependencies |
| osv-scanner | `tools/toolchain.lock` | SHA-256 from the release's sums | – | – |
| Alagard font (Hewett Tsoi, from DaFont) | `tools/toolchain.lock` | SHA-256 of the zip (DaFont serves the same bytes each time); only `alagard.ttf` is taken from it | data only: rasterised by `tools/gen_aa_font.py` (Pillow) | not code: no OSV entries |
| pi32v2 hardware emulator (Rust) | `tools/toolchain.lock` | git commit; `cargo build --locked` | – | `osv_scan.sh hw-emu` |
| Web page codecs (libFLAC, LAME) | `web/emu/vendor/README.md` | SHA-256 per file | encode only, the app's own audio | no OSV entries for these versions |
| GitHub Actions | the workflows | commit SHA; zizmor checks the commits are real | `permissions: {}` / `contents: read`, no persisted credentials | zizmor, Dependabot |

Not covered: Docker Desktop, git, curl and uv themselves (keep them updated), and `cargo` from your own Rust install
for `--hw`. The FM-1 accepts any correctly framed update package: it doesn't check signatures, so only install packages
you built yourself or got from a source you trust. The M-VAVE updater or the USB rescue mode can always restore it.

## Day to day

```sh
tools/setup_toolchain.sh        # fetch or update to the pins; refuses any hash mismatch (--only STEP,... for some)
tools/osv_scan.sh               # every part; or name parts: python node hw-emu build-image
uv run zizmor .github           # after editing a workflow
```

Reports are written to `build/osv/`. The scan fails on any finding, and also when a part sees no packages at all, so
a broken scan can't pass silently.

## Updating a pin

Do it in a commit of its own, so the change is easy to review.

- **Python packages:** edit `pyproject.toml`, then `uv lock` (the cooldown applies), then `tools/osv_scan.sh python`.
  Dependabot opens these updates weekly too.
- **GitHub Actions:** Dependabot opens pull requests updating the commit and the version comment. Check the release
  notes, then run `uv run zizmor .github`.
- **Node:** take the newest release in the LTS line that is at least a week old. Verify its signature, then copy the
  two lines into `tools/toolchain.lock`:

  ```sh
  curl -fsSLO https://nodejs.org/dist/vX.Y.Z/SHASUMS256.txt.asc
  curl -fsSL https://github.com/nodejs/release-keys/raw/HEAD/gpg-only-active-keys/pubring.kbx -o node-keys.kbx
  gpg --no-default-keyring --keyring ./node-keys.kbx --verify SHASUMS256.txt.asc
  grep -E 'darwin-arm64.tar.xz|linux-x64.tar.xz' SHASUMS256.txt.asc
  ```

  `tools/osv_scan.sh node` reports when a newer release in the line is a security release.
- **Zig:** take a release at least a week old from `https://ziglang.org/download/index.json`. Download both archives
  with their `.minisig` files and check them with
  `tools/minisign_verify.py RWSGOq2NVecA2UPNdBUZykf1CCb147pkmdtYxgb3Ti+JO/wCYvhbAb/U FILE`, comparing that key with
  the one on ziglang.org/download. Then record the hashes, run `tools/setup_toolchain.sh` and `tests/run_tests.sh`. The
  browser emulator test must still match the native build bit for bit.
- **Build container:** `docker buildx imagetools inspect gcr.io/distroless/cc-debian13:nonroot`, then copy the digest
  into `tools/toolchain.lock`. `tools/osv_scan.sh build-image` reports when the tag has moved. Rebuild and compare the
  hashes of `build/felucca.bin` and `build/loader/ota.bin`: a new base image should not change the firmware. Then look
  through `tools/osv-build-image.toml` and delete entries whose fix is now in the image.
- **JieLi toolchain or SDK:** download, record the hash or commit, rebuild, and run the host tests and
  `tools/hw_check.sh` before trying the result on hardware.

## Accepted findings

`tools/osv-build-image.toml` lists the known vulnerabilities accepted in the build container, all due for review on
15 January 2027. They are in glibc, the gcc-14 runtime libraries and zlib, and are fixed only in Debian unstable so
far. The container can't be reached from outside: it has no network, runs one compiler on this repository's sources
and is thrown away after each file. Distroless rebuilds the image when Debian ships the fixes.

An alternative with a faster patch cadence is Docker Hardened Images (`dhi.io`, free with a Docker account). They are
also minimal, signed, and come with an SBOM and VEX statements. Switching only means changing the `build-image` line;
`JIELI_DOCKER_IMAGE` lets you try one first.

## On GitHub

Recommended settings for the repository and for forks: secret scanning with push protection, Dependabot alerts,
branch protection on `main` with the `host tests` and `supply chain` checks required, and Actions limited to
selected, pinned actions.

## Reporting a vulnerability

Please use GitHub's private vulnerability reporting on this repository rather than a public issue.
