# Generated files kept in the repository

Files a generator makes but that are committed, because they don't come out the same on every platform, and a
release must build byte for byte the same everywhere (BUILDING.md, Releasing).

| File | Made by | Why it's committed |
|---|---|---|
| `ui_keycaps.h` | `tools/gen_aa_keycaps.py` | The keycap pills' labels are drawn in Inter Tight (a variable font, without hinting instructions) by Pillow. The same Pillow, FreeType and HarfBuzz versions draw some of its glyphs a pixel apart on macOS and on Linux (GitHub's runner). |

`tools/build.py` copies them into `build/gen/` instead of running their generators. After changing a generator, run
it on macOS and commit what it makes:

```sh
.venv/bin/python3 tools/gen_aa_keycaps.py assets/generated/ui_keycaps.h
```

`tests/run_tests.sh` checks, on macOS, that each generator still makes exactly its committed file.
