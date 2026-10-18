/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE: KEY TOUCH (firmware/src/key_touch.c) on synthetic presses, shaped as Velo measured them (notes/01-lab.md
 * in github.com/charlesvestal/fm1-velocity): a pulled-up reading falls from 1023 through the touch to the diode's ~190
 * once seated; a driven one reads the diode (185) plus the pill, lower the harder the key is pressed.
 * 1. the log2 approximation within 0.015 octave (its own 0.008 and Q8's rounding: a 1/70 of a semitone);
 * 2. velocity: 127 at the floor and below, 1 at the ceiling and above, rising the harder the landing, the middle of
 *    the range (in log) where the 0.7 curve puts it, 1 + 126 x 0.5^0.7 = 78;
 * 3. the state machine: a press lands after KT_WAIT pulled samples, its velocity from the lowest driven reading; a
 *    graze (gone before seating) never lands; a release after two samples above RELEASE, not one; samples alternate
 *    driven once touched (kt_want_drive), and a held key's column LEDs stay dark (kt_led_ok);
 * 4. the gate: a measured key waits for its landing and plays with its velocity; one that never lands plays at SELECT
 *    VEL after KT_GATE_MS; a top-row key never waits; OFF, or not live (the emulator), never waits;
 * 5. aftertouch: 0 resting, rising as the key is pressed past its rest, back to 0 eased off; AT SENS more sensitive
 *    gives more for the same lean (eased off it fades through the ~30 ms smoothing);
 * 6. the settings row: saved and loaded back within its 10-count steps; another row's bytes (not KT_TAG): OFF;
 * 7. calibration: the medians of three gentle (the ceiling) and three firm (the floor) landings per key. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "../firmware/src/key_touch.c"

static int bad;
static void check(const char *what, int ok)
{
    printf("key touch: %-86s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

/* one press of key k: the pulled-up curve (touch at 700, seated 190 from the second sample), every other sample
 * driven at `force` (a driven reading); n pulled samples, then released over `up` samples */
static uint32_t now;
static void press(uint32_t k, uint32_t force, uint32_t n)
{
    static const uint16_t fall[] = {700, 300, 190};
    uint32_t i;
    for (i = 0; i < n; i++) {
        kt_sample(k, i < 3u ? fall[i] : 190u, 0);
        if (kt_want_drive(k))
            kt_sample(k, force, 1);
        now += 2;
    }
}
static void lift(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < 4u; i++)
        kt_sample(k, i ? 1023u : 650u, 0);
}

int main(void)
{
    uint32_t x, k;
    double err = 0;
    for (x = 1; x < 5000u; x++) {
        double e = fabs(kt_log2q8(x) / 256.0 - log2((double)x));
        if (e > err)
            err = e;
    }
    check("1. log2 in Q8 within 0.015 octave", err < 0.015);

    kt_init();
    {
        uint8_t a = kt_velocity(450, 450, 896), b = kt_velocity(896, 450, 896), c = kt_velocity(300, 450, 896);
        uint8_t m = kt_velocity(185 + (uint32_t)sqrt(265.0 * 711.0), 450, 896), up = 1;
        uint32_t d;
        for (d = 460; d < 896; d += 10)
            up &= kt_velocity(d, 450, 896) <= kt_velocity(d - 10, 450, 896);
        check("2. velocity: 127 at the floor and harder, 1 at the ceiling", a == 127 && c == 127 && b == 1);
        check("   softer landings never louder; the log middle 78 (the 0.7 curve)", up && m >= 76 && m <= 80);
    }

    kt_init();
    kt_mode = KT_VEL;
    press(3, 500, 6);
    check("3. a press lands, its velocity from the driven reading",
          kt_key[3].st == KT_S_DOWN && kt_key[3].land == 1 && kt_key[3].dland == 500);
    kt_sample(3, 190, 0);
    check("   held: after a pulled sample the next driven, its column's LEDs dark", kt_want_drive(3) && !kt_led_ok(3) && kt_led_ok(4));
    kt_sample(3, 500, 1);
    kt_sample(3, 1023, 0);
    check("   one sample open is not a release", kt_key[3].st != KT_S_OPEN);
    lift(3);
    check("   two open: released, nothing driven, LEDs again", kt_key[3].st == KT_S_OPEN && !kt_want_drive(3) && kt_led_ok(3));
    kt_sample(5, 700, 0);
    kt_sample(5, 1023, 0);
    check("   a graze (touched, gone before seating) never lands", kt_key[5].st == KT_S_OPEN && kt_key[5].land == 0);
    press(6, 300, 6);
    press(7, 800, 6);
    check("   a firmer landing plays louder",
          kt_velocity(kt_key[6].dland, kt_key[6].floor_, kt_key[6].ceil) > kt_velocity(kt_key[7].dland, kt_key[7].floor_, kt_key[7].ceil));

    kt_init();
    kt_mode = KT_VEL;
    kt_live = 1;
    memset(kt_seen, 0, sizeof kt_seen);
    {
        uint32_t cur, prev = 0, t = 1000;
        cur = kt_gate(1u << 2 | 1u << 22, prev, t);
        check("4. a measured key waits for its landing; a top-row key plays at once",
              !((cur >> 2) & 1u) && ((cur >> 22) & 1u) && kt_kvel[22] == 0);
        prev = cur;
        press(2, 400, 6);
        cur = kt_gate(1u << 2 | 1u << 22, prev, t + 4);
        check("   landed: it plays, with its velocity", ((cur >> 2) & 1u) && kt_kvel[2] == kt_velocity(400, 450, 896));
        prev = cur;
        cur = kt_gate(prev | 1u << 9, prev, t + 10);
        check("   one that never lands waits ...", !((cur >> 9) & 1u));
        cur = kt_gate(prev | 1u << 9, prev, t + 10 + KT_GATE_MS);
        check("   ... then plays at SELECT VEL after KT_GATE_MS", ((cur >> 9) & 1u) && kt_kvel[9] == 0);
        kt_live = 0;
        check("   not live (the emulator): never waits", kt_gate(1u << 11, 0, t) == 1u << 11);
        kt_live = 1;
        kt_mode = KT_OFF;
        check("   OFF: never waits", kt_gate(1u << 12, 0, t) == 1u << 12);
    }

    kt_init();
    kt_mode = KT_VELAT;
    kt_sens = 5;
    {
        uint32_t t = 0, i;
        uint8_t rest, firm, eased, firm9;
        press(4, 600, 6);
        for (i = 0; i < 80u; i++) {                /* resting at 600 for 400 ms */
            kt_key[4].d = 600;
            rest = kt_lean(4, t += 5);
        }
        for (i = 0; i < 20u; i++) {                /* leaning: 400 */
            kt_key[4].d = 400;
            firm = kt_lean(4, t += 5);
        }
        for (i = 0; i < 20u; i++) {                /* eased back: 100 ms */
            kt_key[4].d = 640;
            eased = kt_lean(4, t += 5);
        }
        check("5. aftertouch: 0 resting, up leaning past the rest, nearly gone 100 ms eased off",
              rest == 0 && firm > 60 && eased <= 4);
        for (i = 0; i < 40u; i++) {
            kt_key[4].d = 640;
            eased = kt_lean(4, t += 5);
        }
        check("   and gone 300 ms after", eased == 0);
        kt_sens = 9;
        for (i = 0; i < 20u; i++) {
            kt_key[4].d = 560;
            firm9 = kt_lean(4, t += 5);
        }
        kt_sens = 1;
        for (i = 0; i < 20u; i++) {
            kt_key[4].d = 640;
            (void)kt_lean(4, t += 5);
        }
        for (i = 0; i < 20u; i++) {
            kt_key[4].d = 560;
            firm = kt_lean(4, t += 5);
        }
        check("   AT SENS 9 gives more than 1 for the same lean", firm9 > firm);
    }

    kt_init();
    {
        uint8_t row[32], other[32];
        uint32_t ok = 1;
        for (k = 0; k < KT_NKEY; k++)
            kt_ends(&kt_key[k], 300 + 13 * k, 700 + 11 * k);
        kt_mode = KT_VELAT;
        kt_sens = 7;
        kt_calibrated = 1;
        kt_save(row);
        kt_init();
        kt_mode = KT_OFF;
        kt_load(row);
        for (k = 0; k < KT_NKEY; k++)
            ok &= abs((int)kt_key[k].floor_ - (int)(300 + 13 * k)) <= 5 && abs((int)kt_key[k].ceil - (int)(700 + 11 * k)) <= 10;
        check("6. the settings row: the mode, AT SENS and every key's ends back (10-count steps)",
              ok && kt_mode == KT_VELAT && kt_sens == 7 && kt_calibrated);
        memset(other, 0xA5, sizeof other);
        kt_load(other);
        check("   a row that is not KEY TOUCH's: OFF, AT SENS 5, the defaults",
              kt_mode == KT_OFF && kt_sens == 5 && !kt_calibrated && kt_key[0].floor_ == KT_FLOOR0);
    }

    kt_init();
    kt_mode = KT_VEL;
    kt_cal_begin();
    for (k = 0; k < KT_NKEY; k++) {
        press(k, 800 - k, 6), lift(k);
        press(k, 850 - k, 6), lift(k);
        press(k, 820 - k, 6), lift(k);
    }
    check("7. calibration: three gentle presses of every key taken", kt_cal_left(1, 0) == 0 && kt_cal_left(1, 19) == 0);
    kt_cal_phase = 2;
    for (k = 0; k < KT_NKEY; k++) {
        press(k, 320 + k, 6), lift(k);
        press(k, 360 + k, 6), lift(k);
        press(k, 340 + k, 6), lift(k);
    }
    kt_cal_finish();
    check("   each key's ceiling the gentle median, its floor the firm median",
          kt_key[0].ceil == 820 && kt_key[0].floor_ == 340 && kt_key[19].ceil == 801 && kt_key[19].floor_ == 359 &&
          kt_calibrated && !kt_cal_phase);

    printf(bad ? "KEY TOUCH TEST FAILED (%d)\n" : "key touch test passed\n", bad);
    return bad ? 1 : 0;
}
