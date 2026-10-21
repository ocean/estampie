# AGENTS.md: working on ESTAMPIE

ESTAMPIE is custom firmware for the M-VAVE FM-1 synthesiser: a groovebox for mediæval dance music. It runs on the
FM-1's JieLi AC79 chip (the pi32v2 CPU), and comes with a browser emulator, a web installer, a web editor and a user
guide, all published at <https://fm-1.drewr.dev/estampie/>. This file is for anyone, person or coding agent, picking
up the work. [README.md](README.md) says what ESTAMPIE is; [BUILDING.md](BUILDING.md) how to build it;
[docs/SECURITY.md](docs/SECURITY.md) why the build is the way it is. This file is the map between them.

The maintainer is Drew Robinson (GitHub `ocean`). He is friendly and kind to people and to agents; be the same, in
commits, PRs, issues and messages to other sessions.

## Lineage

ESTAMPIE is a fork of [GHOULBOX](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth) (Jason Persinger),
which forks [Felucca](https://github.com/hugelton/Felucca) (Leo Kuroshita, Hügelton Instruments). Nearly everything
here is their work, and the whole history is kept. Credit them, and everyone in [CREDITS.md](CREDITS.md), whenever
something of theirs is touched. Comments marked `GHOULBOX:` or `ESTAMPIE:` say whose change a line is; unmarked code
is Felucca's. The remotes `ghoulbox` and `felucca` are fetch-only.

## House rules

- **Spelling.** Australian/British English everywhere: code, comments, docs, UI ("colour", "initialise",
  "favourite"). Always **mediæval**, with the æ (MEDIÆVAL on screen: the device font has it; `\xC6` / `\xE6` in C
  strings). Never "medieval" or "mediaeval".
- **Comments.** Match the surrounding code: dense, plain English, why rather than what. A one-line comment ends with a
  full stop. Blank lines carry no indentation.
- **Git.** `git --no-pager ...` (a pager rewrites output). Commits are authored as drew.robinson@gmail.com (set in the
  repo's config), never a work address. Commit messages explain the change in prose, like the existing history.
- **Scratch work** goes in `.artifacts/` (git-ignored): `.artifacts/tmp/`, `.artifacts/scripts/`,
  `.artifacts/screenshots/`. Never the repo root or the system temp folder.
- **Tests before every commit:** `tests/run_tests.sh`, all of it. See "Known test failure" below for the one that
  fails on Apple Silicon.
- **Python** through uv only (`uv sync --frozen`, `pyproject.toml` + hash-pinned `uv.lock`), never pip or a bare venv.
- **Anything new that runs** (a package, a tool, a Docker image, a GitHub Action) is pinned by content and scanned:
  see "Supply chain".
- **Ask before anything outward-facing**: publishing a release, deploying the site, opening a PR on someone else's
  repository, flashing a device. Committing and pushing to `main` is routine.
- **PRs to other people's repositories** are warm and thankful, offer each change as a suggestion they're free to
  change or drop, and sign off as Drew. Don't put a Claude session link in a PR description.

## The repository

| Where | What |
|---|---|
| `firmware/src/` | The firmware, one unity build: `felucca.c` `#include`s everything in order. `core.h` has the data model and `ESTAMPIE_VERSION`. |
| `firmware/src/ui*.c` | The UI: `ui.c` (state), `ui_draw.c` (the screen; every knob card goes through `draw_column`), `ui_input.c`, `ui_menu.c` + `menu_items.c` (MENU), `ui_layer.c` (the held-button layers), `ui_help.c`, `ui_name.c` (NAME entry), `ui_speak.c` (the screen reader). |
| `firmware/src/eng_*.c`, `firmware/engines/` | The sound engines. `firmware/engines/` is Felucca-Engines (MIT), vendored: keep it as published but for the marked hooks (`VENDORED.md`). New presets go in `engine_presets.h`. |
| `firmware/src/key_touch.c` | KEY TOUCH: velocity and aftertouch measured from the keys (after Charles Vestal's Velo / MONK-1). |
| `firmware/src/announce.c`, `firmware/words.tsv` | The screen reader: fm1-reader's protocol (a copy: don't edit it, see below) and ESTAMPIE's words for it. |
| `firmware/hal/` | Hardware access (`fm1_*.h`). Only `hal/` touches registers: the build checks it. |
| `firmware/loader/` | The update loader, its own small image. |
| `tools/` | The build (`build.py`, via `./build.sh`), generators (`gen_*.py`: fonts, keycaps, icons, art, tables, samples, the words file), release and supply-chain scripts. |
| `tests/` | Host tests (`run_tests.sh` runs them all), `hostsim.c` (the firmware's sound and UI on the host), `estampie_host.c` (the scripted simulator), `scenarios/` (its scripts). |
| `web/` | The site: `make_site.py`, `home.html`, `index_pkg.html` (the installer), `editor.html`, `fm1*.js`. `web/emu/`: the browser emulator (`felucca_web.c` compiled to WebAssembly with `zig cc`), the Sound Lab (`lab.html`). `web/words/`: the published words files. |
| `docs/guide/` | The user guide (`guide.html`, built by `make_guide.py`). `docs/releases/X.Y.Z.md`: each release's notes. `docs/readme/`: the README pictures and the page that composes them (`hero.html`). |
| `assets/` | Fonts (OFL), CC0 samples, M-VAVE's official V15 firmware (Apache-2.0, for the installer's way back). |
| `tools/toolchain.lock` | Every external download, pinned. |

## Build and test

```sh
tools/setup_toolchain.sh          # once: everything into .toolchain/ and .venv/, each checked against tools/toolchain.lock
./build.sh                        # the firmware: build/felucca.fwsc (dev identity FM-1_378000)
web/emu/build.sh                  # the browser emulator: build/emu/ (serve it: python3 -m http.server -d build/emu)
tests/run_tests.sh                # every host test (four lanes in parallel), about 7 minutes
tools/release.sh X.Y.Z            # a whole release, here: build/dist/ (see Releasing)
```

- **Everything lives in the repo.** The JieLi toolchain is a closed Linux x86-64 binary. On macOS it runs in a
  pinned, distroless Docker container with no network, so Docker must be running. On Linux x86-64 it runs natively.
- **Budgets** (each build prints them, and fails past them):
  - **Flash:** the app slot is 581,564 B, and a build that leaves less than 16 KiB spare fails. At 0.3.1 the image is 547,568 B, about 34 KB spare. Pixel fonts cost about half what anti-aliased ones do.
  - **RAM:** `.data` + `.bss` 61,332 of 73,728 B. The `.pool` section, 334,488 of 368,640 B, is where large, zero-initialised, main-loop-only state goes.
  - **ISR cost:** `tests/target_budget.py` holds the interrupt handlers to their cycle budgets (e.g. the scan tick, `fm1_timer5_irq`). Keep work out of interrupts: no loops, no 64-bit maths, no text.
- **Known test failure:** `REVERB TYPE` (the HALL reverb's cost check) fails on Apple Silicon only. It passes in CI
  (Linux). Every other test must pass.
- **The simulator.** `tests/estampie_host.c` runs the emulator's firmware natively from a script (`press`, `turn`,
  `key`, `wait`, `listen`, `said`, `speakdump`, …): the way to test UI behaviour without a device.
- **Hardware** is tested by Drew on his FM-1. Say plainly what has only been tested in the simulator or the emulator.

## The screen reader

ESTAMPIE supports [fm1-reader](https://github.com/charlesvestal/fm1-reader) (Charles Vestal): a page on a computer
reads the FM-1's screen aloud, or hands it to the computer's own screen reader.

- **The protocol:** `firmware/src/announce.c` and `tools/gen_words.py` are fm1-reader's, copied unchanged. Don't edit them. Change them upstream, then re-sync: `../fm1-reader/tools/sync_announce.sh .`, or `--check` to compare. The checkout sits next to this repo; otherwise `tools/setup_toolchain.sh` fetches the pinned commit into `.toolchain/fm1-reader`.
- **What is said:** `firmware/src/ui_speak.c`, once a UI frame, in the screen's own text. The knob cards are taken from `draw_column` (`sp_card_put`), so a new page speaks without any extra code.
- **The words:** `firmware/words.tsv` says how to read the screen's text ("ATK" → "attack"). The build makes `web/words/<fingerprint>.json` and the link the FM-1 sends. Commit every words file a release links to, and never change or delete a published one: a device out there points at it. Unpublished ones from your own iterations can go.
- **The audit:** `tools/speak_audit.sh` runs fm1-reader's audit over everything ESTAMPIE can say, and must report **0 errors**. It also runs in the tests. Read `--spoken` output yourself too, since the audit can't judge meaning. The rules are in fm1-reader's `skill/fm1-screen-reader/RULES.md`.
- **The emulator page reads its screen too** (`web/emu/index.html`, `worklet.js`). Its voice is eSpeak NG; `tools/osv_scan.sh espeak` tracks its known findings.

## Supply chain

Read [docs/SECURITY.md](docs/SECURITY.md). In short:

- **Pin by content.** Every download is pinned by SHA-256 or commit in `tools/toolchain.lock`, every Python package by hash in `uv.lock`, every GitHub Action by commit, every container by digest. Never `curl | tar`, a floating tag, or "latest".
- **Verify the publisher** where it signs before pinning: minisign for Zig, `gh attestation verify` for uv, and so on. Then wait out a cooldown: nothing less than about two weeks old (uv's `exclude-newer`, Dependabot's `cooldown`).
- **Scan everything.** `tools/osv_scan.sh` scans uv.lock, Node, the hardware emulator, the build container and eSpeak NG. Workflows are linted with zizmor (`uv run --group audit zizmor .github/workflows`). Both run in CI (`.github/workflows/supply-chain.yml`). Use osv-scanner on any new Docker image, package or tool before using it.

## Releasing

GitHub builds and signs every release. Full steps are in [BUILDING.md](BUILDING.md#releasing):

1. **Prepare:** give the version its number in `RELEASES` (`tools/build.py`): each release has its own, and the firmware identity is `FM-1_378` plus that number. Set `ESTAMPIE_VERSION` in `core.h`, write `docs/releases/X.Y.Z.md`, then commit and push.
2. **Build it here first:** `tools/release.sh X.Y.Z`. This also proves it builds.
3. **Publish:** `tools/release.sh X.Y.Z --publish` tags and pushes. `.github/workflows/release.yml` runs the FM-1 Firmware Hub's shared workflow, [zvenson/fm1-build](https://github.com/zvenson/fm1-build) `@v1`. It:
   - builds with `tools/ci_release.sh`, which uses our pinned toolchain, not the workflow's downloads;
   - runs the host tests;
   - signs a build attestation for each file in `build/dist/`: the package, the app, the site zip and its `.sha256`;
   - creates the release, and our `notes` job puts in the notes.

   The hub shows the firmware as **Built from source**. Don't add or replace release files by hand afterwards: that breaks the hub's hash check. Editing the notes is fine.
4. **Check it:** `tools/verify_release.sh X.Y.Z` checks every file's signature and compares it with this machine's build.
5. **The site follows:** fm1-site's update workflow pins the new site zip and opens a PR there.

A **dry run** (Actions › release › Run workflow on a branch) builds and signs without releasing, and also attaches the
generated art headers for comparison.

**Reproducibility.** A release builds byte for byte the same on any machine: from any clone, on macOS and on
GitHub's Linux runner (checked on every dry run and by `tools/verify_release.sh`). What made that work:
- **The site zip:** its files are dated at the commit and zipped in UTC.
- **ABOUT's date:** the commit's (`ESTAMPIE_DATE`), not the build day's.
- **The site's pictures:** PNGs written with Python's own zlib (`tools/gen_emu_art.py`), not each platform's Pillow's.
- **Pillow 13 or later:** 12.x drew Inter Tight's keycap glyphs a pixel apart on macOS and Linux.

If a dry run's files ever differ from a local build, compare the generated headers it attaches and the rasteriser
versions `tools/ci_release.sh` logs.

## The site and the other repositories

- **[ocean/fm1-site](https://github.com/ocean/fm1-site)** serves <https://fm-1.drewr.dev/>, hosted on GitHub Pages. ESTAMPIE ships its part, `/estampie/`, as the release's site zip. Keep the zip's name and its `.sha256`'s. The site's CI checks every page for accessibility (axe, WCAG 2.2 AA), contrast and links. Pages sets each file's type from its extension and takes no per-file headers, so anything meant for reading in a browser is `.txt` or `.html` (`firmware/LICENSE.txt`, not `LICENSE`; `.md` may download). The words files and `.wasm` get the right headers already. It's maintained by another agent session (`fm1-site-2e`); coordinate through it, or through Drew.
- **What `make_site.py` makes:** every page's link-preview card, `llms.txt` and `llms-full.txt` (with absolute links), `words/`, and the emulator's screen reader. It refuses to build without the reader.
- **[charlesvestal/fm1-reader](https://github.com/charlesvestal/fm1-reader)** is pinned in `toolchain.lock`.
- **[zvenson/fm1-build](https://github.com/zvenson/fm1-build)** is used `@v1`, which the hub requires.

## Things that will surprise you

- **The FM-1 shows up as "FM-1 ESTAMPIE"** on USB, both for MIDI and as the audio input (its update loader as
  "FM-1 ESTAMPIE Update"), from 0.3.4; "Felucca" before. "FM-1" stays in front because tools (the Firmware Hub,
  fm1-reader) look for it. The USB id is still Felucca's, pid.codes' test `1209:0001`. Tools accept both names.
- **Some settings live in odd places.** Settings bytes sit in spare rows of the favourites record (`favorites.factory[...]`). KEY TOUCH has row 1 (the retired DIGITAL engine's). Check `settings_persist.c` before taking a byte.
- **The emulator and the device share `ui.c`.** Anything the firmware sends goes through a hook each build supplies, such as `ANN_WIRE`: USB on the device, a buffer in the browser, nothing in the host tests.
- **gcc and clang differ on the HAL.** Host programs that include `hostsim.c` need `-O1` with gcc: at `-O0` it keeps unused HAL functions and chokes on their pi32v2 instructions.
- **Two different "0.1"s.** The `FM-1_378001` identity and the 0.1 version were built but never published. The first published release is v0.2.0.
