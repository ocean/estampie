/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE: the user guide's screenshots (docs/guide/*.png), in VELLUM, of what a fresh device shows: the boot screen,
 * then the power-on scene TROTTO (ui.c gb_scene_load, as main.c felucca_init loads it) on HOME, PRESETS, the step roll,
 * the MENU, and a GURDY's DRONE and RING pages. Drawn by the firmware's own UI code (tests/ui_render.c's host setup),
 * after GHOULBOX's tests/ghoulbox_shots.c.
 *   build/host/estampie_shots OUTDIR   -> OUTDIR/ppm/VELLUM_<name>.ppm (tools/guide_shots.sh: the PNGs)
 * Not a test. */
#define UI_RENDER_NO_MAIN
#include "ui_render.c"

static void scene(void)
{
    uint32_t i;
    ui_power_on();
    gb_scene_load(0);                              /* TROTTO */
    for (i = 0; i < NTRK; i++)
        trk[i].seq_idx = 5;
    song.batt_raw = 600;
    usb.config = 0;
    song.playing = 1;
    for (i = 0; i < SCOPE_N; i++)                  /* a bowed tone: a soft saw with its octave */
        scope_buf[i] = (int16_t)((fmod(i / 96.0, 1.0) * 2.0 - 1.0) * 7000.0 + sin(i * 0.1309) * 3000.0);
    scope_w = 0;
}

/* the page called name (params.c PAGES), on the selected track */
static void page_named(const char *name)
{
    uint32_t i;
    for (i = 0; i < NPAGES; i++)
        if (!strcmp(PAGES[i].title, name))
            break;
    ui.home = 0;
    ui.page = (uint8_t)i;
    page_entered();
}

static void shot(const char *dir, const char *name)
{
    uint32_t k;
    pal(UI_VELLUM_INDEX);                          /* (ui_power_on put the test's palette back) */
    ui.force = 1;
    for (k = 0; k < 8u; k++) { ui.hot_t = 0; ui.bpm_t = 0; ui_draw(); }
    write_ppm(dir, "VELLUM", name);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/guide_shots";
    char cmd[600];
    snprintf(cmd, sizeof cmd, "mkdir -p '%s/ppm'", dir);
    if (system(cmd))
        return 1;
    rep = fopen("/dev/null", "w");                 /* (ui_render's lint hooks report here) */
    pal(UI_VELLUM_INDEX);
    lcd_fill(0, 0, 240, 240, T_BG);                /* the boot screen, as main.c fm1_main draws it */
    draw_splash();
    write_ppm(dir, "VELLUM", "boot");
    scene(); ui.home = 1; shot(dir, "home");
    scene(); go_page(GR_BROWSE); shot(dir, "presets");
    scene(); go_page(GR_ROLL); ui.cursor = 4; shot(dir, "roll");
    scene(); ui.menu = 1; ui.menu_sel = 0; shot(dir, "menu");
    scene(); page_named("EDIT 1"); shot(dir, "drone");    /* track 1: TROTTO GURDY */
    scene(); trk[0].p[P_RING] = 1;
    page_named("RING"); shot(dir, "ring");
    return 0;
}
