/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX's TEMP (P_TEMP), moved out of dsp.c when that became Felucca-Engines' (MIT): included after it, as it
 * builds on dsp.c's cents_inc arithmetic */
/* TEMP (P_TEMP), the track's temperament from its ROOT: each semitone above ROOT, in 1/10 cent from equal.
 * PYTH pure fifths (3:2) and MEAN quarter-comma ones (5^(1/4)) from the flat second (five down) to the sharp fourth
 * (six up): the wolf between those two, out of the minor and dungeon scales' way. JUST 5-limit, the minor seventh 16:9
 * (bVII's fifth pure; 9:5 would make it a wolf). Append-only: a project keeps its value */
static const int16_t TEMPER[4][12] = {
    {0},                                                               /* EQUAL */
    {0, -98, 39, -59, 78, -20, 117, 20, -78, 59, -39, 98},             /* PYTH */
    {0, 171, -68, 103, -137, 34, -205, -34, 137, -103, 68, -171},      /* MEAN */
    {0, 117, 39, 156, -137, -20, -98, 20, 137, -156, -39, -117},       /* JUST */
};

/* note n on a track of values p: TEMP's whole 1/16 semitones (*d16) and the rest in 1/4096 of the increment (as
 * cents_inc); EQUAL 0 and 0 */
static int32_t temper(const int16_t *p, uint32_t n, int32_t *d16)
{
    int32_t c = TEMPER[(uint32_t)p[P_TEMP] & 3u][(n + 12u - (uint32_t)p[P_ROOT] % 12u) % 12u] * 16;   /* 1/16000 st */
    *d16 = c / 1000;
    return (c - *d16 * 1000) * 2367 / 160000;
}
