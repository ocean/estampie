/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared PER4 layout: each feature updates only its own fields, preserving
 * the other feature's saved preferences when either is built independently.
 * PER1/PER2 are upstream; PER3 added bold; PER4 added favorites.
 * palette: UI_PAL_TAG + index; an old id (below 20, earlier firmware) is migrated on import.
 * bold: no longer used (one font weight); kept as it was saved, unless it holds the HOLD setting (panel.c).
 * zoom: no longer used (the large readout); kept as it was saved, unless it holds the LEDS setting (panel.c).
 * favorites.factory[15][29..31]: STYLE, MENU's flags, the layers seen (bytes no engine uses; 0 in older settings = every
 * one's default); [15][12]: CLICK / CLICK LEVEL / COUNT-IN (1.1; Felucca's [15][28]), 1.2 SCALE LEDS (bit 6), 1.5.5 AUTO
 * SELECT (bit 7); [15][25]: SCREEN OFF (1.1.5; 0 = NEVER, the default; Felucca's [15][27]). GHOULBOX: [15][27] SCENE
 * (1.3, ui.c ui_scene), [15][28] the one-time marks below; Felucca's moved to bytes free in both, so a GHOULBOX setting
 * keeps its meaning;
 * [15][26]: MENU > DISPLAY > HOME (1.3; 0 = SCOPE, 1 TRACKS, 2 LEVELS (Felucca 1.5), ui.c ui_home_view);
 * [15][24]: MENU's 1.2 flags SCOPE MIX, STEP PREVIEW ON, CHORD ENTRY ADD (bits 0-2, ui.c ui_prefs2; 0 = the defaults),
 *   Felucca 1.5 HELP ON (bit 3), 1.5.5 SELECT PAGE (bit 4; 0 = BPM, as before), SCREEN WAKE PANEL (bit 5; 0 = +MIDI),
 *   SELECT MODW / VEL (bit 6), 1.6.1 2ND CORE ON (bit 7; 0 = OFF, one core, as before);
 * favorites.factory[ML_ROW = 13]: MIDI LEARN's CCs (Felucca 1.5, midi_learn.c; Felucca's row 14 is GURDY's here);
 * [15][10]: MENU > ANIM IDLE (1.2, Discussion #135, ui.c ui_idle: bit 0; 0 = off, ANIM as its bit in MENU's flags says);
 * [15][11]: PRESETS > LIST's category (1.2, Discussion #90, ui.c ui_lcat: 0 none = ALL / FAV as favorites.filter says);
 * [15][0..9], [13] (1.6, #244: CUSTOM, a palette of the user's: five RGB565 colours little endian, flags (Felucca's [12],
 * GHOULBOX's CLICK byte): favorites.c ui_cpal); older
 * firmware keeps the bytes as saved and shows the built-in palette in persist_t.palette.
 * [15][14..23]: the FX layer's key map (1.2, ui_layer.c fx_keys: 16 white keys x 5 bits, perform.c perf_map_of; 0 = the
 * key's default, an unknown code too: kept as saved, a later firmware's effect survives a round trip here).
 * MENU's flags (ui.c PREF_*) are append-only bits whose 0 is the default: BPM LOCK (16) is clear in every older setting
 * = unlocked, LARGE (32) = OFF, RESTORE LAST OFF (128, 1.2) = ON, read as saved (nothing to migrate, so importing twice changes nothing). */
typedef struct {
    uint32_t magic, palette, lowcut, zoom;
    panel_t panel;
    uint32_t bold;
    struct { uint8_t factory[16][32]; uint32_t user, filter; } favorites;
} persist_t;
#define PERSIST_MAGIC 0x50455234u
/* GHOULBOX: USB SERIAL OFF by default (MIDI host boxes and macOS's USB audio want no serial console). The MENU flag
 * is a bit of favorites.factory[15][30] (ui.c ui_prefs, PREF_SERIAL_OFF); byte [15][28] (no engine 15, unused by
 * Felucca) marks GHOULBOX's one-time changes: settings saved without the mark get SERIAL OFF once, then keep what the
 * owner chooses */
#define GB_SERIAL_OFF 64u                           /* ui.c PREF_SERIAL_OFF */
#define GB_MIG_SERIAL 1u
#ifdef PREF_SERIAL_OFF
_Static_assert(GB_SERIAL_OFF == PREF_SERIAL_OFF, "settings_persist.c GB_SERIAL_OFF is ui.c's PREF_SERIAL_OFF");
#endif
/* GHOULBOX 1.5: RESTORE LAST OFF by default (Felucca's is ON), so the FM-1 starts with the SCENE MENU > SYSTEM picks;
 * the same once-only way: settings without the mark get RESTORE LAST OFF, then keep what the owner chooses */
#define GB_RESTORE_OFF 128u                         /* ui.c PREF_RESTORE_OFF */
#define GB_MIG_RESTORE 2u
/* GHOULBOX 2.1: RESTORE LAST back ON (owners wanted their music back at power-on; a fresh device still starts with
 * the SCENE, there being no autosave yet): settings without this mark get it ON once (after 1.5's OFF above, so every
 * older setting ends ON), then keep what the owner chooses */
#define GB_MIG_RESTORE_ON 4u
/* ESTAMPIE (the Felucca 1.6.1 merge): MIDI LEARN's table is favorites.factory[ML_ROW], SLICE's row, which a setting
 * saved before may hold stray favourite bits in: settings without this mark get the row cleared once (nothing learned) */
#define GB_MIG_LEARN 8u
/* ESTAMPIE: KEY TOUCH (key_touch.c kt_load / kt_save) keeps its settings and calibration in favorites.factory[1], the
 * row of DIGITAL (retired: its stars moved to FM6's at every import until now). Settings without this mark get that
 * move once more and the row cleared, then it is KEY TOUCH's */
#define GB_MIG_KT 16u
#ifndef ML_ROW
#define ML_ROW 13u                                 /* (core.h's: a build of this file alone) */
#endif
#ifdef PREF_RESTORE_OFF
_Static_assert(GB_RESTORE_OFF == PREF_RESTORE_OFF, "settings_persist.c GB_RESTORE_OFF is ui.c's PREF_RESTORE_OFF");
#endif

/* Normalize in place; 1 = current, 2 = migrated, 0 = invalid. */
static int settings_import(persist_t *p, int n)
{
    int kt_mig;
    int current = n == (int)sizeof *p && p->magic == PERSIST_MAGIC;
    int old3 = n == (int)(sizeof *p - sizeof p->favorites) && p->magic == 0x50455233u;
    int old2 = n == (int)(sizeof *p - sizeof p->favorites - sizeof p->bold) && p->magic == 0x50455232u;
    int old1 = n == (int)(8u + sizeof(panel_t)) && p->magic == 0x50455231u;
    if (!(current || old3 || old2 || old1)) return 0;
    if (old1) {
        panel_t old;
        memcpy(&old, (uint8_t *)p + 8, sizeof old);
        p->panel = old;
        p->lowcut = p->zoom = 0;
    }
    if (old1 || old2) p->bold = 0;
    if (!current) memset(&p->favorites, 0, sizeof p->favorites);
    if (!(p->favorites.factory[15][28] & GB_MIG_SERIAL)) {   /* GHOULBOX: once, settings from before (see above) */
        p->favorites.factory[15][30] |= GB_SERIAL_OFF;
        p->favorites.factory[15][28] |= GB_MIG_SERIAL;
    }
    if (!(p->favorites.factory[15][28] & GB_MIG_RESTORE)) {   /* GHOULBOX 1.5: once (see above) */
        p->favorites.factory[15][30] |= GB_RESTORE_OFF;
        p->favorites.factory[15][28] |= GB_MIG_RESTORE;
    }
    if (!(p->favorites.factory[15][28] & GB_MIG_RESTORE_ON)) {   /* GHOULBOX 2.1: once, ON (see above) */
        p->favorites.factory[15][30] &= (uint8_t)~GB_RESTORE_OFF;
        p->favorites.factory[15][28] |= GB_MIG_RESTORE_ON;
    }
    if (!(p->favorites.factory[15][28] & GB_MIG_LEARN)) {   /* ESTAMPIE: once, MIDI LEARN's row (see above) */
        memset(p->favorites.factory[ML_ROW], 0, sizeof p->favorites.factory[ML_ROW]);
        p->favorites.factory[15][28] |= GB_MIG_LEARN;
    }
    kt_mig = !(p->favorites.factory[15][28] & GB_MIG_KT);   /* ESTAMPIE: once, DIGITAL's row (see above) */
    p->favorites.factory[15][28] |= GB_MIG_KT;
    if (p->favorites.factory[15][29] > 1u)        /* STYLE (ui.c ui_style): 2, the retired PIXEL = LINE; unknown = FLAT */
        p->favorites.factory[15][29] = p->favorites.factory[15][29] == 2u ? 1u : 0u;
    {   /* CLICK, CLICK LEVEL, COUNT-IN (ui.c ui_rec_prefs): two bits each, 3 is no value: the default (0) */
        uint32_t b = p->favorites.factory[15][12], f;
        for (f = 0; f < 3u; f++)
            if (((b >> (2u * f)) & 3u) == 3u)
                b &= ~(3u << (2u * f));
        p->favorites.factory[15][12] = (uint8_t)b;
    }
    if (p->favorites.factory[15][25] > 7u)         /* SCREEN OFF (1.1.5, ui.c SCR_CODE): unknown = 0 (NEVER since 1.1.5.1) */
        p->favorites.factory[15][25] = 0;
    if (p->favorites.factory[15][11] > 8u)         /* PRESETS LIST's category (1.2, ui.c ui_lcat, CAT_*): unknown = none */
        p->favorites.factory[15][11] = 0;
    /* [15][26] HOME (1.3, ui.c ui_home_view): kept as saved; a value this firmware does not know draws SCOPE */
    {   /* CUSTOM (1.6, #244, favorites.c ui_cpal): [15][0..9] five RGB565 colours, [13] bit 0 stored, bit 1 in use. Anything else
         * in [13], or colours without the stored flag: none (zero). The colours themselves are whatever the user chose */
        uint8_t *f = p->favorites.factory[15];
        f[13] &= 3u;
        if (!(f[13] & 1u)) {
            memset(f, 0, 10);
            f[13] = 0;
        }
    }
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(palette_from_stored(p->palette));
    settings.magic = SETTINGS_MAGIC;
    settings.palette = palette_from_stored(p->palette);
    settings.lowcut = p->lowcut;
    settings.zoom = p->zoom;
    settings_hold = (uint8_t)hold_from_stored(p->bold);
    settings_leds = (uint8_t)leds_from_stored(p->zoom);
#ifdef FELUCCA_FAVORITES
    memcpy(&favorites, &p->favorites, sizeof favorites);
    favorites.filter = favorites.filter == 1u;
#if defined(FM4_NPRESETS) && !FELUCCA_FM4
    if (kt_mig) {   /* DIGITAL's starred presets (engine 1, retired) -> the FM6 presets that cover them (fm4_convert.c) */
        uint32_t k;
        for (k = 0; k < FM4_NPRESETS; k++)
            if ((favorites.factory[ENGI_DIGITAL][0] >> k) & 1u)
                favorites.factory[ENGI_FM6][FM4_TO_FM6[k] / 8u] |= (uint8_t)(1u << (FM4_TO_FM6[k] % 8u));
        memset(favorites.factory[ENGI_DIGITAL], 0, sizeof favorites.factory[ENGI_DIGITAL]);
    }
#endif
#ifdef SMP_SET_PERC
    if ((favorites.factory[ENGI_SAMPLE][0] >> SMP_SET_PERC) & 1u) {   /* SAMPLE's PERC (preset 4, retired): DRUM's */
        favorites.factory[ENGI_DRUM][0] |= 1u;                         /* kit (core.h drum_from_perc) */
        favorites.factory[ENGI_SAMPLE][0] &= (uint8_t)~(1u << SMP_SET_PERC);
    }
#endif
#ifdef KT_ROW
    kt_load(favorites.factory[KT_ROW]);          /* ESTAMPIE: KEY TOUCH */
#endif
#endif
    (void)kt_mig;
    if (p->panel.magic == PANEL_MAGIC) panel = p->panel;
    return current ? 1 : 2;
}

/* GHOULBOX: a device with no saved settings starts with USB SERIAL OFF and RESTORE LAST OFF (marked: the owner's later
 * choices hold) */
static void settings_fresh(void)
{
#ifdef FELUCCA_FAVORITES
    favorites.factory[15][30] |= GB_SERIAL_OFF;                  /* (2.1: RESTORE LAST ON, Felucca's default) */
    favorites.factory[15][28] |= GB_MIG_SERIAL | GB_MIG_RESTORE | GB_MIG_RESTORE_ON | GB_MIG_LEARN | GB_MIG_KT;
#endif
}

/* Start with the last imported/saved object, including fields owned by a
 * feature absent from this build. No save-on-boot or extra flash writes. */
static void settings_export(persist_t *p)
{
    p->magic = PERSIST_MAGIC;
    p->palette = palette_to_stored(settings.palette);
    p->lowcut = settings.lowcut;
    p->zoom = settings.zoom = leds_to_stored(settings.zoom, settings_leds);
    p->panel = panel;
    p->bold = hold_to_stored(p->bold, settings_hold);
#ifdef FELUCCA_FAVORITES
    memcpy(&p->favorites, &favorites, sizeof favorites);
#endif
}
