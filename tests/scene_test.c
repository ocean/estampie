/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE: the power-on scenes (engines.c GB_SCENES) and their phrases (ES_PHRASES).
 *   - every scene's sounds exist (engine, preset), its patterns or phrases exist, its levels and reverb are in range;
 *   - every phrase fits the sequencer (LEN 1..NSTEP), a 6/8 phrase in 8T fills whole bars, its notes are notes, a
 *     tie follows a note, a DRUM phrase hits only the kit's lanes;
 *   - the TROTTO melody is the transcription: parsed here from its ABC (the source's own notation), an octave down;
 *   - es_phrase_load writes those steps, its LEN and DIV, and the ties as the sequencer reads them;
 *   - TROTTO comes first (a fresh device's scene) and DUNGEON, GHOULBOX's, is still there.
 * Fails (exit 1) on the first difference it reports. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("scene: %-92s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok)
        bad = 1;
}

/* ABC (the subset the transcription uses: notes A-G a-g, a length digit, bars) into eighth-note steps: each note,
 * then ES_T for its further eighths. L:1/8; the key's F is F sharp (K:G, the tune's A Dorian) */
static uint32_t abc_steps(const char *abc, uint8_t *out, uint32_t max)
{
    static const int8_t SEMI[7] = {9, 11, 0, 2, 4, 5, 7};     /* A B C D E F G above C */
    uint32_t n = 0;
    for (; *abc; abc++) {
        int c = *abc, len = 1, oct = 4, midi;
        if (c >= 'a' && c <= 'g') {
            oct = 5;
            c -= 'a' - 'A';
        } else if (c < 'A' || c > 'G') {
            continue;                                         /* bars, spaces */
        }
        midi = 12 * (oct + 1) + SEMI[c - 'A'] + (c == 'F');   /* (C4 = 60) */
        if (abc[1] >= '2' && abc[1] <= '9')
            len = *++abc - '0';
        for (int k = 0; k < len && n < max; k++)
            out[n++] = k ? (uint8_t)ES_T : (uint8_t)midi;
    }
    return n;
}

int main(void)
{
    /* Trotto, BL Add. MS 29987: the first punctum, chiuso, from its first bar, the pickup last; "B2G" is the source's
     * "B/A/BG" with its sixteenth-note turn played as one B (the 8T grid's finest note is an eighth) */
    static const char TROTTO_ABC[] = "eAe e2d|cBc AGA|BAB G3|A2B d2e|d2c B2G|A3 A2A";
    uint8_t want[NSTEP];
    uint32_t s, i, k, n, ok;
    char what[128];

    check("TROTTO is scene 0, a fresh device's; GHOULBOX's DUNGEON is kept (GB_DUNGEON) with its sounds",
          !strcmp(GB_SCENES[0].name, "TROTTO") && !strcmp(GB_SCENES[GB_DUNGEON].name, "DUNGEON") &&
          GB_SCENE == GB_SCENES[GB_DUNGEON].part && GB_SCENES[GB_DUNGEON].part[0][0] == ENGI_GURDY);
    check("CRYPT, TAVERN, CHAPEL, BLANK keep GHOULBOX's ids 1..4 (a saved SCENE plays what it did)",
          !strcmp(GB_SCENES[1].name, "CRYPT") && !strcmp(GB_SCENES[2].name, "TAVERN") &&
          !strcmp(GB_SCENES[3].name, "CHAPEL") && !strcmp(GB_SCENES[4].name, "BLANK"));

    for (s = 0; s < GB_NSCENE; s++) {
        ok = GB_SCENES[s].bpm >= 30 && GB_SCENES[s].bpm <= 240 && GB_SCENES[s].rtype < 3u && GB_SCENES[s].tape <= 127;
        for (i = 0; i < NPART; i++) {
            const uint8_t *p = GB_SCENES[s].part[i];
            ok &= eng_ok(p[0]) && p[1] < ENGINES[p[0]]->npresets;
            ok &= p[2] >= ES_PHR(0) ? p[2] - ES_PHR(0) < ES_NPHRASE : p[2] <= NPATTERNS;
            ok &= GB_SCENES[s].lvl[i] <= 127;
            if (p[2] >= ES_PHR(0) && p[2] - ES_PHR(0) < ES_NPHRASE)   /* a drum phrase on a DRUM part only */
                ok &= !ES_PHRASES[p[2] - ES_PHR(0)].drum || p[0] == ENGI_DRUM;
        }
        snprintf(what, sizeof what, "scene %s: its sounds, patterns or phrases, levels, tempo and reverb", GB_SCENES[s].name);
        check(what, ok);
    }

    for (k = 0; k < ES_NPHRASE; k++) {
        const es_phrase_t *ph = &ES_PHRASES[k];
        ok = ph->len >= 1 && ph->len <= NSTEP && ph->div < 10u && (ph->div != 4u || ph->len % 6u == 0);
        ok &= (ph->acc >> ph->len) == 0;                       /* no accent past the end */
        for (i = 0; i < ph->len; i++) {
            uint32_t v = ph->step[i];
            if (ph->drum)
                ok &= v < 256u;                                /* (eight lanes: any bit pattern) */
            else
                ok &= v == 0 || v == ES_T ? (v != ES_T || i > 0) : v >= 24u && v <= 108u;
        }
        for (i = ph->len; i < NSTEP; i++)
            ok &= ph->step[i] == 0;
        snprintf(what, sizeof what, "phrase %s: %u steps (%s), notes in range, ties after notes, whole 6/8 bars in 8T",
                 ph->name, ph->len, N_DIV[ph->div]);
        check(what, ok);
    }

    n = abc_steps(TROTTO_ABC, want, NSTEP);
    ok = n == ES_PHRASES[0].len;
    for (i = 0; i < n; i++)
        ok &= ES_PHRASES[0].step[i] == (want[i] == ES_T ? ES_T : want[i] - 12u);
    check("TROTTO's melody is the transcription's (its ABC parsed here), an octave down, 36 eighths = 6 bars", ok);
    check("TROTTO GURDY drones on A (DRN), its fifth E sounding, its trompette struck a beat (COUP 1/4)",
          !strcmp(ENGINES[ENGI_GURDY]->presets[6].name, "TROTTO GURDY") && ENGINES[ENGI_GURDY]->presets[6].e[0] == 9 &&
          ENGINES[ENGI_GURDY]->presets[6].e[2] > 0 && ENGINES[ENGI_GURDY]->presets[6].e[5] == 1);

    /* es_phrase_load: the melody's steps, ties and accents, LEN and DIV, as the sequencer reads them */
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], ENGI_GURDY, 6);
    es_phrase_load(&trk[0], 0);
    ok = trk[0].p[P_SLEN] == 36 && trk[0].p[P_SDIV] == 4;
    for (i = 0; i < NSTEP; i++) {
        const step_t *st = &trk[0].step[i];
        uint32_t v = i < 36u ? ES_PHRASES[0].step[i] : 0u;
        if (v == ES_T)
            ok &= st->time == ST_TIE && st->n == 1 && st->note[0] == trk[0].step[i - 1].note[0];
        else if (v)
            ok &= st->time == ST_NOTE && st->n == 1 && st->note[0] == v && st->vel == 96 &&
                  !!(st->flags & SF_ACCENT) == (int)((ES_PHRASES[0].acc >> i) & 1u);
        else
            ok &= st->time == ST_REST && st->n == 0;
    }
    check("es_phrase_load: TROTTO's notes, ties and bar accents into the steps, LEN 36, DIV 8T, the rest empty", ok);
    host_preset(&trk[3], ENGI_DRUM, 1);
    es_phrase_load(&trk[3], 3);
    ok = trk[3].p[P_SLEN] == 12 && trk[3].p[P_SDIV] == 4;
    for (i = 0; i < 12u; i++)
        ok &= trk[3].step[i].hit == ES_PHRASES[3].step[i] && trk[3].step[i].n == 0 &&
              trk[3].step[i].time == (ES_PHRASES[3].step[i] ? ST_NOTE : ST_REST) &&
              trk[3].step[i].acc == (((ES_PHRASES[3].acc >> i) & 1u) ? ES_PHRASES[3].step[i] : 0u);
    check("es_phrase_load: the TABOR's lane hits and accents into the DRUM grid, LEN 12, DIV 8T", ok);

    printf(bad ? "SCENE TEST FAILED\n" : "scene test passed\n");
    return bad;
}
