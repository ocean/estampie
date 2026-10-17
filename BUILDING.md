# Building Felucca

The build makes three files in `build/`:

| File | What |
| --- | --- |
| `felucca.bin` | the firmware app |
| `loader/ota.bin` | the update loader |
| `felucca.fwsc` | the installable package (app + loader) |

## Get the source

```
git clone https://github.com/ocean/estampie.git
```

Felucca keeps five engines in a git submodule; ESTAMPIE has them in the repository (`firmware/engines/`,
[Felucca-Engines](https://github.com/hugelton/Felucca-Engines), MIT, vendored at the commit Felucca 1.6.1 pins: see
`firmware/engines/VENDORED.md`), so a plain clone or source archive is complete.

## Prerequisites (macOS)

ESTAMPIE: one command fetches everything into the repository (`.toolchain/`, `.venv/`, both git-ignored), each
download checked against its pinned SHA-256 or commit in `tools/toolchain.lock` before it is unpacked:

```
tools/setup_toolchain.sh          # add --hw for the pi32v2 hardware emulator (tools/hw_check.sh; needs cargo)
```

It needs [uv](https://docs.astral.sh/uv/), git, curl and Docker Desktop, and fetches:

- the JieLi Linux toolchain (clang 4.0.1 for pi32v2). It is Linux x86-64 only: on macOS the build runs each tool in
  the pinned `gcr.io/distroless/cc-debian13:nonroot` container (Rosetta on Apple silicon), with no network, no
  capabilities and the sources read-only. Keep the source tree in a folder Docker can share, e.g. under `/Users`.
- three files of the JieLi AC79 SDK (Apache-2.0: `cpu/wl82/tools/uboot.boot`, `cfg_tool.bin`, `cfg/eq_cfg_hw.bin`),
  at a pinned commit;
- Python 3.14 and its packages from `uv.lock` (hash-pinned; the UI font and icons are rasterised at build time);
- Zig, whose `zig cc` compiles the browser emulator (`web/emu/`) to WebAssembly (one minisign-signed download; no
  Node, Python or npm in that build);
- Node (the binary alone, no npm) for the web page and emulator tests;
- osv-scanner, for `tools/osv_scan.sh`.

[docs/SECURITY.md](docs/SECURITY.md) explains the pins, the scans and how to update them. Felucca's
`tools/get_toolchain.sh` (an unpinned download into `~/.jieli`) is kept for reference only.

On Linux x86-64 the toolchain runs natively and Docker is not needed.

## Build

```
./build.sh
```

`JIELI_TOOLCHAIN` and `AC79_SDK` override the default locations
(`.toolchain/jieli/toolchain`, `.toolchain/ac79-sdk`); `JIELI_DOCKER_IMAGE` overrides the pinned container.

`./build.sh --release 0.1` makes a release build: the package identity becomes `FM-1_37801`
(378 spells "EST" on a phone keypad; dev builds are `FM-1_37800`) and the version string `0.1`;
the package is `build/estampie-0.1.fwsc`, and `build/release-0.1/` holds what a release ships:
the package, the app (`estampie-0.1-app.bin`), `SHA256SUMS`, the sample attribution, `LICENSE`,
`LICENSING.md` and
`LICENSES/` (the package contains Apache-2.0 SDK files, so the licence texts travel with it).

Build options (environment, `0` or `1`; defaults in `firmware/src/felucca.c`, `core.h` and `icons.c`):

| Flag | Default | |
| --- | --- | --- |
| `FELUCCA_FLASH` | 1 | settings, presets and projects in flash |
| `FELUCCA_OTA` | 1 | update entry (needs `FELUCCA_FLASH`) |
| `FELUCCA_CDC` | 1 | USB serial console |
| `FELUCCA_CDC_DEFAULT` | 1 | `0`: the console is built in but left out of USB from boot (as MENU > USB SERIAL OFF) |
| `FELUCCA_UAC` | 1 | USB audio input (the master output, stereo, 44.1 or 48 kHz) |
| `FELUCCA_UAC_48K` | 1 | `0`: the USB audio input at 44.1 kHz only, as up to 1.0.5.2 (`firmware/src/usb.c`) |
| `FELUCCA_UAC_AS_FIRST` | 1 | the audio control header lists the audio streaming interface before the MIDI one, so the USB audio driver of older macOS (up to 15) finds it; `0`: 1.0 to 1.5's order, byte for byte (`firmware/src/usb.c`) |
| `FELUCCA_CPU2` | 1 | the second core is built in (MENU > SYSTEM > 2ND CORE, OFF by default); `0`: none of it, 1.6's one-core code and RAM (`firmware/hal/fm1_cpu1.h`, `firmware/src/fx.c`) |
| `FELUCCA_UART` | 1 | TRS MIDI IN |
| `FELUCCA_SLICE` | 0 | the SLICE engine and its BREAK and PIANO samples (~70 KB; kept in the tree, its tests build it) |
| `FELUCCA_ICONS` | 1 | parameter icons on the knob cards |
| `FELUCCA_FM4` | 0 | the retired DIGITAL engine (4-operator FM) instead of its FM6 conversion |

`FELUCCA_USB_LAYOUT` (`0` to `3`, default `0`) picks other USB descriptor layouts for testing; see
`firmware/src/usb.c`.

The build fails when the image leaves less than 16 KiB of the 581,564-byte app slot free (from GHOULBOX 2.2), so
there is always room for a fix: a new feature has to find its bytes first. `FELUCCA_SPARE_MIN` (bytes) changes the
floor, e.g. `FELUCCA_SPARE_MIN=0` for an experiment.

## Samples

The CC0 instrument samples that the SAMPLE engine uses are in `assets/samples-cc0/`
(Versilian Studios, see `ATTRIBUTION.txt` there). `tools/fetch_cc0.py` downloads them
again from the source repositories. Without that folder the build still works, with no
instrument sets in the SAMPLE engine: it plays a plain sine
at the note's pitch instead, and the screen says NO SAMPLE.

## Browser emulator

```
./build.sh
web/emu/build.sh
python3 -m http.server -d build/emu 8790     # open http://localhost:8790/
```

`web/emu/felucca_web.c` builds the firmware's sources as the host tests do, against a simulated FM-1, and
`zig cc` compiles it to WebAssembly, a WASI reactor with no imports (`build/emu/felucca.wasm`, with `index.html`, `worklet.js`, the browser helper
modules and the font in `fonts/`). The page keeps the flash's storage sectors (projects, user presets, settings) in
the browser. It can export and import its complete state and record the final stereo output as a 24-bit WAV. Not
emulated: USB (audio, MIDI out, the editor, updates), TRS MIDI and the user sample slots. ESTAMPIE's site will have it
at <https://fm-1.drewr.dev/estampie/> (Felucca's own: <https://hugelton.github.io/Felucca/webapp/try/>).

## Tests

```
tests/run_tests.sh
```

Runs the host tests and, with Node.js, the web page tests (and, with Zig too, the browser emulator's). Run it after `./build.sh`
(it uses `build/`, the in-repo SDK, Python and Node). The suites cover flash storage,
user presets, projects of every format, backup, the keys and knobs, MIDI (USB, TRS, clock,
control), USB audio, the update entry and loader, the command-line installer, the UI (the real
drawing code against stubs: every screen in every palette is rendered and checked for clipped or
overlapping text; PNGs land in `build/ui_new/`), every engine (DRUM, NOISE, PHYS, FM6, SLICE, the
DIGITAL conversion), the chord keys, the modulation matrix, the FX layer, the reverbs, the SLICER
and swing. With `DAISYSP` pointing at a DaisySP checkout, the PHYS models are also compared with
their floating-point originals; without it that test is skipped. Felucca-Engines' own test
(`firmware/engines/tests/run.sh`) runs too, and its standalone host is built once more with Felucca's tables
against the same golden renders.

The regression suite (`tests/regress.c`) renders every engine and preset and compares a
hash of each render with `tests/golden.txt`; it also checks levels, voices and the CPU
cost (`tests/cpu_baseline.txt`, `tests/target_budget.txt`). After an intended change of
the sound, `GOLDEN_UPDATE=1 sh tests/run_tests.sh` rewrites the hashes; `BUDGET_UPDATE=1`
does the same for the cost files.

## Install

Use the web installer in Chrome or Edge:
<https://hugelton.github.io/Felucca/webapp/installer/>. It installs the released package.

From the command line (needs mido and python-rtmidi: `uv sync`, the device group):

```
python3 tools/fm1_install.py build/felucca.fwsc
python3 tools/fm1_install.py --info          # identity of the connected FM-1
```

Or, to install your own build from the web installer, make a local copy of the site and open it from `localhost`
(Web MIDI needs a secure context):

```
python3 web/make_site.py build/felucca.fwsc dev .artifacts/tmp/site
python3 -m http.server -d .artifacts/tmp/site 8000
# open http://localhost:8000/webapp/installer/
```

## The site

ESTAMPIE's site is one folder of <https://fm-1.drewr.dev/>, the site for Drew's FM-1 projects (ESTAMPIE, FM1Kit and
FM-SWITCH), each in a folder of its own: ESTAMPIE's is `/estampie/`. `web/make_site.py` makes that folder's contents:

```
./build.sh --release 0.1                   # build/estampie-0.1.fwsc
tests/run_tests.sh                         # build/host/desc.json: the guide's sound list
sh web/emu/build.sh                        # build/emu: the emulator
python3 web/make_site.py build/estampie-0.1.fwsc 0.1 build/site
```

`build/site/` then holds `index.html` (to the emulator), `webapp/emu/`, `webapp/installer/`, `webapp/editor/`, `guide/`
and `firmware/`; copy it to `estampie/` in the FM-1 site. Every link inside it is relative, so it works in any folder
(and from `localhost`); the address the guide's text shows is `ESTAMPIE_SITE` (default `fm-1.drewr.dev/estampie`).
It has no `CNAME`: the domain belongs to the site above it. The projects share one origin, so the pages' browser
storage is named for ESTAMPIE (`estampie-…`: the emulator's saves, the editor's library and tab, the installer's
resume), never Felucca's or GHOULBOX's names, which the other projects' pages may use too.

Installing firmware is at your own risk. If an install fails and the FM-1 no longer
starts, recovery needs [FM-1-transporter](https://github.com/kurogedelic/FM-1-transporter).
