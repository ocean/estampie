/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot guard (firmware/src/bootguard.h, after zvenson/dinghy): a boot that dies in its first 30 s counts; the
 * second in a row starts the USB RESCUE (no audio, the updater only); a reset in the rescue falls back to the ROM's
 * UBOOT; a power-on starts the count again; OCT- alone held at power-on is the rescue too (OCT- + OCT+: the panel
 * setup, as before); REC alone, the ROM's download mode. The guard lives in .noinit RAM: here a plain struct, its "garbage" a pattern. */
#include <stdio.h>
#include <string.h>
#include "../firmware/src/bootguard.h"

static int bad;
static void check(const char *what, int ok)
{
    printf("bootguard: %-80s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static bootguard_t g;
/* one boot: the reset's P3 source, then what bootguard_begin says */
static uint32_t boot(uint32_t p3) { bootguard_reset(&g, p3); return bootguard_begin(&g); }
static void survives(void) { g.pending = 0; g.failed = 0; }   /* main.c: 30 s up */

int main(void)
{
    uint32_t m;
    memset(&g, 0xA5, sizeof g);                          /* RAM after a power cut: anything */
    m = boot(1u);
    check("first power-on (RAM garbage): a normal boot, pending", m == BOOT_NORMAL && g.pending == 1u && g.magic == BOOTGUARD_MAGIC);
    survives();
    check("30 s up: a later reset boots normally", boot(4u) == BOOT_NORMAL);

    survives();
    m = boot(4u);                                        /* a WDT reset, then another in the first 30 s */
    check("one failed boot (a reset in its first 30 s): still normal", m == BOOT_NORMAL && boot(4u) == BOOT_NORMAL && g.failed == 1u);
    m = boot(0x40u);                                     /* .. and a third: the second failure in a row */
    check("two failed boots in a row: the USB RESCUE", m == BOOT_RECOVERY && g.pending == 2u);
    m = boot(4u);                                        /* the rescue itself reset */
    check("a reset inside the rescue: the ROM's UBOOT, the guard cleared", m == BOOT_ROM && g.pending == 0u && g.failed == 0u);
    check("  after UBOOT: a normal boot", boot(4u) == BOOT_NORMAL);

    survives();
    boot(4u); boot(4u);                                  /* one failure counted */
    m = boot(1u);                                        /* switched off and on: the count starts again */
    check("a power-on (P3 bit 0, no WDT / soft reset) clears the count: normal", m == BOOT_NORMAL && g.failed == 0u);
    boot(1u | 4u);                                       /* power-on flag with a WDT: still a failed boot */
    check("power-on flag together with a WDT reset: counts as a failure", g.failed == 1u);

    survives();
    g.pending = 0; g.failed = 0;
    m = boot(4u);
    g.pending = 0;                                       /* main.c: an intentional reset (update, UBOOT) */
    check("an intentional reset (pending cleared) is no failure", boot(4u) == BOOT_NORMAL && g.failed == 0u);

    g.failed = 7; g.pending = 9;                         /* damaged values */
    check("damaged counters: cleared, a normal boot", boot(4u) == BOOT_NORMAL && g.failed == 0u && g.pending == 1u);

    check("OCT- alone held (raw bit 0): the rescue", bootguard_manual(1u) && bootguard_manual(1u | 0x10u));
    check("OCT- + OCT+ (the panel setup), OCT+ alone, nothing: not the rescue",
          !bootguard_manual(3u) && !bootguard_manual(2u) && !bootguard_manual(0u));
    check("REC alone held (raw bit 13): the ROM's download mode", bootguard_download(1u << 13));
    check("REC with anything else, OCT- alone, nothing: not download mode",
          !bootguard_download(1u << 13 | 1u) && !bootguard_download(1u << 13 | 1u << 12) && !bootguard_download(1u)
          && !bootguard_download(0u) && !bootguard_manual(1u << 13));

    printf("%s\n", bad ? "BOOTGUARD TEST FAILED" : "bootguard test passed");
    return bad != 0;
}
