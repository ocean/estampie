/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* HOME > KEYS (GHOULBOX 2.1: seq.c keyboard_block, keys_split, keys_note; core.h G_KMODE .. G_KOCT) through the real
 * keyboard scan, the voices and the project format (the UI test's harness):
 * 1. OFF: a key plays the selected track only (as ever).
 * 2. LAYER: a key plays the selected track and KTRK, the key-up ends both; MIDI OUT on both channels; KTRK the
 *    selected track itself: one note, no second.
 * 3. LAYER with OCT +1 / -2: KTRK's note moved, the selected track's not.
 * 4. SPLIT at C4 (the default): B3 plays KTRK only, C4 the selected track only; SPLT moves the split.
 * 5. MODE changed while a key is held: its key-up still ends what it started (both, in LAYER).
 * 6. the SEQ pages (step entry): the keys play the selected track only.
 * 7. the project: KEYS saved and loaded (FUN10's tail, PROJ_KEYS_OFF); a project without them (the word 0): OFF.
 * 8. NOTE NAMES (2.1, ui_input.c names_notice): the names of notes and chords, in the header while keys are held, never
 *    over another message, none with MENU > CONTROL > NOTE NAMES OFF; ESTAMPIE: not on HOME, whose readout names them. */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

static void k_reset(void)
{
    uint32_t p, i;
    ui_power_on();
    fm1_in.notes = kb_prev = 0; song.sel = 0;
    memset(midi_notes, 0, sizeof midi_notes);
    memset(lt, 0, sizeof lt);
    for (p = 0; p < NTRK; p++)
        trk_all_off(&trk[p]);
    for (i = 0; i < 8u; i++)
        events_block(CTL);
    for (p = 0; p < NPART; p++)
        for (i = 0; i < NVOICE; i++)
            trk[p].v[i].active = 0;
    for (p = 0; p < NTRK; p++)                        /* (POLY, no chord, no scale: a key's own note) */
        trk[p].p[P_VOICE] = V_POLY;
    song.g[G_KMODE] = 0; song.g[G_KTRK] = 3; song.g[G_KSPLIT] = 4; song.g[G_KOCT] = 2;
    usb.config = 1; mo_r = mo_w;
}
static void keys(uint32_t m) { fm1_in.notes = m; events_block(CTL); }
static int kgated(const track_t *t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].gate && t->v[i].note == note && t->v[i].stage != 4u)
            return 1;
    return 0;
}
static uint32_t ngated(const track_t *t)
{
    uint32_t n = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        n += t->v[i].active && t->v[i].gate && t->v[i].stage != 4u;
    return n;
}
static uint32_t sent_on(uint32_t ch, uint32_t n)       /* MIDI OUT note-ons of note n on channel ch since mo_r */
{
    uint32_t c = 0;
    for (uint32_t i = mo_r; i != mo_w; i++) {
        uint32_t p = midi_out_q[i % MQ];
        c += (p & 0xFu) == 9u && ((p >> 8) & 0x0Fu) == ch && ((p >> 16) & 0x7Fu) == n;
    }
    return c;
}

int main(void)
{
    int bad = 0;
    track_t *t1 = &trk[0], *t4 = &trk[3];
    uint32_t a, b, c, n4;
    project_t q;
    project_store_t st;

    k_reset();                                          /* 1 */
    keys(1u << 7); a = kb_note[7];
    bad += check("OFF: C4 plays the selected track only", kgated(t1, a) && !ngated(t4));
    keys(0);

    k_reset(); song.g[G_KMODE] = 1;                     /* 2 */
    keys(1u << 7); a = kb_note[7]; n4 = kb_map(t4, 7);
    bad += check("LAYER: C4 plays T1 and T4 (KTRK) together", kgated(t1, a) && kgated(t4, n4));
    bad += check("  MIDI OUT: a note-on on each track's channel",
                 sent_on(trk_midi_ch(0), a) == 1u && sent_on(trk_midi_ch(3), n4) == 1u);
    keys(0);
    bad += check("  the key up: both end", !kgated(t1, a) && !kgated(t4, n4));
    k_reset(); song.g[G_KMODE] = 1; song.g[G_KTRK] = 0;
    keys(1u << 7);
    bad += check("  KTRK the selected track: one note, no second", ngated(t1) == 1u && kb_trk2[7] == 0u);
    keys(0);

    k_reset(); song.g[G_KMODE] = 1; song.g[G_KTRK] = 1; song.g[G_KOCT] = 3;   /* 3: T2 (a melodic engine) */
    keys(1u << 7); a = kb_note[7]; n4 = kb_map(&trk[1], 7);
    bad += check("LAYER OCT +1: T2 an octave up, T1 as it was", kgated(t1, a) && kgated(&trk[1], n4 + 12u) &&
                 !kgated(&trk[1], n4));
    keys(0);
    k_reset(); song.g[G_KMODE] = 1; song.g[G_KTRK] = 1; song.g[G_KOCT] = 0;
    keys(1u << 7); n4 = kb_map(&trk[1], 7);
    bad += check("LAYER OCT -2: T2 two octaves down", kgated(&trk[1], n4 - 24u));
    keys(0);
    k_reset(); song.g[G_KMODE] = 1; song.g[G_KOCT] = 4;
    keys(1u << 7); n4 = kb_map(t4, 7);
    bad += check("  a DRUM KTRK (its own key map): OCT leaves its lanes as they are", drum_track(t4) && kgated(t4, n4));
    keys(0);

    k_reset(); song.g[G_KMODE] = 2;                     /* 4 */
    keys(1u << 6); b = kb_note[6];
    bad += check("SPLIT at C4: B3 plays T4 (KTRK) only", kgated(t4, b) && !ngated(t1) && kb_trk[6] == 3u);
    keys(0);
    keys(1u << 7); c = kb_note[7];
    bad += check("  C4 plays T1 (the selected track) only", kgated(t1, c) && !ngated(t4) && kb_trk[7] == 0u);
    keys(0);
    song.g[G_KSPLIT] = 9;                               /* (F4: key 12) */
    keys(1u << 11 | 1u << 12);
    bad += check("  SPLT F4: E4 on T4, F4 on T1", kgated(t4, kb_note[11]) && kgated(t1, kb_note[12]) && ngated(t1) == 1u &&
                 ngated(t4) == 1u);
    keys(0);
    bad += check("  both let go: silent", !ngated(t1) && !ngated(t4));

    k_reset(); song.g[G_KMODE] = 1;                     /* 5 */
    keys(1u << 9); a = kb_note[9]; n4 = kb_map(t4, 9);
    song.g[G_KMODE] = 0;
    keys(0);
    bad += check("MODE turned OFF with a key held: its key-up still ends both notes", !kgated(t1, a) && !kgated(t4, n4));
    k_reset(); song.g[G_KMODE] = 2;
    keys(1u << 3); b = kb_note[3];
    song.g[G_KMODE] = 1; song.g[G_KTRK] = 1;
    keys(0);
    bad += check("  SPLIT's low key with MODE and KTRK changed meanwhile: T4's note ends", !kgated(t4, b) && !ngated(&trk[1]));

    k_reset(); song.g[G_KMODE] = 1;                     /* 6 */
    go_page(GR_ROLL);
    keys(1u << 7);
    bad += check("SEQ > STEP: the keys play the selected track only (they enter its steps)", ngated(t1) == 1u && !ngated(t4));
    keys(0);
    ui.home = 1; page_entered();
    k_reset(); song.g[G_KMODE] = 2;
    go_page(GR_ROLL);
    keys(1u << 3);
    bad += check("  SPLIT too: a low key plays the selected track", ngated(t1) == 1u && !ngated(t4));
    keys(0);

    k_reset();                                          /* 7 */
    song.g[G_KMODE] = 2; song.g[G_KTRK] = 1; song.g[G_KSPLIT] = 23; song.g[G_KOCT] = 0;
    project_capture(&q);
    bad += check("the project: packs", proj_pack(&st, &q) == 1);
    memset(&q, 0, sizeof q);
    bad += check("  and loads SPLIT T2 G5 -2", proj_import(&q, &st, (int)sizeof st) && q.g[G_KMODE] == 2 && q.g[G_KTRK] == 1 &&
                 q.g[G_KSPLIT] == 23 && q.g[G_KOCT] == 0);
    st.raw[PROJ_KEYS_OFF] = st.raw[PROJ_KEYS_OFF + 1u] = 0;   /* a project saved before 2.1: no KEYS word */
    {
        uint32_t sum = proj_hash(st.raw, PROJ_STORE_SIZE - 4u);
        memcpy(st.raw + PROJ_STORE_SIZE - 4u, &sum, 4);
    }
    memset(&q, 0, sizeof q);
    bad += check("  a project from before 2.1 (word 0): KEYS OFF, T4, C4, 0", proj_import(&q, &st, (int)sizeof st) &&
                 q.g[G_KMODE] == 0 && q.g[G_KTRK] == 3 && q.g[G_KSPLIT] == 4 && q.g[G_KOCT] == 2);

    {   /* 8 */
        static const struct { uint8_t n[4]; const char *name; } NN[] = {
            {{60, 64, 67}, "C"}, {{57, 60, 64}, "Am"}, {{64, 67, 72}, "C/E"}, {{62, 65, 69, 72}, "Dm7"},
            {{55, 60, 62}, "Gsus4"}, {{50, 57}, "D5"}, {{62}, "D4"}, {{57, 61, 64, 67}, "A7"}, {{58, 62, 65}, "Bb"},
            {{62, 63}, "D Eb"}, {{59, 62, 65}, "Bdim"},
        };
        char b[16];
        uint32_t i, j, m;
        int ok = 1;
        for (i = 0; i < NELEM(NN); i++) {
            for (m = 0, j = 0; j < 4u && NN[i].n[j]; j++)
                m |= 1u << (NN[i].n[j] % 12u);
            names_of(m, NN[i].n[0], b);
            if (!str_eq(b, NN[i].name)) { printf("  %s: got %s\n", NN[i].name, b); ok = 0; }
        }
        bad += check("NOTE NAMES: D4, D5, C, Am, Bb, Bdim, Gsus4, A7, Dm7, C/E (a bass not the root), D Eb (no chord)", ok);
    }
    k_reset();
    keys(1u << 7 | 1u << 11 | 1u << 14); names_notice();          /* C4 E4 G4 */
    bad += check("  ESTAMPIE: on HOME, its readout names them (ui_graph.c graph_played): the header stays quiet",
                 ui.home && home_readout() && !ui.msg_t);
    keys(0);
    go_page(GR_ADSR);                                   /* (a page with no readout: the header names them) */
    keys(1u << 7 | 1u << 11 | 1u << 14); names_notice();
    bad += check("  held keys: their chord in the header (C)", ui.msg_t && str_eq(ui.msg, "C"));
    keys(1u << 7 | 1u << 11 | 1u << 14 | 1u << 17); names_notice();   /* + A#4 */
    bad += check("  a key added: the name follows (C7)", str_eq(ui.msg, "C7"));
    keys(0); names_notice();                            /* let go: the name fades .. */
    keys(1u << 9); names_notice();                      /* .. and a key played before it has gone renames it (D4) */
    bad += check("  the next key while the name still fades: the name follows (a melody)", ui.msg_t && str_eq(ui.msg, "D4"));
    keys(1u << 7 | 1u << 11 | 1u << 14 | 1u << 17); names_notice();
    ui_message("STOP TO EDIT"); names_notice();
    bad += check("  another message: it stays, the name waits", str_eq(ui.msg, "STOP TO EDIT"));
    keys(0); ui.msg_t = 0;
    ui_prefs2 |= (uint8_t)(PREF_NAMES_OFF >> 16);
    keys(1u << 7); names_notice();
    bad += check("  NOTE NAMES OFF: nothing named", !ui.msg_t);
    keys(0); ui_prefs2 &= (uint8_t)~(PREF_NAMES_OFF >> 16);
    ui.msg_t = 0;
    menu_put(MI_NAMES, 1);                              /* the MENU row itself: OFF */
    keys(1u << 7); names_notice();
    bad += check("  MENU > CONTROL > NOTE NAMES: OFF from the row silences the names, ON back",
                 menu_get(MI_NAMES) == 1u && !ui.msg_t && MI_TAB[MI_NAMES] == MTAB_CONTROL);
    keys(0);
    menu_put(MI_NAMES, 0);
    keys(1u << 7); names_notice();
    bad += check("  .. and ON from the row names again", menu_get(MI_NAMES) == 0u && ui.msg_t && str_eq(ui.msg, "C4"));
    keys(0);

    usb.config = 0;
    printf("%s\n", bad ? "KEYS TEST FAILED" : "keys test passed");
    return bad != 0;
}
