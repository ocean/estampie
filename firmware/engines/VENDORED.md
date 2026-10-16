# Felucca-Engines, vendored

This folder is [Felucca-Engines](https://github.com/hugelton/Felucca-Engines) by Leo Kuroshita (Hügelton
Instruments), MIT licence (`LICENSE` here): the ANALOG, LOFI, TRIO, WHEEL and NOISE engines, `dsp.c` and the engine
interface `felucca_engine.h`.

- **Source:** `https://github.com/hugelton/Felucca-Engines.git`
- **Commit:** `c46e0086f69bdc26df1cd144dca9e683cc4531dd` (the one Felucca 1.6.1 pins as its `firmware/engines`
  submodule)

Felucca includes it as a git submodule. ESTAMPIE keeps a copy in the repository instead, so a clone or a source
archive is complete without a second fetch, and every byte that is built can be reviewed here (see
`docs/SECURITY.md`: nothing is fetched at build time that is not pinned).

## Local changes

Two kinds of hook, each marked `ESTAMPIE` and doing nothing unless the host defines its macro:

- `src/eng_{analog,lofi,trio,wheel,noise}.c`: each preset table ends with `ESTAMPIE_<ENGINE>_PRESETS`, which
  appends GHOULBOX's and ESTAMPIE's own presets from `firmware/src/engine_presets.h` (GPL-3.0-only, like the rest
  of ESTAMPIE).
- `src/felucca_engine.h`: `voice_t` has `ESTAMPIE_VOICE_FIELDS` after `side`, where GHOULBOX's `tfine` (TEMP's
  temperaments, `firmware/src/temper.c`) sits in what was padding.

GHOULBOX's TEMP code, which was in `dsp.c` before the engines moved here, is now `firmware/src/temper.c`.

## Updating

```sh
git clone https://github.com/hugelton/Felucca-Engines.git /tmp/fe && git -C /tmp/fe checkout <commit>
diff -ru /tmp/fe firmware/engines        # only the hooks above (and this file) should differ
```

Copy the new files over, keep the hooks, update the commit above, then run `tests/run_tests.sh`: the golden
renders must match unless the change is meant to alter the sound.
