/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE sound lab: candidate sounds, tried in the TROTTO before any becomes a preset. Each candidate takes one of
 * the scene's four parts (the tune, the lute, the recorder's long notes, the tabor) with its own engine values,
 * envelope and sends, and is rendered twice through the whole mix (hostsim.c, as the device): alone, and with the
 * band. The scene's own sounds are rendered the same way, to compare.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/sound_lab tests/sound_lab.c -lm
 *   build/host/sound_lab OUTDIR [SECONDS]   -> OUTDIR/<id>_solo.wav, OUTDIR/<id>_band.wav, and an index on stdout
 * Not a test: an ear check while ESTAMPIE's sounds are made (ROADMAP.md, the sounds). */
#define main hostsim_main
#include "hostsim.c"
#undef main

/* the engines without a name in core.h (engines.c ENGINES[]) */
enum { LAB_ANALOG = 0, LAB_VOICE = 5, LAB_TRIO = 6, LAB_WHEEL = 7 };
enum { PART_TUNE, PART_LUTE, PART_LONG, PART_DRUM };
enum { PAT_SCENE, PAT_TABOR, PAT_FRAME, PAT_BELLS };   /* the part's own phrase, or a hand-drum line on notes */
typedef struct {
    const char *id, *group, *name, *idea;
    uint8_t eng;
    int8_t e[8];
    uint8_t env[4];
    int8_t fenv;
    uint8_t mono, part, pat;
    uint8_t fx[4];               /* DIST CHO DLY REV sends */
    uint8_t temp, ring;          /* TEMP (0 EQUAL, 1 PYTH ..), RING */
} cand_t;

#define KEEP 0xFFu               /* eng: the scene's own sound in that part */
static const cand_t CANDS[] = {
    /* the scene as it is, to compare */
    {"ref_tune", "As now", "TROTTO GURDY", "The scene's tune: GURDY, COUP 1/4.", ENGI_GURDY,
     {9, 70, 55, 110, 100, 1, 55, 15}, {2, 64, 127, 20}, 0, 1, PART_TUNE, PAT_SCENE, {8, 0, 0, 40}, 0, 0},
    {"ref_lute", "As now", "LUTE", "The scene's lute: PHYS STRNG.", ENGI_PHYS,
     {1, 48, 55, 70, 30, 127, 0, 0}, {0, 100, 127, 70}, 0, 0, PART_LUTE, PAT_SCENE, {0, 15, 20, 80}, 0, 0},
    {"ref_long", "As now", "TENOR RECORD", "The scene's long notes: SAMPLE, the CC0 tenor recorder.", KEEP,
     {0}, {0}, 0, 0, PART_LONG, PAT_SCENE, {0}, 0, 0},
    {"ref_drum", "As now", "DANCE KIT", "The scene's tabor: DRUM, ESTAMPIE's DANCE kit.", KEEP,
     {0}, {0}, 0, 0, PART_DRUM, PAT_SCENE, {0}, 0, 0},
    /* plucked strings (the lute's part) */
    {"citole", "Plucked strings", "CITOLE", "A small bright quill-plucked lute: STRNG near the bridge, a little bridge buzz.",
     ENGI_PHYS, {1, 26, 104, 62, 12, 127, 0, 34}, {0, 100, 127, 55}, 0, 0, PART_LUTE, PAT_SCENE, {0, 10, 15, 60}, 0, 0},
    {"gittern", "Plucked strings", "GITTERN", "The lute's small, rounder cousin: STRNG, softer pick, plucked mid-string.",
     ENGI_PHYS, {1, 40, 78, 66, 36, 120, 0, 16}, {0, 100, 127, 60}, 0, 0, PART_LUTE, PAT_SCENE, {0, 15, 15, 65}, 0, 0},
    {"psaltery", "Plucked strings", "PSALTERY", "Plucked psaltery: SYMP strings an octave up ringing along, long and bright.",
     ENGI_PHYS, {3, 4, 100, 96, 70, 120, 0, 20}, {0, 100, 127, 95}, 0, 0, PART_LUTE, PAT_SCENE, {0, 20, 15, 75}, 1, 0},
    {"brayharp", "Plucked strings", "BRAY HARP", "A gothic harp with bray pins: SYMP fifths, the pins' buzz (BUZZ).",
     ENGI_PHYS, {3, 20, 82, 86, 50, 120, 72, 10}, {0, 100, 127, 90}, 0, 0, PART_LUTE, PAT_SCENE, {0, 15, 15, 75}, 1, 0},
    {"kora", "Beyond", "KORA", "Kora: SYMP with a major chord ringing, a harp's attack and a light rattle.",
     ENGI_PHYS, {3, 56, 92, 80, 76, 127, 36, 28}, {0, 100, 127, 85}, 0, 0, PART_LUTE, PAT_SCENE, {0, 10, 25, 60}, 0, 0},
    /* reeds and winds (the tune) */
    {"crumhorn", "Reeds and winds", "CRUMHORN", "A capped reed, cylindrical bore: TRIO squares in unison, a nasal resonance.",
     LAB_TRIO, {1, 0, 0, 3, 0, 58, 62, 64}, {3, 70, 120, 20}, 0, 1, PART_TUNE, PAT_SCENE, {6, 0, 0, 45}, 0, 0},
    {"rausch", "Reeds and winds", "RAUSCHPFEIFE", "The loud capped shawm: TRIO saws an octave and a twelfth up, wide open.",
     LAB_TRIO, {0, 12, 19, 5, 0, 104, 22, 64}, {2, 70, 120, 18}, 0, 1, PART_TUNE, PAT_SCENE, {10, 0, 0, 40}, 0, 0},
    {"shawm2", "Reeds and winds", "BRIGHT SHAWM", "The SHAWM preset brighter and more biting, for outdoors.",
     LAB_TRIO, {1, 12, 19, 4, 1, 104, 78, 20}, {4, 70, 118, 35}, 0, 1, PART_TUNE, PAT_SCENE, {12, 0, 0, 40}, 0, 0},
    {"trumpet", "Reeds and winds", "HERALD TRUMPET", "A natural trumpet: ANALOG saw, the filter opening on each note.",
     LAB_ANALOG, {0, 6, 0, 0, 52, 28, 46, 70}, {6, 80, 110, 30}, 46, 1, PART_TUNE, PAT_SCENE, {8, 0, 10, 55}, 0, 0},
    {"pipes", "Beyond", "UILLEANN", "Uilleann pipes: GURDY without buzz, a sweeter chanter and a quiet drone, RING on.",
     ENGI_GURDY, {9, 58, 30, 78, 0, 0, 96, 12}, {6, 64, 127, 30}, 0, 1, PART_TUNE, PAT_SCENE, {0, 12, 15, 60}, 0, 1},
    {"chant", "Organ and chant", "CANTOR", "A single voice singing the tune: VOICE on 'o', a breath, a little wander.",
     LAB_VOICE, {104, 110, 10, -3, 70, 26, 66, 12}, {28, 90, 118, 60}, 0, 1, PART_TUNE, PAT_SCENE, {0, 10, 10, 85}, 1, 0},
    /* organ (the long notes) */
    {"portative", "Organ and chant", "PORTATIVE", "A small organ on the player's knee: WHEEL flutes, top up, a chiff.",
     LAB_WHEEL, {0, -4, 0, 4, 0, 70, 6, 0}, {4, 64, 127, 30}, 0, 0, PART_LONG, PAT_SCENE, {0, 10, 0, 60}, 1, 0},
    {"regal", "Organ and chant", "REGAL", "A reed organ: WHEEL's REED registration, nasal and buzzy.",
     LAB_WHEEL, {10, -3, 2, 2, 0, 40, 30, 0}, {3, 64, 127, 30}, 0, 0, PART_LONG, PAT_SCENE, {4, 0, 0, 55}, 1, 0},
    /* percussion (the recorder's part, under the tabor) */
    {"nakers", "Percussion", "NAKERS", "Small kettledrums struck with sticks: MEMB, a loaded head, short.",
     ENGI_PHYS, {2, 92, 100, 54, 64, 127, 8, 60}, {0, 100, 127, 40}, 0, 0, PART_LONG, PAT_TABOR, {0, 0, 0, 30}, 0, 0},
    {"frame", "Percussion", "FRAME DRUM", "A deep frame drum: MEMB struck near the centre, its pitch falling.",
     ENGI_PHYS, {2, 24, 44, 70, 18, 110, 26, 14}, {0, 100, 127, 70}, 0, 0, PART_LONG, PAT_FRAME, {0, 0, 0, 40}, 0, 0},
    {"bells", "Percussion", "HAND BELLS", "Hand bells on the downbeats: MODAL, inharmonic, ringing on.",
     ENGI_PHYS, {0, 92, 108, 110, 10, 127, 0, 20}, {0, 100, 127, 100}, 0, 0, PART_LONG, PAT_BELLS, {0, 10, 20, 70}, 0, 0},
};
#define NCAND (sizeof CANDS / sizeof CANDS[0])

/* hand-drum lines in 8T, 12 steps (two bars of 6/8): notes, 0 rest */
static const uint8_t LINES[4][12] = {
    [PAT_TABOR] = {57, 0, 64, 57, 0, 64, 57, 64, 64, 57, 0, 69},   /* the tabor's long-short, answered higher */
    [PAT_FRAME] = {41, 0, 0, 41, 0, 48, 41, 0, 0, 41, 48, 48},
    [PAT_BELLS] = {81, 0, 0, 0, 0, 0, 76, 0, 0, 0, 0, 0},
};

static void setup(const cand_t *c, int solo)
{
    uint32_t i, t;
    const uint32_t s = 0;                              /* TROTTO */
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = GB_SCENES[s].bpm;
    song.g[G_RTYPE] = GB_SCENES[s].rtype;
    song.g[G_TAPE] = GB_SCENES[s].tape;
    rev_clear();
    hall_clear();
    fx.rtype = GB_SCENES[s].rtype;
    for (i = 0; i < NPART; i++) {
        const uint8_t *p = GB_SCENES[s].part[i];
        host_preset(&trk[i], p[0], p[1]);
        trk[i].p[P_SDIV] = 1;
        es_phrase_load(&trk[i], p[2] - ES_PHR(0));
        if (GB_SCENES[s].lvl[i])
            trk[i].p[P_LEVEL] = GB_SCENES[s].lvl[i];
    }
    if (c->eng != KEEP) {   /* the candidate in its part, as a preset load (host_preset_values) */
        track_t *tr = &trk[c->part];
        preset_t p = {c->name, {0}, {0}, c->fenv, c->mono, {0}, 0};
        memcpy(p.e, c->e, 8);
        memcpy(p.env, c->env, 4);
        for (i = 0; i < 4u; i++)
            p.fx[i] = (uint8_t)(c->fx[i] + 1u);
        host_preset_values(tr, c->eng, 0, &p);
        tr->engine = tr->eng_req;
        eng_mem_claim(tr);
        tr->p[P_TEMP] = c->temp;
        tr->p[P_RING] = c->ring;
        if (c->pat != PAT_SCENE) {                     /* a hand-drum line instead of the part's phrase */
            for (t = 0; t < NSTEP; t++)
                memset(&tr->step[t], 0, sizeof tr->step[t]);
            for (t = 0; t < 12u; t++) {
                uint8_t n = LINES[c->pat][t];
                put_step(tr, t, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, 0);
                tr->step[t].vel = n ? (t % 6u == 0 ? 120 : 90) : 0;
            }
            tr->p[P_SLEN] = 12;
            tr->p[P_SDIV] = 4;                         /* 8T, as the scene */
        }
    }
    if (solo)                                          /* the other parts' steps emptied (their sounds stay) */
        for (i = 0; i < NPART; i++)
            if (i != c->part)
                for (t = 0; t < NSTEP; t++)
                    memset(&trk[i].step[t], 0, sizeof trk[i].step[t]);
}

static void render(const char *dir, const cand_t *c, int solo, uint32_t seconds)
{
    char path[512];
    FILE *f;
    uint32_t t, i, frames = (seconds + 3u) * FS;
    int32_t o[2 * CTL];
    setup(c, solo);
    snprintf(path, sizeof path, "%s/%s_%s.wav", dir, c->id, solo ? "solo" : "band");
    if (!(f = fopen(path, "wb"))) {
        perror(path);
        exit(1);
    }
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t == seconds * FS)
            transport_req = 2;                         /* stop, then the tail */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
}

int main(int argc, char **argv)
{
    uint32_t k, seconds = argc > 2 ? (uint32_t)atoi(argv[2]) : 13u;
    if (argc < 2) {
        fprintf(stderr, "usage: sound_lab OUTDIR [SECONDS]\n");
        return 2;
    }
    for (k = 0; k < NCAND; k++) {
        render(argv[1], &CANDS[k], 1, seconds);
        render(argv[1], &CANDS[k], 0, seconds);
        printf("%s\t%s\t%s\t%s\t%s\t%u\t%d %d %d %d %d %d %d %d\t%u %u %u %u\t%u\n", CANDS[k].id, CANDS[k].group, CANDS[k].name,
               CANDS[k].eng == KEEP ? ENGINES[trk[CANDS[k].part].engine]->name : ENGINES[CANDS[k].eng]->name,
               CANDS[k].idea, CANDS[k].part, CANDS[k].e[0], CANDS[k].e[1], CANDS[k].e[2], CANDS[k].e[3], CANDS[k].e[4],
               CANDS[k].e[5], CANDS[k].e[6], CANDS[k].e[7], CANDS[k].env[0], CANDS[k].env[1], CANDS[k].env[2],
               CANDS[k].env[3], CANDS[k].eng == KEEP);
    }
    return 0;
}
