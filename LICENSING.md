# Felucca licensing

Felucca is free software, licensed under the GNU General Public License, version 3 only
(`GPL-3.0-only`, full text in `LICENSE`). That covers the code and its own assets. A few
bundled or ported parts keep their own licences; they are listed below, and their licence
texts are in `LICENSES/`.

Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments

## What is GPL-3.0-only

Every file in this tree that carries an `SPDX-License-Identifier: GPL-3.0-only` header:

- the firmware: `firmware/` (app, HAL, update loader; not `firmware/engines/`, MIT: below)
- the build script and tools: `build.sh`, `tools/` (not `tools/gen_waves.py`, the Hügelton Sample Pack: CC0 1.0, below)
- the web pages (installer, editor, the browser emulator `web/emu/`) and their tests: `web/` (not the Fukiai and DotGothic16 fonts, below)
- the host tests: `tests/`

Three source files are ports and keep the licence of their originals:
`firmware/src/phys_dsp.c` (DaisySP, MIT), `firmware/src/phys_symp.c` (Rings, MIT) and
`firmware/src/fm6_core.c` (msfa, Apache-2.0). The firmware built with them is GPL-3.0-only as
a whole.

`firmware/src/ota.c` (GPL-3.0-only) includes `ota_boot_valid()` and the check in `ota_stage()` that
calls it, from [felucca-bsp](https://github.com/hugelton/felucca-bsp) commit `afd83ed` by Charles
Vestal, under the Mozilla Public License 2.0 (`LICENSES/MPL-2.0-felucca-bsp.txt`). MPL-2.0 lets that
code be part of this GPL-3.0-only firmware; on its own it stays available under MPL-2.0 from
felucca-bsp.

Five engines and the code they share are under the MIT licence, not the GPL. They are
Hügelton Instruments' own work and live in their own repository, Felucca-Engines
(<https://github.com/hugelton/Felucca-Engines>), copied here (vendored at a pinned commit, see
`firmware/engines/VENDORED.md`) as `firmware/engines/`: the ANALOG, LOFI, TRIO, WHEEL and NOISE engines (`eng_analog.c`,
`eng_lofi.c`, `eng_trio.c`, `eng_wheel.c`, `eng_noise.c`), the engine interface
(`felucca_engine.h`), the shared DSP helpers (`dsp.c`) and the generator of their tables
(`SPDX-License-Identifier: MIT`, `firmware/engines/LICENSE`, also
`LICENSES/MIT-Felucca-Engines.txt`). The firmware built with them is GPL-3.0-only as a whole.
ESTAMPIE's two hooks in those files (marked `ESTAMPIE`, VENDORED.md) are its own; the presets they pull in
(`firmware/src/engine_presets.h`) are GPL-3.0-only. A clone or source archive of ESTAMPIE is complete.

You may use, study, change and share Felucca under the GPL. If you distribute Felucca, or
firmware derived from it, you must also give your recipients its complete corresponding
source under the same licence. That includes devices that ship with modified Felucca inside.

## Hügelton Instruments' own work

All by Hügelton Instruments (Leo Kuroshita), in this tree:

| What | Licence | Where |
| --- | --- | --- |
| Felucca: firmware, tools, web pages, tests | GPL-3.0-only | `LICENSE` |
| The PHASE engine | GPL-3.0-only | `firmware/src/eng_phase.c` |
| The DRUM voices and kit | GPL-3.0-only | `firmware/src/drum_voice.c`, `firmware/src/eng_drum.c` |
| The Hügelton Sample Pack: Felucca's drum sounds, made by `tools/gen_waves.py`, from which the SLICE engine's BREAK is built (only in a `FELUCCA_SLICE=1` build); the script and the sounds it makes | CC0 1.0 (<https://creativecommons.org/publicdomain/zero/1.0/>) | `tools/gen_waves.py` |
| The controls picture | GPL-3.0-only | `docs/controls.jpg` |
| The Fukiai icon font (<https://github.com/hugelton/Fukiai>): the firmware's icons (rasterised at build time by `tools/gen_aa_icons.py`) and the web editor's | MIT | `web/fukiai.ttf`, `LICENSES/MIT-Fukiai.txt`, `web/FUKIAI-LICENSE.txt` |
| The ANALOG, LOFI, TRIO, WHEEL and NOISE engines, the engine interface and the shared DSP helpers: Felucca-Engines (<https://github.com/hugelton/Felucca-Engines>) | MIT | `firmware/engines/`, `LICENSES/MIT-Felucca-Engines.txt` |

## Third-party material

| What | Licence | Where |
| --- | --- | --- |
| Inter Tight font by The Inter Project Authors: the keycaps' labels, rasterised into the firmware at build time (`tools/gen_aa_keycaps.py`), and the panel diagram (the generated pictures are not offered as a font, and the font declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/InterTight[wght].ttf`, `LICENSES/OFL-InterTight.txt` (also `assets/fonts/OFL.txt`) |
| Alegreya Sans font (Medium) by The Alegreya Sans Project Authors (ESTAMPIE): the UI text, rasterised into the firmware at build time (`tools/gen_aa_font.py --preset estampie`; the generated tables are not offered as a font; a capital's tail is cut 1 or 2 rows under the baseline in them; the font itself is unmodified, from google/fonts at commit 1d0e46c, and declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/AlegreyaSans-Medium.ttf`, `LICENSES/OFL-AlegreyaSans.txt` |
| Cinzel font by The Cinzel Project Authors (ESTAMPIE): the large text, rasterised into the firmware at build time (`tools/gen_aa_font.py --preset estampie`; the generated tables are not offered as a font; the font itself is unmodified, from google/fonts at commit 1d0e46c, and declares no Reserved Font Name) | SIL OFL 1.1 | `assets/fonts/Cinzel[wght].ttf`, `LICENSES/OFL-Cinzel.txt` |
| VT323 font by The VT323 Project Authors (Peter Hull) (GHOULBOX): GHOULBOX's UI text (`tools/gen_aa_font.py --preset ghoulbox`, no longer the build's) and the video tools | SIL OFL 1.1 | `assets/fonts/VT323-Regular.ttf`, `LICENSES/OFL-VT323.txt` |
| UnifrakturMaguntia font by j. 'mach' wust and Peter Wiegel (ESTAMPIE): the boot / ABOUT wordmark and its illuminated initial, rasterised into the firmware at build time (`tools/gen_estampie_art.py`; the generated bitmaps are pictures of one word, not offered as a font; the font itself is unmodified) | SIL OFL 1.1 (Reserved Font Name "UnifrakturMaguntia") | `assets/fonts/UnifrakturMaguntia-Book.ttf`, `LICENSES/OFL-UnifrakturMaguntia.txt` |
| IM Fell English and IM Fell English SC by Igino Marini (ESTAMPIE): the web pages' and the user guide's text and headings, loaded from Google Fonts (not in this tree) | SIL OFL 1.1 | <https://fonts.google.com/specimen/IM+Fell+English> |
| Alegreya by Juan Pablo del Peral (Huerta Tipográfica), and Alegreya Sans and Cinzel as above (ESTAMPIE): the user guide's text, its key names and numbers, loaded from Google Fonts | SIL OFL 1.1 | <https://fonts.google.com/specimen/Alegreya> |
| Pirata One font by Rodrigo Fuenzalida and Nicolas Massi (GHOULBOX): the boot / ABOUT wordmark, rasterised into the firmware at build time (`tools/gen_ghoulbox_art.py`; the generated bitmaps are pictures of one word, not offered as a font; the font itself is unmodified) | SIL OFL 1.1 (Reserved Font Name "Pirata") | `assets/fonts/PirataOne-Regular.ttf`, `LICENSES/OFL-PirataOne.txt` |
| Instrument samples (Versilian Studios VSCO-2 Community Edition, VCSL): the SAMPLE sets (and, in a `FELUCCA_SLICE=1` build, the SLICE engine's PIANO) | CC0 1.0 | `assets/samples-cc0/`, provenance in `ATTRIBUTION.txt` there |
| DaisySP by Electrosmith, Corp and Emilie Gillet (<https://github.com/electro-smith/DaisySP>): the PHYS engine's modal and string models and the resonator, ported to fixed point | MIT | `firmware/src/phys_dsp.c`, `LICENSES/MIT-DaisySP.txt` |
| Rings by Emilie Gillet (<https://github.com/pichenettes/eurorack>): the PHYS engine's sympathetic strings, ported to fixed point | MIT | `firmware/src/phys_symp.c`, `LICENSES/MIT-Rings.txt` |
| msfa by Google Inc. and Pascal Gauthier, from Dexed (<https://github.com/asb2m10/dexed>): the FM6 engine's synthesis, ported to integer C (Dexed itself is GPL-3.0; only msfa is used; the FM6 factory patches are Felucca's own) | Apache-2.0 | `firmware/src/fm6_core.c`, `LICENSES/Apache-2.0-msfa.txt` |
| klattsch by Tony Gies (<https://github.com/tgies/klattsch>): design reference for the VOICE engine; no code copied. Formant data from Klatt (1980) / Hillenbrand et al. (1995) | MIT (klattsch) | credit only |
| X0X by charlesvestal (<https://github.com/charlesvestal/fm1-x0x>), a Felucca fork: the design of the browser emulator (the worklet, the device clock driven by its audio, the exports); its files credit it in their headers | GPL-3.0 | `web/emu/` |
| KEY TOUCH, velocity and aftertouch from the keys: the sensing (the converter on the key rows, the driven row, the landing's depth on a log scale, the pressure past the key's rest) ported from Velo and MONK-1 by charlesvestal (<https://github.com/charlesvestal/fm1-velocity>, <https://github.com/charlesvestal/fm1-monk>) to integer arithmetic | GPL-3.0 | `firmware/src/key_touch.c`, `firmware/hal/fm1_input.h`, `firmware/hal/fm1_adc.h` |
| The screen reader: fm1-reader by Charles Vestal (<https://github.com/charlesvestal/fm1-reader>, commit pinned in `tools/toolchain.lock`): its protocol code and the words-file generator, copied unchanged (`tools/sync_announce.sh --check`); the emulator page's words and voice code, copied in at build time | MIT | `firmware/src/announce.c`, `tools/gen_words.py`, the site's `webapp/emu/reader/`, `LICENSES/MIT-fm1-reader.txt` |
| eSpeak NG (<https://github.com/espeak-ng/espeak-ng>), compiled to WebAssembly in fm1-reader (English only), with its wrapper: the emulator page's own voice, a choice under its panel. Served beside the page, not linked into it; its source is the commit in its README plus the wrapper | GPL-3.0-or-later | the site's `webapp/emu/reader/espeak/` (with its `LICENSE` and `README.md`) |
| The second core: its start, mailbox and guards from X0X by charlesvestal (<https://github.com/charlesvestal/fm1-x0x>, with the dual-core findings of Melodee by keremimo, <https://github.com/keremimo/melodee>); its audio split, measurements and hardening after Jangada by zednaked (<https://github.com/zednaked/jangada>); each credited in its files | GPL-3.0 | `firmware/hal/fm1_cpu1.{h,S}`, `firmware/src/fx.c`, `tests/c1_host.h`, `tests/dualcore_test.c` |
| DotGothic16 font by The DotGothic16 Project Authors (<https://github.com/fontworks-fonts/DotGothic16>): the browser emulator's knob labels, served with its page (cut to printable ASCII; the font declares no Reserved Font Name) | SIL OFL 1.1 | `web/emu/fonts/DotGothic16-subset.woff`, `LICENSES/OFL-DotGothic16.txt` (also `web/emu/fonts/OFL.txt`) |
| libflac.js 5.6.0 by DFKI GmbH (<https://github.com/mmig/libflac.js>): the web synth's FLAC encoder wrapper | MIT | `web/emu/vendor/libflac/libflac.min.wasm.js`, `LICENSES/MIT-libflac.js.txt` |
| libFLAC 1.3.4 by Josh Coalson and the Xiph.Org Foundation (<https://github.com/xiph/flac/tree/1.3.4>): compiled into the web synth's FLAC WebAssembly module | BSD 3-Clause | `web/emu/vendor/libflac/libflac.min.wasm.wasm`, `LICENSES/BSD-3-Clause-libFLAC.txt` |
| wasm-media-encoders 0.7.0 by arseneyr (<https://github.com/arseneyr/wasm-media-encoders>): the web synth's MP3 encoder wrapper | MIT | `web/emu/vendor/mp3/WasmMediaEncoder.min.js`, `LICENSES/MIT-wasm-media-encoders.txt` |
| LAME by the LAME project (<https://github.com/arseneyr/lame/tree/98db548e8e851defbba3184125ce10725355c332>): compiled into the web synth's separately loaded MP3 WebAssembly module | LGPL-2.0-or-later | `web/emu/vendor/mp3/mp3.wasm`, `LICENSES/LGPL-2.0-LAME.txt`, `LICENSES/LAME-NOTICE.txt` |
| JieLi AC79 SDK by JieLi Technology: three of its files go into every `.fwsc` package (below); none are in this tree | Apache-2.0 | `LICENSES/Apache-2.0.txt` |
| Official FM-1 V15 firmware by M-VAVE (<https://github.com/m-vave/fm1_open>): installed, unmodified, by the installer's "Return to official V15" (below); not part of the firmware | Apache-2.0 | `assets/m-vave/FM-1_015.fwsc`, `assets/m-vave/README.md`, `LICENSES/Apache-2.0.txt` |

The exact browser-codec versions, source commits, build-source links and binary SHA-256 hashes are recorded in
`web/emu/vendor/README.md`. The LAME WebAssembly file is loaded as a separate runtime module and can be replaced
with a compatible build from the cited source. None of these codecs is linked into the FM-1 firmware.

On the device, HOME held > ABOUT opens the information screen; turning PRESETS scrolls on into
CREDITS, a short list of these authors, licences and source URLs.

## JieLi SDK files in the packages (Apache-2.0)

A `.fwsc` package made by `tools/build.py` (with `tools/fm1pkg_make.py`; this is the package the
web installer installs) holds three unmodified files from the JieLi AC79 SDK
(<https://gitee.com/Jieli-Tech/fw-AC79_AIoT_SDK>, `cpu/wl82/tools/`). They are read from your SDK
checkout at build time (see BUILDING.md); no SDK files are in this tree.

| File in the package | What it is |
| --- | --- |
| `uboot.boot` | the first-stage boot loader (SPL) |
| `cfg_tool.bin` | the chip configuration block |
| `eq_cfg_hw.bin` | the default hardware EQ table |

They are licensed under the Apache License, Version 2.0 (`LICENSES/Apache-2.0.txt`), not under
the GPL, and their copyright stays with JieLi Technology. The SDK has no NOTICE file. Apache-2.0
files may be distributed together with GPL-3.0 code; Felucca itself stays GPL-3.0-only.

Every distribution of a package carries the licence texts with it: the release folder
(`tools/build.py --release`) and the site (`web/make_site.py`, next to the package in
`firmware/` and linked from the installer page).

## No vendor material in the firmware

No M-VAVE material is part of the firmware. It links no vendor code and contains no vendor
data, and the packages hold only the firmware and the three SDK files above.

ESTAMPIE: the installer's "Return to official V15" installs M-VAVE's own official V15 firmware,
`assets/m-vave/FM-1_015.fwsc`, unmodified. M-VAVE publish it under the Apache License, Version 2.0
(<https://github.com/m-vave/fm1_open>, `THIRD_PARTY.md`); the site carries it in `firmware/stock/`
with that licence and its provenance (`assets/m-vave/README.md`). The installer checks its SHA-256
before writing anything, and still accepts a copy you downloaded from M-VAVE yourself.

## Contributions

Contributions are welcome under GPL-3.0-only. Changes to the five engines and the code they
share (`firmware/engines/`) go to the Felucca-Engines repository, under the MIT licence.

## Trademarks

"Felucca" and "Hügelton Instruments" are names of Hügelton Instruments.

"M-VAVE" and "FM-1" are trademarks of their respective owners. Felucca is independent
firmware that runs on FM-1 hardware. It is not affiliated with, endorsed by or supported
by those owners.

## Radio

Felucca never enables the Bluetooth / Wi-Fi radio of the hardware.
