/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Sound categories (1.2, Discussion #90): every sound in the PRESETS list belongs to one, so KNOB 4 (LIST) can show
 * only the basses, the pads, .. (ui.c list_mode). Factory presets: a static table here (a letter per preset, in the
 * engine's preset order); user presets: a byte of their record (upreset.c up_cat, set on the NAME screen when saving
 * or renaming, ui_name.c). Included by ui.c and upreset.c (the host build of the user presets needs the numbers only).
 * The numbers are stored (a user preset's record, the editor's UP_LIST / UP_GET / UP_PUT): append-only. CAT_NONE is a
 * record of before (or the editor's without one): its category follows from its engine (cat_of_engine) */
#ifndef FELUCCA_CATEGORY
#define FELUCCA_CATEGORY 1
enum { CAT_NONE, CAT_BASS, CAT_LEAD, CAT_PAD, CAT_PLUCK, CAT_KEYS, CAT_DRUM, CAT_FX, CAT_OTHER, CAT_N };
static const char *const CAT_NAME[CAT_N] = {"-", "BASS", "LEAD", "PAD", "PLUCK", "KEYS", "DRUM", "FX", "OTHER"};
/* the category a sound without one is listed under: a kit (DRUM, SLICE: engines 10, 13) DRUM, any other OTHER */
static uint32_t cat_of_engine(uint32_t e) { return e == 10u || e == 13u ? CAT_DRUM : CAT_OTHER; }

#ifndef UP_HOST
/* the factory presets' categories: per engine number (engines.c ENGINES[]), a letter per preset in its order (B BASS,
 * L LEAD, P PAD, U PLUCK, K KEYS, D DRUM, F FX, O OTHER); a preset past its string (DIGITAL's, FELUCCA_FM4 builds
 * only; SAMPLE's retired PERC, past SMP_NPRESETS): its engine's (cat_of_engine). tests/ui_test.c checks every string
 * against its engine's preset count. A preset's category is a hint for browsing: nothing else reads it */
static const char *const PRESET_CAT[16] = {   /* GHOULBOX: its sounds after Felucca's, GURDY at 14 */
    "LPBPBKLBULLP" "PPLB",       /* 0 ANALOG: SAW LEAD, SOFT PAD, SQR BASS, PWM STR, ACID, SINE KEY, RAVE, SUB BASS,
                                  * PLUCK, BRASS, WIND, STRINGS; CRYPT PAD, FROST STR, WAR HORN, DIRGE BASS */
    "",                          /* 1 DIGITAL (retired) */
    "LKPLUU" "L",                /* 2 PHASE: BRASS, ORGAN, STRING, RESO, BELL, WIRE; GRIM BRASS */
    "LBLLL" "LPL",               /* 3 LOFI: PULSE LD, WAVE BASS, ARP 8BIT, WAVE LEAD, STEP LEAD; TOWER LEAD, CASIO CHOIR,
                                  * RECORDER */
    "KKLL" "K" "PKLU",           /* 4 SAMPLE: PIANO, (PIANO: the retired TRANH's alias), FLUTE, (FLUTE: the retired SAX's
                                  * alias), (PIANO: the retired PERC's alias); BOWED PSALT, REN ORGAN, TENOR RECORD, FOLK HARP */
    "PLBF" "PPFP",               /* 5 VOICE: CHOIR AAH, VOX LEAD, WOW BASS, WHISPER; MONKS, CRYPT CHOIR, WRAITH, LOW DRONE */
    "BLLUP" "PLPL",              /* 6 TRIO: FAT BASS, ARP LEAD, SYNC LEAD, RING BELL, CHIP CHOIR; FANTASY PAD, MOURN HORN,
                                  * STRING MACH, SHAWM */
    "KKKKK" "KKKK",              /* 7 WHEEL: the organs; CRYPT ORGAN, CATHEDRAL, HARMONIUM, TOWER FLUTE */
    "PFPP",                      /* 8 GRAIN: CLOUD PAD, GLITCH, FROZEN, SHIMMER */
    "UUUPUDDPU" "UKUP",          /* 9 PHYS: BELL TREE, MARIMBA, PLUCK, BOWED METAL, KALIMBA, HAND DRUM, TOMS, DRONE
                                  * STRING, HARP; LUTE, HARPSICHORD, DUNGEON HARP, GURDY DRONE */
    "D" "DD" "DD" "DDD",         /* 10 DRUM: DRUM KIT; CRYPT KIT, TOMB DRUMS; DANCE KIT, PROCESSION; Felucca 1.5.6's DEEP
                                  * KIT, TIGHT KIT, PERC KIT */
    "FFFF" "FF",                 /* 11 NOISE: WIND, RAIN, ARCADE, METAL; CAVE WIND, TORCH */
    "KUBLPUKU" "LKPPLKB",        /* 12 FM6: TINE EP, BELL, FM BASS, BRASS, PAD, MARIMBA, ORGAN, PLUCK; FRENCH HORN,
                                  * VIRGINAL, GLASS CHOIR, DARK STRINGS, LOFI FLUTE, PIPE ORGAN, TUBA */
    "DD",                        /* 13 SLICE: CHOP, STUTTER */
    "LPLLLL" "L",                /* 14 GURDY: HURDY GURDY, DRONE WHEEL, DANCE GURDY, TROMPETTE, VIELLE, BAGPIPE;
                                  * ESTAMPIE's TROTTO GURDY */
};
static uint32_t preset_cat(uint32_t e, uint32_t k)    /* factory preset k of engine e */
{
    static const char L[] = "?BLPUKDFO";               /* (a letter's index is its CAT_*) */
    const char *s = e < NELEM(PRESET_CAT) && PRESET_CAT[e] ? PRESET_CAT[e] : "";
    uint32_t i, c;
    for (i = 0; i < k && s[i]; i++)
        ;
    for (c = 1; s[i] && L[c] && L[c] != s[i]; c++)
        ;
    return s[i] && L[c] ? c : cat_of_engine(e);
}
#endif
#endif
