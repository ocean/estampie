# ESTAMPIE roadmap

Plans change; nothing here is a promise. GHOULBOX's own roadmap is kept at
[docs/GHOULBOX-ROADMAP.md](docs/GHOULBOX-ROADMAP.md).

## Done: 0.1, the look

- Builds with an in-repo toolchain (`tools/setup_toolchain.sh`), checked on emulated hardware (`tools/hw_check.sh`).
- VELLUM palette (the default), an illuminated boot screen, ABOUT and credits.
- The browser emulator page as a manuscript leaf.

## Next

- **Marginalia**: replace GHOULBOX's wall torches beside the graphs with something from a manuscript's margins.
- **Installer, editor and tools pages** in the same style.
- **A brighter power-on scene**: a dance at a dance's tempo, instead of the dungeon.

## The sounds

In this order, each with golden renders and a page to hear it:

1. **Period percussion**: tabor, nakers, frame drum, riq and tambourine, clappers, bells, chain rattle, battle toms.
   Punchy, for dancing.
2. **Plucked strings**: lute, citole, gittern, psaltery; a harp with bray pins.
3. **Reeds and winds**: a brighter shawm, crumhorn, rauschpfeife, bagpipe with drones, recorder, herald trumpet.
4. **Portative organ** and a reworked **chant**.
5. **Kora and uilleann pipes**, beyond the mediæval set but close to it in spirit:
   - Kora: PHYS's SYMP model is most of the way there already (a plucked string driving sympathetic strings, after
     Rings, with BUZZ for the rattle). It needs a harp-like attack, the gourd's body resonance and a pentatonic or
     heptatonic tuning on the keys.
   - Uilleann pipes: GURDY's drones under a reedy chanter, and the regulators' chords on the chord keys. The
     chanter's sweet, conical-bore tone could come from PHYS, FM6 or VOICE; Melodee's CZ phase-distortion engine
     is worth a look for it. A CC0 recording for SAMPLE is the fallback.
   - Also check what SLOOP's and Melodee's sound models do that ours don't, and port what fits (with credit).
6. **Dances**: patterns on the estampie, saltarello, ductia and trotto, and the rhythmic modes.
7. **Modes**: all the church modes beside major and minor, with GHOULBOX's tunings.

## Later

- A phrase library, as the EP-1320 has.
- Foley: swords, horses' hooves, the crowd.
- New GHOULBOX and Felucca releases merged as they come.
