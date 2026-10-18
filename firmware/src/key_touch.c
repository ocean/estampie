/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE: KEY TOUCH, velocity and aftertouch from the FM-1's own keys, with no hardware change. Ported from Charles
 * Vestal's Velo (github.com/charlesvestal/fm1-velocity) as MONK-1 (github.com/charlesvestal/fm1-monk) plays it
 * (sense.c and ui.c pressure(); GPL-3.0-only), to integer arithmetic (this firmware has no float) and to this
 * firmware's keyboard: the scan still decides when a key is down (hal/fm1_input.h), this only measures how hard.
 *
 * How: each key is a rubber dome with a carbon pill, whose resistance falls as it is pressed harder. Two of the four
 * matrix rows (PA7, PA8: notes 0..19, F3..C5) are converter inputs too, so the scan tick samples the key of the column
 * it has just latched on one of them, each key every ~2.2 ms (fm1_input.h fm1_an). A key read against the row's
 * pull-up only shows the matrix diode's drop once it seats, so every other sample of a held key is taken with its row
 * driven high by the pin's weak driver (DRIVE 2, Velo's default): then the reading is the diode (KT_DBASE) plus the
 * pill's resistance, spread over a few hundred counts. The top seven keys (PA6) have no converter: they play at
 * MENU > SELECT VEL, as every key does with KEY TOUCH OFF.
 *
 * The state machine (kt_sample, in the scan ISR) runs on the pulled-up samples: OPEN, TOUCH (below KT_TOUCH: the pill
 * conducts), DOWN (seated below KT_PRESS, KT_WAIT samples after the touch at least), LIFT, and OPEN again after two
 * samples above KT_RELEASE (a seated key can read open for one). The dome snaps, so every press lands within a sample
 * or two whatever its speed: what tells a hard hit from a soft one is how deep the key sits just after landing. The
 * velocity is the lowest driven reading since the touch (the scan ISR keeps it, kt_gate maps it), on a log scale between the key's floor (its firmest landing:
 * 127) and its ceiling (its lightest landing that seats: 1): t = log(r / r_floor) / log(r_ceil / r_floor) with r the
 * reading less KT_DBASE (conductive rubber's resistance falls as a power of the force, so its log is linear in the
 * force's), velocity = 1 + 126 (1 - t)^0.7 (Velo's curve: free playing gave quartiles 45 / 63 / 80).
 *
 * The keyboard (seq.c keyboard_block, kt_gate) holds a measured key's note back until its velocity is in (KT_WAIT
 * pulled samples, two driven: ~6.6 ms from the touch), at most KT_GATE_MS after the scan saw it down; then it plays
 * as any key, at its own velocity. A key that never seats in time plays at SELECT VEL.
 *
 * Aftertouch (VEL+AT, kt_pressure every KT_AT_MS from the audio ISR): pressing harder than the key rests. A key goes
 * on settling after it lands, so the zero follows the driven reading down for KT_SETTLE_MS, then creeps after it
 * (seconds), and comes back up at once when the key is eased. The lean is the pill's resistance below that rest in
 * octaves: KT_DEAD is noise, the span (AT SENS: 0.8 .. 0.12 octave) is 127. Smoothed over ~30 ms. A track's
 * aftertouch (mod.c MS_AT, the modulation matrix's AT) is the most any of its keys lean; each key's goes out as
 * polyphonic aftertouch with its notes.
 *
 * Calibration (MENU > KEYS > TOUCH CAL, ui_touch.c): every measured key pressed gently, then firmly, KT_CAL_N times
 * each; the ceiling is the median of its gentle landings, the floor of its firm ones. Kept in the settings
 * (settings_persist.c: favorites.factory[KT_ROW]): 6 bits each, floor 185 + 10 f, ceiling floor + 80 + 10 c. Until a
 * key is calibrated it uses Velo's defaults for DRIVE 2 (floor 450, ceiling 896).
 *
 * No HAL here: tests/key_touch_test.c runs it on synthetic presses. */

#define KT_NKEY 20u                    /* the measured keys: notes 0..19 (PA7, PA8) */
#define KT_ANALOG 0xFFFFFu
#define KT_TOUCH 900u                  /* pulled-up readings (1023 open) */
#define KT_PRESS 400u
#define KT_RELEASE 600u
#define KT_WAIT 2u                     /* pulled samples after the touch before the velocity (two driven between) */
#define KT_DBASE 185u                  /* a driven reading with no pill resistance: the diode and the 595 */
#define KT_FLOOR0 450u                 /* the defaults (Velo, DRIVE 2) */
#define KT_CEIL0 896u
#define KT_SPAN 80u                    /* the least a ceiling sits above its floor */
#define KT_GATE_MS 12u                 /* the longest a measured key's note waits for its velocity */
#define KT_AT_MS 5u
#define KT_SETTLE_MS 250u
#define KT_DEAD 10                     /* aftertouch: Q8 octaves of noise (0.04) */
#define KT_CAL_N 3u                    /* calibration: presses per key and kind */
#define KT_ROW 1u                      /* settings: favorites.factory[1] (DIGITAL's, retired: settings_persist.c) */
#define KT_TAG 0x4Bu                   /* ('K') the row holds KEY TOUCH's settings */
enum { KT_OFF, KT_VEL, KT_VELAT, KT_MODES };
enum { KT_S_OPEN, KT_S_TOUCH, KT_S_DOWN, KT_S_LIFT };

typedef struct {
    uint16_t v;                        /* the last pulled-up reading */
    uint16_t d, dmin;                  /* driven: the last reading, the lowest since the touch */
    uint16_t floor_, ceil;             /* the key's ends (calibration) */
    uint8_t st, since, up, dn, dnext;
    uint16_t dland;                    /* the last landing's lowest driven reading (0xFFFF: none driven) */
    volatile uint8_t land;             /* bumps at each landing (kt_gate: its velocity from dland) */
} kt_key_t;

static kt_key_t kt_key[KT_NKEY];
static uint8_t kt_mode;                /* MENU > KEYS > KEY TOUCH: OFF, VEL, VEL+AT */
static uint8_t kt_sens = 5;            /* MENU > KEYS > AT SENS, 1..9 */
static uint8_t kt_calibrated;          /* the ends came from a TOUCH CAL (else the defaults) */
static volatile uint8_t kt_live;       /* the scan is measuring the keys (main.c: KEY TOUCH on, on the hardware); the
                                        * emulator and the host never are, and their keys never wait */

/* calibration in progress (ui_touch.c): 1 gentle, 2 firm; each landing of a measured key is kept */
static volatile uint8_t kt_cal_phase;
static uint16_t kt_cal_v[2][KT_NKEY][KT_CAL_N];
static uint8_t kt_cal_n[2][KT_NKEY];

/* (i / 32)^0.7 in Q12, i = 0..32: the velocity's curve */
static const uint16_t KT_POW07[33] = {0, 362, 588, 781, 955, 1117, 1269, 1414, 1552, 1685, 1814, 1940, 2061, 2180,
                                      2296, 2410, 2521, 2631, 2738, 2844, 2948, 3050, 3151, 3251, 3349, 3446, 3542,
                                      3637, 3730, 3823, 3915, 4006, 4096};

/* log2(x) in Q8 (x >= 1): the exponent (a binary search: no loop, the audio ISR's cost budget), and the mantissa's
 * log by f + 0.3466 f (1 - f) (within 0.008 octave); 32-bit arithmetic only */
static int32_t kt_log2q8(uint32_t x)
{
    uint32_t e = 0, m;
    int32_t f;
    if (!x)
        x = 1;
    if (x >> 16)
        e = 16;
    if (x >> (e + 8u))
        e += 8;
    if (x >> (e + 4u))
        e += 4;
    if (x >> (e + 2u))
        e += 2;
    if (x >> (e + 1u))
        e += 1;
    m = e <= 15u ? x << (15u - e) : x >> (e - 15u);
    f = (int32_t)(m - 32768u);                     /* Q15, 0..1 */
    f += (((f * (32768 - f)) >> 15) * 11358) >> 15;   /* (0.3466 in Q15) */
    return (int32_t)(e << 8) + (f >> 7);
}

static void kt_ends(kt_key_t *k, uint32_t fl, uint32_t cl)
{
    if (fl < KT_DBASE + 16u)
        fl = KT_DBASE + 16u;
    if (cl > 1020u)
        cl = 1020u;
    if (cl < fl + KT_SPAN) {                       /* too close: part them about the middle */
        uint32_t mid = (fl + cl) / 2u;
        fl = mid - KT_SPAN / 2u;
        cl = mid + KT_SPAN / 2u;
    }
    k->floor_ = (uint16_t)fl;
    k->ceil = (uint16_t)cl;
}

static void kt_defaults(void)
{
    uint32_t i;
    for (i = 0; i < KT_NKEY; i++)
        kt_ends(&kt_key[i], KT_FLOOR0, KT_CEIL0);
    kt_calibrated = 0;
}

static void kt_init(void)
{
    uint32_t i;
    for (i = 0; i < KT_NKEY; i++) {
        kt_key[i].v = kt_key[i].d = kt_key[i].dmin = 1023;
        kt_key[i].st = KT_S_OPEN;
        kt_key[i].dland = 0xFFFFu;
    }
    kt_defaults();
}

/* the velocity of driven reading d on a key with ends fl (127) .. cl (1) */
static uint8_t kt_velocity(uint32_t d, uint32_t fl, uint32_t cl)
{
    int32_t l, l0, l1, t, i, fr, p;
    if (d <= fl)
        return 127;
    if (d >= cl)
        return 1;
    l = kt_log2q8(d - KT_DBASE);
    l0 = kt_log2q8(fl > KT_DBASE ? fl - KT_DBASE : 1u);
    l1 = kt_log2q8(cl - KT_DBASE);
    if (l1 <= l0)
        return 64;
    t = 4096 - ((l - l0) << 12) / (l1 - l0);       /* 1 - t, Q12 */
    t = t < 0 ? 0 : t > 4096 ? 4096 : t;
    i = t >> 7;                                    /* the curve: 32 steps, interpolated */
    fr = t & 127;
    p = KT_POW07[i] + (i < 32 ? ((KT_POW07[i + 1] - KT_POW07[i]) * fr) >> 7 : 0);
    return (uint8_t)(1 + ((126 * p + 2048) >> 12));
}

/* the scan asks before each sample of key k: drive its row? (the first after the touch, then every other one) */
static int kt_want_drive(uint32_t k) { return (kt_mode || kt_cal_phase) && k < KT_NKEY && kt_key[k].dnext; }
/* the key's column LEDs may be lit while it is read (fm1_input.h): an open key. A lit LED's current goes through the
 * column and lifts its keys' readings (Velo measured +25): a held key's would jump as an LED changed */
static int kt_led_ok(uint32_t k) { return k >= KT_NKEY || kt_key[k].st == KT_S_OPEN; }
/* no measured key is touched (main.c: the battery's slow reading pauses the scan's sampling only then) */
static int kt_idle(void)
{
    uint32_t k;
    for (k = 0; k < KT_NKEY; k++)
        if (kt_key[k].st != KT_S_OPEN)
            return 0;
    return 1;
}

static void kt_cal_keep(uint32_t key, uint32_t m)
{
    uint32_t ph = kt_cal_phase - 1u, n = kt_cal_n[ph][key];
    if (n < KT_CAL_N) {
        kt_cal_v[ph][key][n] = (uint16_t)m;
        kt_cal_n[ph][key] = (uint8_t)(n + 1u);
    }
}

/* a sample of key k (0..19): v the reading, driven: taken with the row driven high. The scan ISR */
static void kt_sample(uint32_t key, uint32_t v, int driven)
{
    kt_key_t *k;
    if (key >= KT_NKEY)
        return;
    k = &kt_key[key];
    if (driven) {                                  /* the force: no state change (a driven graze reads open too) */
        k->dnext = 0;
        if (k->st == KT_S_OPEN)
            return;
        k->d = (uint16_t)v;
        if (k->st == KT_S_TOUCH)
            k->dn++;
        if (k->st != KT_S_LIFT && v < k->dmin)
            k->dmin = (uint16_t)v;
        return;
    }
    k->v = (uint16_t)v;
    if (k->st == KT_S_OPEN && v < KT_TOUCH) {
        k->st = KT_S_TOUCH;
        k->since = 0;
        k->dn = 0;
        k->dmin = 1023;
    }
    if (k->st == KT_S_TOUCH) {
        if (v >= KT_TOUCH) {                       /* gone before it seated: a graze */
            k->st = KT_S_OPEN;
        } else if (k->since >= KT_WAIT && v < KT_PRESS) {   /* seated: the landing (its velocity: kt_gate) */
            uint32_t m = k->dn ? k->dmin : 0xFFFFu;
            k->st = KT_S_DOWN;
            k->dland = (uint16_t)m;
            if (kt_cal_phase && m != 0xFFFFu)
                kt_cal_keep(key, m);
            k->land++;
        } else if (k->since < 255u) {
            k->since++;
        }
    } else if (k->st == KT_S_DOWN && v >= KT_PRESS) {
        k->st = KT_S_LIFT;
        k->up = 0;
    }
    if (k->st == KT_S_LIFT) {
        if (v < KT_PRESS)
            k->st = KT_S_DOWN;
        else if (v < KT_RELEASE)
            k->up = 0;
        else if (++k->up >= 2u)
            k->st = KT_S_OPEN;
    }
    k->dnext = (uint8_t)(k->st != KT_S_OPEN);      /* a held key: the next sample driven */
}

/* ------------------------------------------------------------------ the keyboard (seq.c keyboard_block, audio ISR) */
static uint8_t kt_seen[KT_NKEY];        /* the landing the keyboard last took */
static uint32_t kt_wait;                /* keys down whose velocity is not in yet */
static uint32_t kt_wait_ms[KT_NKEY];
static uint8_t kt_kvel[27];             /* the velocity each key played with (0: SELECT VEL's) */

/* the keys the keyboard may play now: cur (the scan's) less those still waiting for their velocity. prev: the keys it
 * played last time. A measured key newly down waits until its landing is in, at most KT_GATE_MS */
static uint32_t kt_gate(uint32_t cur, uint32_t prev, uint32_t now_ms)
{
    uint32_t k, newd = cur & ~prev & KT_ANALOG;
    if (!kt_mode || !kt_live) {
        kt_wait = 0;
        return cur;
    }
    kt_wait &= cur;                                /* (let go while waiting: nothing to play) */
    for (k = 0; k < KT_NKEY; k++) {
        if (!((newd >> k) & 1u))
            continue;
        if (!((kt_wait >> k) & 1u)) {              /* newly down: from now */
            kt_wait |= 1u << k;
            kt_wait_ms[k] = now_ms;
        }
        if (kt_key[k].land != kt_seen[k]) {        /* landed: its velocity */
            uint32_t m = kt_key[k].dland;
            kt_seen[k] = kt_key[k].land;
            kt_kvel[k] = m == 0xFFFFu ? 0u : kt_velocity(m, kt_key[k].floor_, kt_key[k].ceil);
            kt_wait &= ~(1u << k);
        } else if (now_ms - kt_wait_ms[k] >= KT_GATE_MS) {   /* never seated in time: SELECT VEL */
            kt_kvel[k] = 0;
            kt_wait &= ~(1u << k);
        }
    }
    for (k = KT_NKEY; k < 27u; k++)                /* the top row: SELECT VEL */
        if ((cur & ~prev) >> k & 1u)
            kt_kvel[k] = 0;
    return cur & ~kt_wait;
}

/* ------------------------------------------------------------------ aftertouch (VEL+AT) */
static int32_t kt_base8[KT_NKEY];        /* the rest, Q8 of the driven reading less KT_DBASE */
static uint32_t kt_t0[KT_NKEY];
static int32_t kt_ps[KT_NKEY];           /* the smoothed pressure, Q8 of 0..127 */
static uint8_t kt_on[KT_NKEY], kt_p[KT_NKEY];

/* key k's pressure now (0..127), from its driven readings; called every KT_AT_MS for every measured key */
static uint8_t kt_lean(uint32_t k, uint32_t now_ms)
{
    kt_key_t *x = &kt_key[k];
    int32_t d8, lean, span, xq;
    if (x->st != KT_S_DOWN || !x->dn || kt_mode != KT_VELAT) {
        kt_on[k] = 0;
        kt_ps[k] = 0;
        return 0;
    }
    d8 = (int32_t)((x->d > KT_DBASE + 1u ? x->d - KT_DBASE : 1u) << 8);
    if (!kt_on[k]) {
        kt_on[k] = 1;
        kt_base8[k] = d8;
        kt_t0[k] = now_ms;
    }
    if (d8 > kt_base8[k])                          /* eased off: that is the rest now */
        kt_base8[k] = d8;
    else if (now_ms - kt_t0[k] < KT_SETTLE_MS)     /* settling: follow it down */
        kt_base8[k] += (d8 - kt_base8[k]) / 4;
    else                                           /* then creep (~1/1250 a step) */
        kt_base8[k] += (d8 - kt_base8[k]) / 1024;
    lean = kt_log2q8((uint32_t)kt_base8[k]) - kt_log2q8((uint32_t)d8) - KT_DEAD;
    span = 205 - (205 * 85 / 100) * (kt_sens - 1) / 8;   /* AT SENS 1..9: 0.8 .. 0.12 octave (Q8) */
    xq = lean <= 0 ? 0 : lean >= span ? 127 << 8 : (lean << 15) / span * 127 >> 7;
    kt_ps[k] += (xq - kt_ps[k]) * 44 / 256;        /* (~30 ms at 5 ms) */
    return (uint8_t)((kt_ps[k] + 128) >> 8);
}

/* ------------------------------------------------------------------ the settings (settings_persist.c) */
/* favorites.factory[KT_ROW]: [0] KT_TAG, [1] the mode (bits 0-1), calibrated (bit 2), AT SENS (bits 4-7, 0: 5);
 * [2..31] per measured key 12 bits from bit 12 k: the floor 185 + 10 f (bits 0-5), the ceiling floor + 80 + 10 c
 * (bits 6-11). Not KT_TAG: OFF, the defaults */
static void kt_load(const uint8_t *r)
{
    uint32_t i, s;
    kt_defaults();
    kt_mode = KT_OFF;
    kt_sens = 5;
    if (r[0] != KT_TAG)
        return;
    kt_mode = (uint8_t)((r[1] & 3u) < KT_MODES ? r[1] & 3u : KT_OFF);
    s = r[1] >> 4;
    kt_sens = (uint8_t)(s >= 1u && s <= 9u ? s : 5u);
    if (!(r[1] & 4u))
        return;
    for (i = 0; i < KT_NKEY; i++) {
        uint32_t b = 12u * i, w = (uint32_t)(r[2 + b / 8u] | r[3 + b / 8u] << 8) >> (b % 8u), fl;
        fl = KT_DBASE + 10u * (w & 63u);
        kt_ends(&kt_key[i], fl, fl + KT_SPAN + 10u * ((w >> 6) & 63u));
    }
    kt_calibrated = 1;
}
static void kt_save(uint8_t *r)
{
    uint32_t i;
    memset(r, 0, 32);
    r[0] = KT_TAG;
    r[1] = (uint8_t)(kt_mode | (kt_calibrated ? 4u : 0u) | (uint32_t)kt_sens << 4);
    if (!kt_calibrated)
        return;
    for (i = 0; i < KT_NKEY; i++) {
        int32_t f = ((int32_t)kt_key[i].floor_ - (int32_t)KT_DBASE + 5) / 10, c, fl;
        uint32_t b = 12u * i, w;
        f = f < 0 ? 0 : f > 63 ? 63 : f;
        fl = (int32_t)KT_DBASE + 10 * f;
        c = ((int32_t)kt_key[i].ceil - fl - (int32_t)KT_SPAN + 5) / 10;
        c = c < 0 ? 0 : c > 63 ? 63 : c;
        w = ((uint32_t)f | (uint32_t)c << 6) << (b % 8u);
        r[2 + b / 8u] |= (uint8_t)w;
        r[3 + b / 8u] |= (uint8_t)(w >> 8);
    }
}

/* ------------------------------------------------------------------ calibration (ui_touch.c) */
static void kt_cal_begin(void)
{
    memset(kt_cal_n, 0, sizeof kt_cal_n);
    kt_cal_phase = 1;
}
/* how many presses phase ph (1 gentle, 2 firm) still wants of key k */
static uint32_t kt_cal_left(uint32_t ph, uint32_t k) { return KT_CAL_N - kt_cal_n[ph - 1u][k]; }
static uint32_t kt_cal_median(const uint16_t *v)
{
    uint32_t a = v[0], b = v[1], c = v[2];
    return a > b ? (b > c ? b : a > c ? c : a) : (a > c ? a : b > c ? c : b);
}
/* the presses in: every key's ends from the medians (a key with too few keeps its own), calibrated */
static void kt_cal_finish(void)
{
    uint32_t k;
    kt_cal_phase = 0;
    for (k = 0; k < KT_NKEY; k++) {
        uint32_t fl = kt_key[k].floor_, cl = kt_key[k].ceil;
        if (kt_cal_n[0][k] == KT_CAL_N)
            cl = kt_cal_median(kt_cal_v[0][k]);
        if (kt_cal_n[1][k] == KT_CAL_N)
            fl = kt_cal_median(kt_cal_v[1][k]);
        kt_ends(&kt_key[k], fl, cl);
    }
    kt_calibrated = 1;
}
