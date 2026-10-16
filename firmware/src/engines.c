/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* Engine table (order = PRESETS browsing order and the engine numbers of the editor protocol), the
 * factory patterns (SAVE > PHRASES) and the parts' sounds at power-on. */

/* The parts' engine memory (1.5): a part plays one engine at a time, so the engines with large per-part state
 * (engine_t.mem: PHYS's slots, GRAIN's grains and index, DRUM's lanes, FM6's notes, SLICE's reverse windows)
 * keep it in one region per part instead of an array each. The region holds the state of the engine that
 * last claimed it (eng_mem_owner); a switch to another such engine zeroes its engine_t.mem bytes, which is
 * the state each had at power-on, so a switch sounds as it did with the arrays (tests/union_test.c). State
 * that runs on across notes and switches stays outside (WHEEL's wheels, GRAIN's random numbers).
 * Sized by the largest, PHYS (3 x SYMP's slot); an engine that needs more than ENG_MEM_PART grows it for
 * every part (x NPART of the pool). Zero-initialised pool section: main.c fm1_cstart clears it. */
#define ENG_MEM_PART 12888u
static union {
    uint64_t align;
    uint8_t b[ENG_MEM_PART];
} eng_mem[NPART] __attribute__((section(".pool")));
static uint8_t eng_mem_owner[NPART];       /* engine index + 1 whose state is in the region, 0 = none (zeros) */
#define ENG_MEM(p) ((void *)eng_mem[p].b)  /* part p's region (p < NPART) */
#define ENG_MEM_FITS(name, size) _Static_assert((size) <= ENG_MEM_PART, name " state over ENG_MEM_PART")

#include "dsp.c"
#include "eng_analog.c"
#include "eng_phase.c"
#include "eng_lofi.c"
#include "eng_sample.c"
#include "eng_formant.c"
#include "eng_trio.c"
#include "eng_wheel.c"
#include "eng_grain.c"
#include "eng_phys.c"           /* PHYS: DaisySP physical models (phys_dsp.c, MIT) */
#include "eng_drum.c"           /* DRUM: the 8-lane kit (drum_voice.c) */
#include "eng_noise.c"
#include "eng_fm6.c"            /* FM6: 6-operator FM, msfa ported (fm6_core.c, Apache-2.0) */
#include "fm4_convert.c"        /* DIGITAL's tables, and its sounds -> FM6 */
#if FELUCCA_FM4
#include "eng_digital.c"        /* DIGITAL: four-operator FM (retired; FELUCCA_FM4=1 builds it) */
#endif
#if FELUCCA_SLICE
#include "eng_slice.c"
#endif
#include "eng_gurdy.c"          /* GURDY (GHOULBOX): the hurdy-gurdy */

/* the editor protocol, user presets and projects store these indices: append, never reorder */
static const engine_t *const ENGINES[NENGINES] = {
    &ENG_ANALOG,                 /* 0 */
#if FELUCCA_FM4
    &ENG_DIGITAL,                /* 1 (ENGI_DIGITAL) */
#else
    &ENG_FM4_GONE,               /* 1: reserved (DIGITAL, retired: its sounds convert to FM6, fm4_convert.c) */
#endif
    &ENG_PHASE,                  /* 2 */
    &ENG_LOFI,                   /* 3 */
    &ENG_SAMPLE,                 /* 4 */
    &ENG_FORMANT,                /* 5 VOICE (eng_formant.c: "voice" is a sounding note in voice.c) */
    &ENG_TRIO,                   /* 6 */
    &ENG_WHEEL,                  /* 7 */
    &ENG_GRAIN,                  /* 8 */
    &ENG_PHYS,                   /* 9 (ENGI_PHYS) */
    &ENG_DRUM,                   /* 10 (ENGI_DRUM) */
    &ENG_NOISE,                  /* 11 */
    &ENG_FM6,                    /* 12 (ENGI_FM6) */
#if FELUCCA_SLICE
    &ENG_SLICE,                  /* 13 (ENGI_SLICE) */
#else
    &ENG_FM4_GONE,               /* 13: reserved (FELUCCA_SLICE=0: no SLICE; never offered, eng_ok) */
#endif
    &ENG_GURDY,                  /* 14 (ENGI_GURDY, GHOULBOX) */
};

/* a track's engine number as an index (the audio paths: a compare, cheaper than % NENGINES; a bad number: 0) */
static inline uint32_t eng_idx(uint32_t e) { return e < NENGINES ? e : 0u; }

/* part t's engine memory goes to its engine (t->engine; voice.c engine_block, once a block: on a switch, after
 * the old engine's fade, before any note on the new one): zeroed (its engine_t.mem bytes) when another
 * engine's state is in it. An engine without memory leaves it as it is: a switch back finds its state. */
static void eng_mem_claim(const track_t *t)
{
    uint32_t p = (uint32_t)(t - trk), e, k, n;
    uint32_t *w;
    if (p >= NPART)
        return;
    e = eng_idx(t->engine);
    n = ENGINES[e]->mem;
    if (!n || eng_mem_owner[p] == e + 1u)
        return;
    w = (uint32_t *)ENG_MEM(p);
    for (k = 0; k < (n + 3u) / 4u; k++)
        w[k] = 0;
    eng_mem_owner[p] = (uint8_t)(e + 1u);
}

/* the sample source a track's sound plays that has no data (SAMPLE's SET, GRAIN's SRC, SLICE's SRC: an empty or
 * invalid user slot, a set or PIANO this build lacks): its name (it plays a sine: eng_sample.c smp_sine), 0 = none;
 * *code a small number for it (1.., per engine and source) so the UI says it once (ui_input.c sample_notice) */
static uint8_t snd_said[NTRK];   /* per track: the code last said, 0 = none (a project load clears them: said again) */
static const char *snd_missing(const track_t *t, uint32_t *code)
{
    const engine_t *e = ENGINES[eng_idx(t->eng_req)];
    uint32_t si = (uint32_t)t->p[P_E0] % SMP_NALL;
#if FELUCCA_SLICE
    if (e == &ENG_SLICE) {
        si = slc_src_of(t->p);
        *code = 64u + si;
        return slc_get(si) ? 0 : N_SLC_SRC[si];
    }
#endif
    if (e != &ENG_SAMPLE && e != &ENG_GRAIN)
        return 0;
    *code = (e == &ENG_GRAIN ? 32u : 1u) + si % 32u;
    return smp_set_missing(si) ? SMP_ALL_NAMES[si] : 0;
}

/* the order the engines are shown in (PRESETS browsing and its ENG knob, the EDIT layer's keys, the editor's list):
 * engine indices, never DIGITAL's reserved 1 (with FELUCCA_FM4 it follows FM6). The indices stay as they are (the
 * stores and the protocol hold them); only this table orders them */
static const uint8_t ENGINE_ORDER[NENG_SHOWN] = {
    0,                           /* ANALOG */
    12,                          /* FM6 */
#if FELUCCA_FM4
    1,                           /* DIGITAL */
#endif
    2, 3, 4, 5, 6, 7, 8, 9,      /* PHASE LOFI SAMPLE VOICE TRIO WHEEL GRAIN PHYS (SAMPLE: GHOULBOX 1.1's CC0 sets) */
    14,                          /* GURDY (GHOULBOX) */
    11,                          /* NOISE */
    10,                          /* DRUM (GHOULBOX 1.2: its CRYPT kit) */
};                               /* GHOULBOX: SLICE not offered (every preset retired, GB_HIDDEN); it still plays
                                  * what a project or a user preset holds (eng_ok) */

/* GHOULBOX: factory presets the owner retired. Kept in their engines (a stored preset number - a project, a user
 * preset, an old format - still loads its sound) but not browsed: PRESETS, the knob, favourites, the editor
 * (ui.c preset_hidden). By name; every preset of that name in that engine. ui_test checks each name exists */
static const struct { uint8_t e; const char *name; } GB_HIDDEN[] = {
    {0, "SAW LEAD"}, {0, "SQR BASS"}, {0, "ACID"}, {0, "SINE KEY"}, {0, "RAVE"}, {0, "PLUCK"}, {0, "BRASS"},
    {2, "BRASS"}, {2, "ORGAN"}, {2, "RESO"}, {2, "BELL"}, {2, "WIRE"},
    {3, "PULSE LD"}, {3, "WAVE BASS"}, {3, "ARP 8BIT"}, {3, "WAVE LEAD"}, {3, "STEP LEAD"},
    {5, "VOX LEAD"}, {5, "WOW BASS"}, {5, "WHISPER"},
    {6, "FAT BASS"}, {6, "ARP LEAD"}, {6, "SYNC LEAD"}, {6, "RING BELL"},
    {7, "JAZZ PERC"}, {7, "GOSPEL"}, {7, "ROCK DRIVE"},
    {8, "GLITCH"}, {8, "SHIMMER"},
    {9, "BELL TREE"}, {9, "MARIMBA"}, {9, "PLUCK"}, {9, "BOWED METAL"}, {9, "KALIMBA"}, {9, "HAND DRUM"}, {9, "TOMS"},
    {9, "DRONE STRING"}, {9, "HARP"},
    {10, "DRUM KIT"},
    {11, "WIND"}, {11, "RAIN"}, {11, "ARCADE"}, {11, "METAL"},
    {12, "TINE EP"}, {12, "FM BASS"}, {12, "BRASS"}, {12, "MARIMBA"}, {12, "ORGAN"}, {12, "PLUCK"},
#if FELUCCA_SLICE
    {13, "CHOP"}, {13, "STUTTER"},
#endif
};

/* the engines one can pick (engine 1 only with FELUCCA_FM4), in ENGINE_ORDER: eng_ok(e), the n-th of them
 * eng_vis(n), e's place among them eng_rank(e), the next / previous one eng_step(e, dir) (wraps) */
static int eng_ok(uint32_t e) { return e < NENGINES && (FELUCCA_FM4 || e != ENGI_DIGITAL) && (FELUCCA_SLICE || e != ENGI_SLICE); }
static uint32_t eng_vis(uint32_t n) { return ENGINE_ORDER[n % NENG_SHOWN]; }
static uint32_t eng_rank(uint32_t e)
{
    uint32_t n;
    if (!eng_ok(e))
        e = ENGI_FM6;                            /* (DIGITAL without FELUCCA_FM4: its sounds play as FM6) */
    for (n = 0; n < NENG_SHOWN && ENGINE_ORDER[n] != e; n++)
        ;
    return n < NENG_SHOWN ? n : 0u;
}
static uint32_t eng_step(uint32_t e, int32_t dir)
{
    return eng_vis((eng_rank(e % NENGINES) + (dir > 0 ? 1u : NENG_SHOWN - 1u)) % NENG_SHOWN);
}

/* factory sequence patterns: SAVE > PHRASES loads one into the selected track (ui.c pat_load); a preset
 * only suggests one with PAT(n), loading a sound never touches the steps. Absolute notes, loaded as they
 * are (DRUM plays them as GM drums, SLICE as slices; SCL TRANS and OCT transpose): 0 = rest;
 * flags 1 = accent, 2 = slide, 4 = tie (holds the previous note). Names: at most 8 characters */
#define T_ 4
static const struct {
    const char *name;
    uint8_t note[16], flags[16];
} PATTERNS[] = {
    {"ACID", {45, 45, 57, 45, 0, 48, 45, 55, 45, 0, 57, 52, 45, 48, 0, 50},          /* 1 */
     {1, 0, 2, 0, 0, 0, 1, 2, 0, 0, 1, 0, 0, 2, 0, 1}},
    {"OFFBEAT", {0, 36, 0, 36, 0, 36, 0, 48, 0, 36, 0, 36, 0, 39, 0, 43},          /* 2 bass */
     {0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}},
    {"MELODY", {60, 0, 67, 0, 72, 67, 0, 64, 62, 0, 69, 0, 74, 69, 0, 67},         /* 3 pluck */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"LEAD", {72, 0, 0, 74, 0, 0, 76, 0, 79, 0, 76, 0, 74, 0, 0, 0},               /* 4 */
     {1, T_, 0, 2, T_, 0, 0, 0, 1, 0, 2, 0, 0, T_, T_, 0}},
    {"PAD", {60, 0, 0, 0, 0, 0, 0, 0, 57, 0, 0, 0, 55, 0, 0, 0},                   /* 5 long notes */
     {0, T_, T_, T_, T_, T_, T_, 0, 0, T_, T_, 0, 0, T_, T_, 0}},
    {"KEYS", {0, 0, 60, 0, 0, 63, 0, 0, 0, 0, 60, 0, 0, 65, 0, 63},                /* 6 offbeat stabs */
     {0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0}},
    {"BELL", {72, 0, 0, 79, 0, 0, 84, 0, 0, 0, 76, 0, 0, 0, 0, 0},                 /* 7 sparse */
     {1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0}},
    {"SUB", {36, 0, 0, 0, 0, 0, 0, 36, 0, 0, 34, 0, 0, 0, 0, 0},                   /* 8 low, held */
     {1, T_, T_, T_, 0, 0, 0, 0, 0, 0, 0, T_, T_, T_, 0, 0}},
    /* SLICE (eng_slice.c): note = C4 + slice */
    {"CHOP", {60, 61, 62, 67, 64, 65, 60, 69, 68, 70, 62, 67, 72, 72, 74, 64},     /* 9 16 slices re-ordered */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"STUTTER", {60, 60, 61, 61, 62, 0, 63, 63, 64, 65, 65, 0, 66, 66, 66, 67},    /* 10 8 slices, repeats */
     {1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}},
    {"SLICES", {60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75},   /* 11 in order */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* DRUM (General MIDI: 36 kick, 38 snare, 42 closed / 46 open hi-hat) */
    {"BEAT", {36, 42, 42, 42, 38, 42, 36, 42, 36, 42, 42, 36, 38, 42, 46, 42},     /* 12 */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    /* 16ths up a C minor arpeggio, twice: the line the ARP presets (RAVE, ARP 8BIT, ARP LEAD) suggest
     * now that the arpeggiator is the track's and a preset no longer switches it on */
    {"ARP", {48, 51, 55, 60, 63, 67, 72, 75, 48, 51, 55, 60, 63, 67, 72, 75},       /* 13 */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
    /* GHOULBOX: the power-on scene in D minor (TRK_DEF; played in 1/8 steps). Chords D, Bb, C, A, a bar each */
    {"DIRGE", {62, 0, 0, 0, 58, 0, 0, 0, 60, 0, 0, 0, 57, 0, 0, 0},                 /* 14 the pad's roots */
     {0, T_, T_, T_, 0, T_, T_, T_, 0, T_, T_, T_, 0, T_, T_, T_}},
    {"BALLAD", {62, 0, 65, 69, 67, 0, 65, 64, 62, 0, 0, 0, 57, 58, 60, 0},          /* 15 the melody */
     {1, T_, 0, 0, 1, T_, 0, 2, 1, T_, T_, 0, 0, 0, 2, T_}},
    {"CHANT", {50, 0, 0, 0, 0, 0, 53, 0, 48, 0, 0, 0, 45, 0, 0, 0},                 /* 16 low voices */
     {1, T_, T_, 0, 0, 0, 1, T_, 0, T_, T_, 0, 0, T_, T_, T_}},
    {"PEDAL", {38, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},                    /* 17 one D, held */
     {1, T_, T_, T_, T_, T_, T_, T_, T_, T_, T_, T_, T_, T_, T_, T_}},
    /* GHOULBOX 1.2: more in D minor over the scene's chords (Dm Bb C A), appended (18..21) */
    {"MARCH", {38, 0, 45, 0, 34, 0, 41, 0, 36, 0, 43, 0, 33, 0, 40, 0},             /* 18 the bass's march: root, fifth, a chord a bar */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"LAMENT", {69, 0, 67, 65, 64, 0, 62, 0, 65, 64, 62, 61, 62, 0, 0, 0},          /* 19 a falling line, A down to D */
     {1, T_, 0, 0, 0, T_, 0, T_, 0, 0, 0, 0, 1, T_, T_, T_}},
    {"ARPEGGIO", {50, 53, 57, 62, 46, 50, 53, 58, 48, 52, 55, 60, 45, 49, 52, 57},  /* 20 each chord broken upward (harp, lute) */
     {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}},
    {"ANTIPHON", {62, 0, 65, 64, 62, 0, 0, 0, 50, 0, 53, 52, 50, 0, 0, 0},          /* 21 a call, answered an octave down */
     {1, T_, 0, 0, 0, T_, T_, 0, 1, T_, 0, 0, 0, T_, T_, 0}},
    /* GHOULBOX 1.2: for the DRUM engine's CRYPT kit (GM notes: its FRAME, SHAKR, TIMP, ROPE, SLAP lanes) */
    {"TOMBBEAT", {36, 42, 0, 42, 45, 42, 36, 42, 36, 42, 45, 45, 38, 42, 39, 42},       /* 22 a procession */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
    /* ESTAMPIE: for the DRUM engine's DANCE kit (GM notes: its TABOR, TAMB, BUZZ, NAKER, HANDS, JINGL, BONES lanes) */
    {"TABOR", {36, 0, 42, 36, 38, 42, 36, 0, 36, 42, 45, 45, 38, 39, 46, 37},           /* 23 a dance: the drum's long-short */
     {1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}},
};
#undef T_
#define NPATTERNS (sizeof PATTERNS / sizeof PATTERNS[0])

/* the parts at power-on (engine, preset, PATTERNS[n - 1] in the sequencer, 0 = empty: all are): bass, pad, lead, drums */
static const uint8_t TRK_DEF[NPART][3] = {{0, 4, 0}, {ENGI_FM6, 4, 0}, {3, 0, 0}, {ENGI_DRUM, 0, 0}}; /* ANALOG ACID,
                                                                       * FM6 PAD (was DIGITAL PAD), LOFI PULSE LD, DRUM KIT */
/* ESTAMPIE: the scenes' own phrases, longer than PATTERNS' 16 steps (a dance's whole punctum), each with its length and
 * division. A melodic phrase: absolute notes, 0 = rest, ES_T = hold the previous note; a DRUM phrase: each step's lane
 * hits (bit l: lane l, eng_drum.c DRUM_LANE_NOTE). acc: bit i accents step i. Loaded by es_phrase_load. */
#define ES_T 255u
#define ES_PHR(n) (0x80u + (n))                  /* a scene part's pattern: ES_PHRASES[n] (below 0x80: PATTERNS[n - 1]) */
typedef struct {
    const char *name;
    uint8_t len, div, drum;                      /* steps, N_DIV index (4 = 8T: 6/8 as three steps a beat), lane hits */
    uint64_t acc;
    uint8_t step[NSTEP];
} es_phrase_t;
#define ES_BAR6(n) (1ull << (6u * (n)))           /* the first step of 6/8 bar n */
static const es_phrase_t ES_PHRASES[] = {
    /* 0..3 TROTTO: "Trotto", London, British Library Add. MS 29987 f. 63v (Tuscany, c. 1390): its first punctum with
     * the chiuso (closed) ending, A Dorian, 6/8, one 8T step an eighth note: 6 bars, the pickup (A2A) last. The
     * chiuso's ornament B/A/B (sixteenths) is played as a plain B, the grid's finest note being an eighth. */
    {"TROTTO", 36, 4, 0, ES_BAR6(0) | ES_BAR6(1) | ES_BAR6(2) | ES_BAR6(3) | ES_BAR6(4) | ES_BAR6(5),  /* the melody, */
     {64, 57, 64, 64, ES_T, 62,   60, 59, 60, 57, 55, 57,   59, 57, 59, 55, ES_T, ES_T,             /* an octave down */
      57, ES_T, 59, 62, ES_T, 64, 62, ES_T, 60, 59, ES_T, 55,   57, ES_T, ES_T, 57, ES_T, 57}},     /* (the drones above) */
    {"TROT LUTE", 36, 4, 0, ES_BAR6(0) | ES_BAR6(1) | ES_BAR6(2) | ES_BAR6(3) | ES_BAR6(4) | ES_BAR6(5),
     {57, ES_T, 64, 57, ES_T, 64,   57, ES_T, 64, 57, ES_T, 64,   55, ES_T, 62, 55, ES_T, 62,      /* root and fifth, long */
      50, ES_T, 57, 50, ES_T, 57,   55, ES_T, 62, 55, ES_T, 62,   57, ES_T, 64, 57, ES_T, 64}},    /* short: A A G D G A */
    {"TROT DESC", 36, 4, 0, ES_BAR6(0) | ES_BAR6(3),                                              /* the tune's skeleton, */
     {76, ES_T, ES_T, 76, ES_T, ES_T,   72, ES_T, ES_T, 69, ES_T, ES_T,   71, ES_T, ES_T, 67, ES_T, ES_T,   /* held: */
      69, ES_T, ES_T, 74, ES_T, ES_T,   74, ES_T, ES_T, 71, ES_T, ES_T,   69, ES_T, ES_T, ES_T, ES_T, 0}},  /* heterophony */
    {"TROT TABOR", 12, 4, 1, ES_BAR6(0) | ES_BAR6(1),   /* (the DANCE kit) the tabor on the beats, its snare string */
     {1, 0, 2, 1 | 16, 0, 2,   1, 8, 2, 1 | 16, 32, 2}},  /* between, the tambourine; the nakers answer at the end */
};
#define ES_NPHRASE (sizeof ES_PHRASES / sizeof ES_PHRASES[0])

/* ESTAMPIE: phrase n into track t's steps, with its length and division (ui.c gb_scene_load; tests/estampie_demo.c) */
static void es_phrase_load(track_t *t, uint32_t n)
{
    const es_phrase_t *ph = &ES_PHRASES[n % ES_NPHRASE];
    uint32_t i, prev = 0;
    for (i = 0; i < NSTEP; i++) {
        step_t *s = &t->step[i];
        uint32_t v = i < ph->len ? ph->step[i] : 0u, acc = (uint32_t)((ph->acc >> i) & 1u);
        memset(s, 0, sizeof *s);
        if (ph->drum) {                                    /* lane hits, as the DRUM grid holds them */
            s->hit = (uint8_t)v;
            s->acc = acc ? (uint8_t)v : 0u;
            s->time = v ? ST_NOTE : ST_REST;
            s->vel = v ? 96 : 0;
        } else if (v == ES_T && prev) {
            s->note[0] = (uint8_t)prev;
            s->n = 1;
            s->time = ST_TIE;
        } else {
            s->note[0] = (uint8_t)(v == ES_T ? 0u : v);
            s->n = s->note[0] ? 1 : 0;
            s->time = s->n ? ST_NOTE : ST_REST;
            s->flags = s->n && acc ? SF_ACCENT : 0u;
            s->vel = s->n ? 96 : 0;
            prev = s->note[0];
        }
    }
    t->p[P_SLEN] = ph->len;
    t->p[P_SDIV] = ph->div;
}

/* GHOULBOX: what felucca_init plays instead (TRK_DEF stays Felucca's: an old project's missing parts are those)
 * (ui.c gb_scene_load). 1.3: MENU > SYSTEM > SCENE picks one (ui_scene; append-only: a saved value keeps its scene, an
 * unknown one plays the first). PATTERNS parts play in 1/8 steps; ES_PHR parts at their own length and division.
 * ESTAMPIE: a dance first: TROTTO takes id 0, a fresh device's (and a GHOULBOX DUNGEON's: its sounds are still here,
 * moved to the end); CRYPT, TAVERN, CHAPEL and BLANK keep their ids. rtype: G_RTYPE (ROOM SPRING HALL); lvl: each
 * part's LVL (P_LEVEL), 0 = the sound's own */
#define GB_NSCENE 6u
#define GB_DUNGEON 5u
static const struct { const char *name; uint8_t bpm, tape, rtype, part[NPART][3], lvl[NPART]; } GB_SCENES[GB_NSCENE] = {
    {"TROTTO", 112, 12, 0, {{ENGI_GURDY, 6, ES_PHR(0)}, {ENGI_PHYS, 9, ES_PHR(1)}, {ENGI_SAMPLE, 7, ES_PHR(2)},
                           {ENGI_DRUM, 3, ES_PHR(3)}},   /* ESTAMPIE: TROTTO GURDY, LUTE, TENOR RECORD, DANCE KIT; */
     {0, 118, 86, 94}},                                  /* the lute up, the recorder and the drums under the melody */
    {"CRYPT", 60, 55, 2, {{5, 7, 17}, {5, 5, 14}, {11, 4, 17}, {ENGI_DRUM, 2, 22}}},           /* LOW DRONE PEDAL,
                                    * CRYPT CHOIR DIRGE, CAVE WIND PEDAL, TOMB DRUMS TOMBBEAT */
    {"TAVERN", 96, 30, 2, {{ENGI_GURDY, 2, 15}, {ENGI_PHYS, 11, 20}, {ENGI_PHYS, 9, 18}, {ENGI_DRUM, 1, 22}}},   /* DANCE
                                    * GURDY BALLAD, DUNGEON HARP ARPEGGIO, LUTE MARCH, CRYPT KIT TOMBBEAT */
    {"CHAPEL", 66, 40, 2, {{7, 6, 14}, {5, 4, 16}, {ENGI_SAMPLE, 7, 19}, {ENGI_GURDY, 4, 21}}},  /* CATHEDRAL DIRGE,
                                    * MONKS CHANT, TENOR RECORD LAMENT, VIELLE ANTIPHON */
    {"BLANK", 72, 40, 2, {{ENGI_GURDY, 0, 0}, {0, 12, 0}, {5, 4, 0}, {ENGI_GURDY, 1, 0}}},      /* DUNGEON's sounds, no
                                    * notes: a clean start */
    {"DUNGEON", 72, 40, 2, {{ENGI_GURDY, 0, 15}, {0, 12, 14}, {5, 4, 16}, {ENGI_GURDY, 1, 17}}},  /* HURDY GURDY BALLAD,
                                    * CRYPT PAD DIRGE, MONKS CHANT, DRONE WHEEL PEDAL: GHOULBOX's scene up to 1.2 */
};
#define GB_SCENE (GB_SCENES[GB_DUNGEON].part)   /* (GHOULBOX's scene up to 1.2: tests/descdump.c, tests/ghoulbox_shots.c) */
static uint32_t trk_def_engine(uint32_t i) { return TRK_DEF[i % NPART][0]; }
