/* SPDX-License-Identifier: GPL-3.0-only */
/* The boot guard, in RAM only (.noinit: never the bootloader or the flash). Ported from zvenson/dinghy's
 * bootguard.h (itself from Felucca / SLOOP); Felucca's magic and layout, so a reset across the update keeps its
 * meaning. A boot is pending until it has run 30 s (main.c clears it then, and before an intentional reset: an
 * update, UBOOT); a boot that finds one pending counts a failure. The second failure in a row starts the USB RESCUE
 * (main.c rescue.c: no audio, the updater only); a reset in the rescue falls back to the ROM's UBOOT. A power-on (and
 * not a WDT or soft reset with it) starts the count again. It cannot rescue an image that fails before main.c. */
#ifndef GHOULBOX_BOOTGUARD_H
#define GHOULBOX_BOOTGUARD_H
#include <stdint.h>
#define BOOTGUARD_MAGIC 0x42475244u
typedef struct { uint32_t magic, failed, pending; } bootguard_t;
enum { BOOT_NORMAL, BOOT_RECOVERY, BOOT_ROM };
static void bootguard_clear(bootguard_t *b)
{
    b->magic = BOOTGUARD_MAGIC;
    b->failed = b->pending = 0;
}
/* p3: fm1_boot.p3_rst (fm1_sys.h: bit 0 power-on, 2 WDT, 6 soft) */
static void bootguard_reset(bootguard_t *b, uint32_t p3)
{
    if ((p3 & 1u) && !(p3 & (4u | 0x40u)))
        bootguard_clear(b);
}
static uint32_t bootguard_begin(bootguard_t *b)
{
    if (b->magic != BOOTGUARD_MAGIC || b->failed > 2u || b->pending > 2u)
        bootguard_clear(b);
    if (b->pending == 2u) {                       /* the rescue itself reset: the ROM */
        bootguard_clear(b);
        return BOOT_ROM;
    }
    if (b->pending && b->failed < 2u)
        b->failed++;
    b->pending = b->failed >= 2u ? 2u : 1u;
    return b->pending == 2u ? BOOT_RECOVERY : BOOT_NORMAL;
}
/* the raw buttons at power-on (fm1_in.buttons, before the panel table): OCT- (bit 0) alone; with OCT+ (bit 1) it is
 * the panel setup (main.c) */
static int bootguard_manual(uint32_t buttons) { return (buttons & 3u) == 1u; }
/* ESTAMPIE: REC (raw bit 13, panel.c's PANEL_DEFAULT) alone, nothing else: the ROM's download mode (rescue.c), the key
 * and the hold m-bootloader uses, so it means the same whichever bootloader is installed */
static int bootguard_download(uint32_t buttons) { return buttons == 1u << 13; }
#endif
