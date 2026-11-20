/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* A song uses the sequences of four saved projects, with the current sounds.
 * Sources are copied in the main loop before PLAY. The ISR changes only their
 * index and five timing parameters; the editable steps stay untouched.
 *
 * Lanes (1.2): a section (a row) names the slot each track plays (core.h chain_row_t). Each track reads its steps (with
 * CHANCE, RATCH, NUDGE), its automation and locks, LEN / DIV / SWING / GATE and QUANTIZE from its own slot's copy for
 * that track only; all four slots in use are copied before PLAY (chain_prepare: the four sources were there already
 * for whole rows, so lanes cost no more RAM). Sections keep one boundary for all four tracks: a section lasts `repeat`
 * loops of its clock track (chain_clock): the first track that is not silent there (track 1 when it plays: every song
 * before lanes counts as it did), or, every track silent, a 4/4 bar (CHAIN_SIL_*). At each section all four tracks
 * start together from their step 1, as rows always did.
 * A silent track ("-", CHAIN_SILENT) plays nothing in that section: at its start the track's sequenced notes are
 * released (seq_release, as at every section) and its automation goes back to the sound's own values (motion_restore);
 * no step, automation or lock of any slot plays there. Live keys and the arp still play it (the sound stays). */
typedef struct {
    step_t step[NTRK][NSTEP];
    int16_t timing[NTRK][4];
    uint8_t qnt[NTRK];                       /* each track's QUANTIZE (P_SQNT) as saved */
    motion_store_t motion;
} chain_pattern_t;
static chain_config_t chain_config;
static struct {
    chain_pattern_t source[4];
    chain_config_t config;
    int16_t timing[NTRK][4];                 /* the editable LEN DIV SWING GATE and QUANTIZE, back on STOP */
    uint8_t qnt[NTRK];
    volatile uint8_t armed, running, row, remaining;
    uint8_t lane[NTRK];                      /* the slot each track plays in this section, CHAIN_SILENT none */
    uint8_t clk;                             /* the track whose loop counts the section (chain_clock) */
    uint8_t rec;
    uint8_t stream;                          /* ESTAMPIE: a scene's song (es_stream), not the user's */
    uint8_t tick_row;                        /* ESTAMPIE: the row chain_apply applied last (0 or 1 with a stream) */
    uint32_t carry;
} chain;
/* ESTAMPIE: a scene's song (engines.c es_song_t), streamed from ROM through two of the slots: the section playing in
 * one (chain.row 0 or 1), the next one put in the other by the main loop (es_stream_poll) while it plays, so a song of
 * any length and phrases of any of ES_PHRASES fit, and no project slot is read. The ISR goes on only once that one
 * is ready (else the section plays again), and sets each section's tempo: a percentage of the BPM at PLAY (base); STOP
 * puts the base back, so the scene stays as it was (the autosave, ui.c es_song_ok). Turning the tempo while it plays
 * moves the base, so the rest of the song keeps to it (es_stream_poll) */
static struct {
    const es_song_t *song;
    uint16_t next;                           /* the section the free slot gets next */
    uint16_t sec[2];                         /* each slot's section (the header shows the playing one's) */
    volatile uint8_t want;                   /* the ISR moved on: the slot it left is free (main loop) */
    volatile uint8_t ready;                  /* the free slot holds the next section */
    uint8_t pct[2];                          /* each slot's section's tempo, % of base */
    volatile int16_t base;                   /* the BPM at 100 % (at PLAY; moved by a tempo turned) */
    volatile int16_t set;                    /* the BPM the ISR set last (another one: turned by hand) */
} es_stream;
static int chain_song(void) { return chain.running && !chain.stream; }   /* the user's song plays (the SONG page) */
/* a silent lane: its steps (empty: nothing plays) and its timing (a 4/4 bar of 1/16: the section's length when every
 * track is silent) */
static const step_t chain_silent[NSTEP] = {{{0}}};
#define CHAIN_SIL_LEN 16
#define CHAIN_SIL_DIV 2                      /* 1/16 */

static void seq_release(track_t *t);
static void seq_stop(void);
static uint32_t div_samples(uint32_t div);
static uint32_t step_samples(const track_t *t, uint32_t period, uint32_t idx);

static chain_row_t chain_row_of(uint32_t slot, uint32_t repeat)   /* a section with all four tracks on one slot */
{
    chain_row_t r;
    memset(r.slot, (int)slot, sizeof r.slot);
    r.repeat = (uint8_t)repeat;
    return r;
}
static void chain_defaults(chain_config_t *c)
{
    uint32_t i;
    memset(c, 0, sizeof *c);
    for (i = 0; i < CHAIN_ROWS; i++)
        c->row[i].repeat = 1;
}
static int chain_valid(const chain_config_t *c)
{
    uint32_t i, k;
    if (c->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < c->count; i++) {
        for (k = 0; k < NTRK; k++)
            if (c->row[i].slot[k] > CHAIN_SILENT)
                return 0;
        if (!c->row[i].repeat || c->row[i].repeat > 16u)
            return 0;
    }
    return 1;
}
/* the chain of FUN6..FUN9 (rows {slot, repeat}) -> today's: each row a section with the four tracks on its slot. 0: not
 * a valid one (as chain_valid then) */
static int chain_from_v9(chain_config_t *c, const chain_v9_t *o)
{
    uint32_t i;
    chain_defaults(c);
    if (o->count > CHAIN_ROWS)
        return 0;
    for (i = 0; i < o->count; i++) {
        if (o->row[i].slot >= 4u || !o->row[i].repeat || o->row[i].repeat > 16u)
            return 0;
        c->row[i] = chain_row_of(o->row[i].slot, o->row[i].repeat);
    }
    c->count = o->count;
    return 1;
}
/* the track whose loops count section r's length: the first one not silent; every track silent: track 1 (a bar) */
static uint32_t chain_clock(const chain_row_t *r)
{
    uint32_t k;
    for (k = 0; k < NTRK; k++)
        if (r->slot[k] < CHAIN_SILENT)
            return k;
    return 0;
}
/* a track parameter a playing song sets from its slots (the editor and the pages leave it alone meanwhile) */
static int chain_owns(uint32_t id) { return (id >= P_SLEN && id <= P_SGATE) || id == P_SQNT; }
static const step_t *seq_steps(const track_t *t)
{
    uint32_t k = (uint32_t)(t - trk), s;
    if (!chain.running)
        return t->step;
    s = chain.lane[k];
    return s < 4u ? chain.source[s].step[k] : chain_silent;
}
static void motion_restore(track_t *t);
/* a section starts (chain_start, chain_tick: once a section, not per block; not inlined: off the audio ISR's loop) */
static __attribute__((noinline)) void chain_apply(void)
{
    uint32_t i;
    const chain_row_t *r;
    if (chain.stream && chain.tick_row != chain.row) {   /* ESTAMPIE: a scene's song (es_stream) moved on (chain_tick */
        chain.row = es_stream.ready ? chain.row & 1u : chain.tick_row;   /* counts on, as for a song): the other slot */
        chain.remaining = chain.config.row[chain.row].repeat;           /* once filled, else this section again */
        if (es_stream.ready) {
            es_stream.ready = 0;
            es_stream.want = 1;
        }
    }
    chain.tick_row = chain.row;
    r = &chain.config.row[chain.row];
    chain.clk = (uint8_t)chain_clock(r);
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        uint32_t s = r->slot[i];
        chain.lane[i] = (uint8_t)(s < 4u ? s : CHAIN_SILENT);
        seq_release(t);
        motion_restore(t);
        if (s < 4u) {
            memcpy(&t->p[P_SLEN], chain.source[s].timing[i], sizeof chain.timing[i]);
            t->p[P_SQNT] = chain.source[s].qnt[i];
        } else {
            t->p[P_SLEN] = CHAIN_SIL_LEN;
            t->p[P_SDIV] = CHAIN_SIL_DIV;
            t->p[P_SSWING] = 0;
            t->p[P_SQNT] = 1;
        }
        t->seq_idx = (uint16_t)(t->p[P_SLEN] - 1);
        t->seq_pos = 0x7FFFFFFFu;
        t->rh_n = t->rskip_n = 0;
    }
    if (chain.stream) {                      /* (a constant divisor: a multiply) */
        int32_t bpm = es_stream.base * (int32_t)es_stream.pct[chain.row & 1u] / 100;
        song.g[G_BPM] = (int16_t)clamp(bpm, GP[G_BPM].min, GP[G_BPM].max);
        es_stream.set = song.g[G_BPM];
    }
}
static void chain_start(void)
{
    uint32_t i;
    if (!chain.armed)
        return;
    chain.rec = song.rec;
    song.rec = 0;
    for (i = 0; i < NTRK; i++) {
        memcpy(chain.timing[i], &trk[i].p[P_SLEN], sizeof chain.timing[i]);
        chain.qnt[i] = (uint8_t)trk[i].p[P_SQNT];
    }
    chain.row = 0;
    chain.remaining = chain.config.row[0].repeat;
    chain.carry = 0;
    chain.running = 1;
    chain.armed = 0;
    chain_apply();
}
static void chain_stop(void)
{
    uint32_t i;
    chain.armed = 0;
    if (!chain.running)
        return;
    for (i = 0; i < NTRK; i++) {
        memcpy(&trk[i].p[P_SLEN], chain.timing[i], sizeof chain.timing[i]);
        trk[i].p[P_SQNT] = chain.qnt[i];
    }
    song.rec = chain.rec;
    if (chain.stream)
        song.g[G_BPM] = es_stream.base;      /* (the scene's tempo again) */
    chain.stream = 0;
    chain.running = 0;
}
static void chain_tick(uint32_t n)
{
    const track_t *t = &trk[chain.clk % NTRK];
    uint32_t length;
    if (!chain.running || t->seq_pos >= 0x7FFFFFFFu || t->seq_idx + 1u != (uint32_t)t->p[P_SLEN])
        return;
    length = step_samples(t, div_samples((uint32_t)t->p[P_SDIV]), t->seq_idx);
    if (t->seq_pos + n < length)
        return;
    if (chain.remaining > 1u) {
        chain.remaining--;
        return;
    }
    if (chain.row + 1u >= chain.config.count) {
        seq_stop();
        return;
    }
    chain.carry = t->seq_pos + n - length;
    chain.row++;
    chain.remaining = chain.config.row[chain.row].repeat;
    chain_apply();
}

/* ESTAMPIE: the scene song's section sec into slot (0 or 1): each part's phrase (with its LEN and DIV; SWING, GATE
 * and QUANTIZE the track's own, as the sound is), silent where it has none, no automation; the section's repeats and
 * tempo. Main loop only, and never the slot playing */
static void es_slot_fill(uint32_t slot, uint32_t sec)
{
    const es_row_t *r = &es_stream.song->row[sec % es_stream.song->n];
    chain_pattern_t *src = &chain.source[slot];
    chain_row_t *row = &chain.config.row[slot];
    uint32_t k;
    src->motion.count = 0;
    for (k = 0; k < NTRK; k++) {
        if (k >= NPART || r->phr[k] == ES_SIL) {
            row->slot[k] = CHAIN_SILENT;
            continue;
        }
        const es_phrase_t *ph = es_phrase_steps(src->step[k], r->phr[k]);
        src->timing[k][0] = ph->len;
        src->timing[k][1] = ph->div;
        src->timing[k][2] = trk[k].p[P_SSWING];
        src->timing[k][3] = trk[k].p[P_SGATE];
        src->qnt[k] = trk[k].p[P_SQNT] != 0;
        row->slot[k] = (uint8_t)slot;
    }
    row->repeat = r->rep ? r->rep : 1u;
    es_stream.pct[slot] = r->pct ? r->pct : 100u;
    es_stream.sec[slot] = (uint16_t)(sec % es_stream.song->n);
}
/* ESTAMPIE: the scene song set up in slots 0 and 1 from its start, at the tempo now (ui.c es_song_play arms it) */
static void es_stream_begin(const es_song_t *s)
{
    es_stream.song = s;
    es_stream.base = es_stream.set = song.g[G_BPM];
    chain_defaults(&chain.config);
    es_slot_fill(0, 0);
    es_slot_fill(1, 1);
    es_stream.next = (uint16_t)(2u % s->n);
    es_stream.want = 0;
    es_stream.ready = 1;
    chain.config.count = CHAIN_ROWS;         /* (chain_tick counts on; chain_apply brings it back to 0 or 1) */
    chain.tick_row = 0;                      /* (chain_start: row 0) */
    chain.stream = 1;
}
/* ESTAMPIE: the main loop's part (ui_input): a tempo turned since the ISR set one moves the base (read between two
 * reads of the ISR's, so not across a section's start); the next section into the slot the ISR left */
static void es_stream_poll(void)
{
    int16_t set, bpm;
    if (!chain.stream || !chain.running)
        return;
    set = es_stream.set;
    bpm = song.g[G_BPM];
    if (bpm != set && set == es_stream.set) {
        es_stream.base = (int16_t)(bpm * 100 / es_stream.pct[chain.row & 1u]);
        es_stream.set = bpm;
    }
    if (!es_stream.want)
        return;
    es_stream.want = 0;
    es_slot_fill((chain.row & 1u) ^ 1u, es_stream.next);
    es_stream.next = (uint16_t)((es_stream.next + 1u) % es_stream.song->n);
    RING_PUBLISH();
    es_stream.ready = 1;
}
