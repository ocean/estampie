# ESTAMPIE: a mediæval groovebox for the FM-1

**ESTAMPIE** is custom firmware for the **M-VAVE FM-1**, named after the *estampie*, a fast dance of the
13th and 14th centuries. It is a groovebox for lively mediæval music: tavern and court dances rather than the
crypt, with plucked lutes and citoles, buzzing hurdy-gurdies, shawms and bagpipes over drones, frame drums,
tabors and nakers, chant, and the church modes.

It is a fork of [GHOULBOX](https://github.com/jasonpersinger/ghoulbox-fm1-dungeon-synth) by Jason Persinger, the
dungeon-synth firmware, which is itself a fork of [Felucca](https://github.com/hugelton/Felucca) by Leo Kuroshita
(Hügelton Instruments). Nearly everything here is their work: Felucca's engines, sequencer, effects, editor,
installer and tests, and GHOULBOX's hurdy-gurdy, medieval tunings, drone latch, dungeon sounds and browser synth.
Thank you both. The browser emulator follows the design of Charles Vestal's
[X0X](https://github.com/charlesvestal/fm1-x0x), and HOME's chord readout (the chord or notes you are playing) is
ported from Kerem Kilic's [Melodee](https://github.com/keremimo/melodee), another Felucca fork. The full history of
both projects is kept in this repository.

Inspired by the idea of teenage engineering's EP-1320 *medieval*; not affiliated with them, and none of their
sounds are used.

**Status: 0.1, early work.** So far: the VELLUM look (parchment, iron-gall ink, vermilion rubrics, lapis), the
illuminated boot screen and the browser emulator page. The sounds are still GHOULBOX's. See [ROADMAP.md](ROADMAP.md).

## Building

Everything the build needs is fetched into the repository (`.toolchain/`, `.venv/`, both git-ignored), so
nothing is installed in your home directory. Needs [uv](https://docs.astral.sh/uv/), Docker Desktop and git:

```sh
tools/setup_toolchain.sh     # every tool, pinned by hash (tools/toolchain.lock); Python 3.14 from uv.lock
tools/osv_scan.sh            # known vulnerabilities in everything the build runs (docs/SECURITY.md)
./build.sh                   # build/felucca.fwsc: the firmware package (Docker runs the JieLi tools on macOS)
sh web/emu/build.sh          # build/emu/: the browser emulator (zig cc to WebAssembly)
python3 -m http.server -d build/emu 8790     # open http://localhost:8790/
```

Docker Desktop must be running for `./build.sh` on macOS.

## Testing

Three levels, cheapest first:

1. **Browser emulator** (`web/emu/`): the firmware's own C sources compiled to WebAssembly against a simulated
   FM-1. The quick loop for designing sounds and screens.
2. **Host tests**: `tests/run_tests.sh` (with `AC79_SDK=.toolchain/ac79-sdk` and the venv on `PATH`): golden
   renders of every sound, the UI's layout lint, CPU budgets and more. The reverb's HALL cost check fails on
   Apple silicon hosts (it measures host instructions; CI has no counter and skips it); this is inherited.
3. **Emulated hardware**: `tools/hw_check.sh` boots the real `build/felucca.fwsc` on an emulation of the
   AC791N's pi32v2 cores and the board
   ([shcherbakov7/m-wave-fm1-browser-emulator](https://github.com/shcherbakov7/m-wave-fm1-browser-emulator), Rust),
   and saves the screen to `build/hw/screen.png`. This checks the real image: memory layout, start-up, timing.

Then a real FM-1. M-VAVE's own updater returns it to stock firmware.

## Licence

GPL-3.0-only, as GHOULBOX and Felucca. See [LICENSING.md](LICENSING.md) for the parts under other licences.
GHOULBOX's own README is kept at [docs/GHOULBOX-README.md](docs/GHOULBOX-README.md).

M-VAVE and FM-1 are trademarks of their respective owners. ESTAMPIE, GHOULBOX and Felucca are not affiliated with
or endorsed by them.
