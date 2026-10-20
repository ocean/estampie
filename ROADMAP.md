# ESTAMPIE roadmap

Plans change; nothing here is a promise. GHOULBOX's own roadmap is kept at
[docs/GHOULBOX-ROADMAP.md](docs/GHOULBOX-ROADMAP.md).

## Done: 0.2.0, the touch (the first release)

- KEY TOUCH: velocity and aftertouch from the 20 lower keys, with no hardware change (Charles Vestal's Velo and
  MONK-1): each key learns your touch as you play, and MENU > SYSTEM > CALIBRATION > KEY TOUCH calibrates it.
- The installer: official V15 bundled (and its factory sounds restored, as a separate step), sturdier connecting,
  build IDs, an optional full backup first. The editor checks a saved project against the FM-1's.
- The site: a front page, llms.txt, link-preview cards; the Sound Lab with touch and the MOD matrix.

## Done: 0.1, the look (not published on its own)

- Builds with an in-repo toolchain (`tools/setup_toolchain.sh`), checked on emulated hardware (`tools/hw_check.sh`).
- VELLUM palette (the default), an illuminated boot screen, ABOUT and credits.
- The browser emulator page as a manuscript leaf.

## Next

- **Marginalia**: replace GHOULBOX's wall torches beside the graphs with something from a manuscript's margins.
- **Installer, editor and tools pages** in the same style.
- **A brighter power-on scene**: a dance at a dance's tempo, instead of the dungeon.
- **The user guide's PDF**, made again from the restyled guide (`docs/guide/make_guide.py pdf`) and attached to a
  release; and the panel diagram (`docs/controls.jpg`, still Felucca's dark one) in the VELLUM style.

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
- New GHOULBOX and Felucca releases merged as they come (Felucca 1.6.1: merged in 0.1).
