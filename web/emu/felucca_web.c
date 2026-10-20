/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * The design (the device clock driven by the audio the worklet asks for, the exports below, the saved
 * flash handed back before boot) follows X0X's browser build, web/emu/x0x_web.c of charlesvestal/fm1-x0x
 * (GPL-3.0), a Felucca fork. */
/* Felucca in the browser: the firmware's sources, as the host tests build them (tests/hostsim.c: libc,
 * engines, voices, FX, sequencer; then the UI, main.c, storage and projects, in src/felucca.c's order),
 * compiled to WebAssembly and run in an AudioWorklet against a simulated FM-1:
 *   - the device clock: TIMER4 ticks (24 MHz) and fm1_ms advance with the audio the worklet asks for; the
 *     audio ISR (audio.c fm1_alnk0_irq, the real one) renders each I2S half (HALF_FRAMES) as the DMA would
 *     ask for it, and the main loop (main.c's, below in web_frame) runs every 15 ms, with ui_input on every
 *     millisecond in between as main.c's wait loop calls it;
 *   - the panel: buttons and encoders by their printed labels (panel.c's table maps them to the matrix),
 *     the keys as fm1_in.notes, the LEDs read back from fm1_led / fm1_led_dim through the key matrix;
 *   - the screen: lcd_fill / lcd_blit into a 240 x 240 RGB565 frame (the panel's byte order);
 *   - the flash: a 1 MiB NOR image behind storage.c's hooks (settings, projects, user presets, the user
 *     sample slots read through it as XIP); the page keeps its storage sectors and gives them back at boot.
 * Not here: USB (MIDI, audio, the editor, updates), TRS MIDI, the serial console, the battery (full), the
 * crash / UBOOT paths. Nothing of the firmware changes for this file.
 *   web/emu/build.sh  ->  build/emu/{felucca.wasm, worklet.js, index.html} */
#include <stdint.h>

#define FELUCCA_WEB 1
#ifndef FELUCCA_CDC
#define FELUCCA_CDC 1                    /* as src/felucca.c: MENU > USB SERIAL is there (nothing behind it) */
#endif
#define FELUCCA_FLASH 1
#define FELUCCA_OTA 0
#define FELUCCA_UAC 0
#define FELUCCA_UART 0
#ifndef FELUCCA_VERSION
#define FELUCCA_VERSION "WEB"
#endif

static uint8_t nor[0x100000];            /* the SPI NOR (erased: 0xFF) */
#define SMP_USER_XIP(k) ((const uint8_t *)nor + SMP_USER_BASE + (k) * SMP_USER_SIZE)   /* eng_sample.c */
#define main hostsim_main
#include "../../tests/hostsim.c"
#undef main

/* ------------------------------------------------------------ the clock --- */
#define FM1_TICKS_PER_US 24u             /* TIMER4: 24 MHz */
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* ------------------------------------------------------ audio (ALNK0) --- */
static uint32_t web_half;                /* the half the next render fills */
#define FM1_AUDIO_HALF 1u
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return web_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{ (void)buf; (void)words; (void)isr; (void)prio; }
static void fm1_audio_stop(void) {}
void isr_alnk0(void) {}
#include "../../firmware/src/audio.c"

/* ------------------------------------------------------------ the panel --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h: key id at (column, row bit) */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL], fm1_led_breath[FM1_NCOL], fm1_led_mid[FM1_NCOL];
static uint32_t web_dim_lo;
static void fm1_led_dim_level(uint32_t lo) { web_dim_lo = lo; }
/* ESTAMPIE: KEY TOUCH's calibration screen (ui_input.c touch_setup) lights a key (no keys are measured here; the
 * converter's switch: hostsim.c) */
static void fm1_led_key(uint32_t id, int on) { (void)id; (void)on; }
/* the power-on LED sweep (hal/fm1_input.h fm1_led_anim_start, the picture hal/fm1_led_anim.h): its frame from the
 * clock (a scan frame 1.1 ms), the page draws each LED's level (web_anim_levels); at its end, or on a key or a button
 * down, the last picture (the glow everywhere, or dark) and the UI's LEDs from the next frame, as the scan does */
#include "../../firmware/hal/fm1_led_anim.h"
static uint32_t web_anim_t0, web_anim_glow, web_anim_run, web_anim_idle;   /* (idle: 1.2's idle animation, MENU >
                                                                              * ANIM IDLE: fm1_idle_level, endless) */
static uint32_t web_anim_frame(void) { return (web_ticks - web_anim_t0) / (1100u * FM1_TICKS_PER_US); }
static void fm1_led_anim_start(uint32_t glow)
{
    web_anim_idle = 0;
    memset(fm1_led, 0, sizeof fm1_led);
    memset(fm1_led_dim, 0, sizeof fm1_led_dim);
    memset(fm1_led_breath, 0, sizeof fm1_led_breath);
    memset(fm1_led_mid, 0, sizeof fm1_led_mid);
    web_anim_t0 = web_ticks;
    web_anim_glow = glow != 0u;
    web_anim_run = 1;
}
static int fm1_led_anim_on(void)
{
    if (web_anim_run && web_anim_idle && (fm1_in.notes || fm1_in.buttons))
        web_anim_run = 0;                       /* (the idle animation: ended, the UI's LEDs from now) */
    if (web_anim_run && !web_anim_idle && (web_anim_frame() >= FM1_ANIM_FRAMES || fm1_in.notes || fm1_in.buttons)) {
        uint32_t c, r;
        web_anim_run = 0;
        for (c = 0; c < FM1_NCOL; c++)
            for (r = 1; r < 5u; r++)
                if (web_anim_glow && FM1_KEYMAP[r][c] >= 0)
                    fm1_led_dim[c] |= (uint8_t)(1u << r);
    }
    return (int)web_anim_run;
}
static void fm1_led_idle_start(uint32_t glow)
{
    fm1_led_anim_start(glow);
    web_anim_idle = 1;
}
static int fm1_led_idle_on(void) { return fm1_led_anim_on() && web_anim_idle; }
static void fm1_led_anim_stop(void) { web_anim_run = 0; }
static uint32_t web_pressed, web_released, web_notes_pressed;
static int32_t web_enc_steps[7];
static uint32_t fm1_input_edges(uint32_t *released)
{
    uint32_t p = web_pressed;
    if (released)
        *released = web_released;
    web_pressed = web_released = 0;
    return p;
}
static uint32_t fm1_input_note_edges(void) { uint32_t n = web_notes_pressed; web_notes_pressed = 0; return n; }
static int32_t fm1_enc_take(uint32_t e) { int32_t s = web_enc_steps[e % 7u]; web_enc_steps[e % 7u] = 0; return s; }
static void fm1_input_init(void) {}
static void fm1_input_tick(void) {}
static void fm1_wdt_feed(void) {}
static void fm1_irq_off(void) {}
static void fm1_irq_on(void) {}

/* ----------------------------------------------------------- the screen --- */
static uint16_t web_fb[240 * 240];       /* RGB565 as the panel gets it (high byte first) */
static uint32_t web_draws;
static void lcd_fill(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint16_t c)
{
    uint32_t i, j;
    uint16_t sw = (uint16_t)((c >> 8) | (c << 8));
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = sw;
    web_draws++;
}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    uint32_t i, j;
    for (j = 0; j < h && y + j < 240u; j++)
        for (i = 0; i < w && x + i < 240u; i++)
            web_fb[(y + j) * 240u + x + i] = p[j * w + i];
    web_draws++;
}
static void lcd_sync(void) {}
static void lcd_power(uint32_t s)              /* MENU > SCREEN OFF (lcd.c): off, the page dark; on: redrawn before */
{
    if (!s) {
        memset(web_fb, 0, sizeof web_fb);
        web_draws++;
    }
}
static void lcd_wake_now(void) {}
static void lcd_init(void) {}

#include "../../firmware/src/gfx.c"
#include "../../firmware/src/panel.c"
/* ESTAMPIE: the screen reader (ui.c announce.c, ui_speak.c): its SysEx frames, F0..F7 each, kept for the page
 * (web_said_buf / web_said_len, then web_said_clear); a frame that does not fit is not sent, as on the device with
 * a full USB queue. WEB_SAID_HOOK: the host simulator's log of every SAY (tests/estampie_host.c) */
static uint8_t web_said[4096];
static uint32_t web_said_n;
static int web_said_put(const uint8_t *p, uint32_t n)
{
#ifdef WEB_SAID_HOOK
    WEB_SAID_HOOK(p, n);
#endif
    if (web_said_n + n > sizeof web_said)
        return -1;
    memcpy(web_said + web_said_n, p, n);
    web_said_n += n;
    return 0;
}
#define ANN_WIRE web_said_put
#include "../../firmware/src/ui.c"
#include "../../firmware/src/menu_items.c"
#include "../../firmware/src/icons.c"
#include "../../firmware/src/ui_graph.c"
#include "../../firmware/src/ui_draw.c"
#include "../../firmware/src/ui_menu.c"
#include "../../firmware/src/ui_input.c"
#include "../../firmware/src/ui_layer.c"

/* ------------------------------------------------------------ the flash --- */
static uint32_t web_flash_writes;
static uint8_t flash_ok;
static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off >= sizeof nor || n > sizeof nor - off)
        return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (off >= sizeof nor || 4096u > sizeof nor - off)
        return -8;
    memset(nor + (off & ~4095u), 0xFF, 4096);
    web_flash_writes++;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)       /* NOR: a write only clears bits */
{
    const uint8_t *s = src;
    uint32_t i;
    if (off >= sizeof nor || n > sizeof nor - off)
        return -8;
    for (i = 0; i < n; i++)
        nor[off + i] &= s[i];
    web_flash_writes++;
    return 0;
}
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
static uint32_t fl_jedec_ram(void) { return 0x856014u; }             /* the FM-1's part (project.c) */
static void fl_plain_window_init(void) {}
#define FL_FAR(fn) (fn)
#include "../../firmware/src/storage.c"
#include "../../firmware/src/upreset.c"
#include "../../firmware/src/project.c"

/* ------------------------------------------------- main.c and its HAL --- */
#if FELUCCA_CPU2
typedef struct { uint32_t magic, count, vec, pc, emu, dbg, rets, core; } fm1_crash_t;
#else
typedef struct { uint32_t magic, count, vec, pc, emu, dbg, rets; } fm1_crash_t;
#endif
#define FM1_CRASH_MAGIC 0x43525348u
static fm1_crash_t fm1_crash;                     /* (none: main.c boot_leds runs the sweep) */
static struct { uint8_t p3_rst, wdt_con; uint32_t rst_src; } fm1_boot;
uint32_t _data_start[1], _data_end[1], _data_load[1], _bss_start[1], _bss_end[1];
uint32_t _pool_start[1], _pool_end[1], _rt_start[1], _rt_end[1], _rt_load[1];
static uint32_t web_master_adc = 700;   /* MASTER, 0..1023 as the ADC reads it */
enum { FM1_ADC_BATT = 3, FM1_ADC_MASTER = 4 };
static int32_t fm1_adc_read(uint32_t ch) { return ch == FM1_ADC_MASTER ? (int32_t)web_master_adc : 620; }
static void fm1_adc_init(void) {}
static void fm1_timer5_ack(void) {}
static void fm1_timer5_start(void (*isr)(void), uint32_t prio) { (void)isr; (void)prio; }
void isr_timer5(void) {}
static void fm1_reboot(void) {}
static void fm1_enter_uboot(void) {}
static void fm1_guard_lock_top(void) {}
static void fm1_irq_enable_all(void) {}
static void fm1_irq_init(void) {}
static void fm1_time_init(void) {}
static void fm1_reset_reason(void) {}
static void fm1_wdt_arm(uint32_t v) { (void)v; }
static void fm1_mailbox_clear(void) {}
static void fm1_guard_enable(uint32_t m) { (void)m; }
enum { FM1_GUARD_STACK = 1, FM1_GUARD_WRITE = 2, FM1_GUARD_BUS = 4, FM1_GUARD_PC = 8 };
#if FELUCCA_CPU2
/* the second core (hal/fm1_cpu1.h): none here, one core whatever MENU > 2ND CORE says (fx.c mix_block's C1_ON() is 0
 * without felucca.c's macros) */
uint32_t _c1_start[1], _c1_end[1], _c1_load[1];
static uint8_t fm1_c1_on, fm1_c1_gave_up;
static void fm1_cpu1_halt(void) {}
static int fm1_cpu1_start(void) { return -1; }
static void fm1_guard_pc_open(void) {}
#endif
#if FELUCCA_CDC
static void cdc_task(void) {}
#endif
/* main.c's calls into the USB driver (registers): nothing here (usb.c's own functions stay as they are) */
#define usb_poll() ((void)0)
#define usb_start() ((void)0)
#define usb_retry(ms) ((void)(ms))
#define usb_detach() ((void)0)
#include "../../firmware/src/master.c"
#include "../../firmware/src/main.c"
#undef usb_poll
#undef usb_start
#undef usb_retry
#undef usb_detach

/* --------------------------------------------------------- the device --- */
static uint32_t web_booted, web_boot_ms, web_last_frame;

/* main.c fm1_main's boot, up to the main loop (no USB, no UART, no panel setup) */
static void web_power_on(void)
{
    persist_boot();
    settings_init();
    usb_serial_apply();
    lcd_init();
    draw_splash();                        /* main.c: the splash (ui_draw.c) */
    memset(&felucca_dbg, 0, sizeof felucca_dbg);
    felucca_dbg.magic = DBG_MAGIC;
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    felucca_init();
    master_boot();                        /* main.c (#137): the pot's level from the first block */
    kb_boot_hold = 1;                     /* main.c (#137): the keys silent under the splash */
    audio_init();
    boot_leds();                          /* main.c: the power-on LED sweep, from the scan's start */
    autosave_boot(boot_clean);            /* main.c: the last session's music (1.2, project.c) */
    web_boot_ms = fm1_ms + 430u;         /* main.c: 30 + 400 ms before the first frame */
    web_booted = 1;
}

/* one pass of main.c fm1_main's loop (what the browser has: no USB, editor, UBOOT, console) */
static void web_frame(void)
{
    int32_t b = fm1_adc_read(FM1_ADC_BATT);
    if (b > 0)
        song.batt_raw = song.batt_raw ? song.batt_raw + (b - song.batt_raw) / 32 : b;
    master_poll();
    felucca_dbg.ui_frames++;
    ui_input();
    settings_poll();
    autosave_poll();
    ui_leds();
    ui_draw();
    ui_speak(fm1_ms);                     /* ESTAMPIE: the screen reader (web_sysex in, web_said_buf out) */
}

/* output ring, stereo float, filled by web_ms, drained by web_render */
#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

/* one millisecond of the device: the clock, the audio halves due by now, the main loop */
static void web_ms(void)
{
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    web_samples_due += FS;
    while (web_samples_due >= 1000u * HALF_FRAMES) {      /* an I2S half: the audio ISR renders it */
        uint32_t k;
        const int32_t *o = &abuf[web_half * HALF_WORDS];
        web_samples_due -= 1000u * HALF_FRAMES;
        fm1_alnk0_irq();
        for (k = 0; k < HALF_FRAMES; k++) {
            uint32_t i = (ring_w++ % RING) * 2u;
            ring[i] = (float)o[2u * k] / 8388608.0f;
            ring[i + 1] = (float)o[2u * k + 1u] / 8388608.0f;
        }
        web_half ^= 1u;
    }
    if (!web_booted || (int32_t)(fm1_ms - web_boot_ms) < 0)
        return;
    if (web_boot_ms) {                                    /* main.c: the splash goes, the loop starts */
        web_boot_ms = 0;
        kb_boot_hold = 0;
        lcd_fill(0, 0, 240, 240, T_BG);
        web_last_frame = fm1_ms - 15u;
    }
    if (fm1_ms - web_last_frame >= 15u) {                /* main.c: a frame, then ui_input until 15 ms are over */
        web_last_frame = fm1_ms;
        web_frame();
    } else {
        ui_input();                                       /* main.c's wait loop */
    }
}

#define EXPORT __attribute__((used, visibility("default")))
#undef __attribute__

static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
/* n frames (<= 1024) into out_l / out_r: the device runs until they exist */
EXPORT void web_render(uint32_t n)
{
    uint32_t k;
    if (n > 1024u)
        n = 1024u;
    while (ring_w - ring_r < n)
        web_ms();
    for (k = 0; k < n; k++) {
        uint32_t i = (ring_r++ % RING) * 2u;
        out_l[k] = ring[i];
        out_r[k] = ring[i + 1];
    }
}

/* the flash: erased, then the page's saved sectors (web_nor), then boot */
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }
EXPORT void web_boot(void)
{
    if (!web_booted)
        web_power_on();
}

/* A portable emulator backup carries the live music separately from the flash sectors: unlike the automatic
 * restore object, it is exact even while playing or during the ten-second autosave quiet period. Import uses the
 * project format's existing checksum, range and compatibility validation before it touches the running music. */
static uint8_t web_project_in[3840];
EXPORT uint8_t *web_project_capture(void)
{
    project_capture(&proj_scratch);
    proj_wire_gen++;
    return proj_pack(&proj_wire, &proj_scratch) ? proj_wire_u.raw : 0;
}
EXPORT uint32_t web_project_size(void) { return sizeof(project_store_t); }
EXPORT uint8_t *web_project_import_buffer(void) { return web_project_in; }
EXPORT uint32_t web_project_import_capacity(void) { return sizeof web_project_in; }
EXPORT uint32_t web_project_validate(uint32_t n)
{
    return n <= sizeof web_project_in && proj_import(&proj_scratch, web_project_in, (int)n);
}
EXPORT uint32_t web_project_restore(uint32_t n)
{
    if (!web_project_validate(n))
        return 0;
    proj_bound(&proj_scratch);
    return project_restore_runtime(&proj_scratch) == 0;
}

/* input, by the printed labels: buttons bit b = B_FX .. B_OCTUP (panel.c), keys bit k = key k (F3 .. G5),
 * encoders by role (EN_SELECT .. EN_K4), MASTER 0..1023 */
static uint32_t web_btn_labels, web_key_mask;
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, old = fm1_in.buttons;
    web_btn_labels = labels;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    web_pressed |= m & ~old;
    web_released |= old & ~m;
    fm1_in.buttons = m;
}
EXPORT void web_keys(uint32_t mask)
{
    uint32_t old = fm1_in.notes;
    mask &= (1u << 27) - 1u;
    web_key_mask = mask;
    web_notes_pressed |= mask & ~old;
    fm1_in.notes = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        web_enc_steps[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }

/* ESTAMPIE: the sound lab (web/emu/lab.html) sets a track's sound and steps directly, between audio blocks (this build
 * is one thread). web_track_engine: as a sound load's engine change (ui.c set_engine_of: its first preset, the fade);
 * web_track_set: parameter id of track k, clamped to its range as the track's engine names it (PHYS's per-model
 * controls too); returns the value kept. web_track_step: step i one note (0 a rest) at velocity vel; web_transport:
 * 1 start, 2 stop, 3 restart (seq.c transport_req) */
EXPORT void web_track_engine(uint32_t k, uint32_t e)
{
    if (k < NTRK && e < NENGINES && eng_ok(e))
        set_engine_of(&trk[k], e);
}
EXPORT int32_t web_track_set(uint32_t k, uint32_t id, int32_t v)
{
    const param_desc_t *d;
    if (k >= NTRK || id >= P_COUNT)
        return 0;
    d = track_desc(&trk[k], id);
    if (d->max > d->min)
        trk[k].p[id] = (int16_t)clamp(v, d->min, d->max);
    ui.force = 1;
    return trk[k].p[id];
}
static uint8_t web_vel_own[NTRK][NSTEP], web_vel_kept[NTRK];   /* (web_track_vel, below) */
EXPORT void web_track_step(uint32_t k, uint32_t i, uint32_t note, uint32_t vel)
{
    step_t *s;
    if (k >= NTRK || i >= NSTEP)
        return;
    s = &trk[k].step[i];
    web_vel_kept[k] = 0;
    memset(s, 0, sizeof *s);
    s->note[0] = (uint8_t)(note & 127u);
    s->n = note ? 1 : 0;
    s->time = note ? ST_NOTE : ST_REST;
    s->vel = (uint8_t)(note ? (vel ? vel & 127u : 96u) : 0u);
}
/* the lab's touch, as KEY TOUCH or a MIDI controller gives it: web_track_vel, every note step of track k at
 * velocity v (1..127), or 0 their own again (kept at the first change; a part or a step loaded forgets them);
 * web_track_touch, its aftertouch and mod wheel (0..127: mod.c AT and MODW) */
EXPORT void web_track_vel(uint32_t k, uint32_t v)
{
    uint32_t i;
    if (k >= NTRK || v > 127u)
        return;
    if (!v) {
        if (web_vel_kept[k])
            for (i = 0; i < NSTEP; i++)
                if (trk[k].step[i].n)
                    trk[k].step[i].vel = web_vel_own[k][i];
        web_vel_kept[k] = 0;
        return;
    }
    if (!web_vel_kept[k]) {
        for (i = 0; i < NSTEP; i++)
            web_vel_own[k][i] = trk[k].step[i].vel;
        web_vel_kept[k] = 1;
    }
    for (i = 0; i < NSTEP; i++)
        if (trk[k].step[i].n)
            trk[k].step[i].vel = (uint8_t)v;
}
EXPORT void web_track_touch(uint32_t k, uint32_t at, uint32_t mw)
{
    if (k >= NTRK)
        return;
    trk[k].at = (uint8_t)(at > 127u ? 127u : at);
    trk[k].mw = (uint8_t)(mw > 127u ? 127u : mw);
}
/* ESTAMPIE: the screen reader, for the page: a reader's SysEx in (web_sysex_in, n bytes, F0..F7 or what is between
 * them: HELLO every 2 s, BYE, WHERE; 1 if it was the reader's), and the device's frames out */
static uint8_t web_sx_in[64];
EXPORT uint8_t *web_sysex_in(void) { return web_sx_in; }
EXPORT uint32_t web_sysex(uint32_t n)
{
    const uint8_t *p = web_sx_in;
    if (n > sizeof web_sx_in)
        return 0;
    if (n && p[0] == 0xF0u) {
        p++;
        n--;
    }
    if (n && p[n - 1u] == 0xF7u)
        n--;
    return (uint32_t)ann_frame(p, n, fm1_ms);
}
EXPORT uint8_t *web_said_buf(void) { return web_said; }
EXPORT uint32_t web_said_len(void) { return web_said_n; }
EXPORT void web_said_clear(void) { web_said_n = 0; }
EXPORT void web_transport(uint32_t req) { if (req >= 1u && req <= 3u) transport_req = (uint8_t)req; }
EXPORT int32_t web_track_get(uint32_t k, uint32_t id) { return k < NTRK && id < P_COUNT ? trk[k].p[id] : 0; }
EXPORT uint32_t web_track_engine_of(uint32_t k) { return k < NTRK ? trk[k].eng_req : 0u; }
/* parameter id's descriptor as track k's engine names it: format, range, label, and value i's name (0 = none) */
static const param_desc_t *web_desc(uint32_t k, uint32_t id) { return track_desc(&trk[k % NTRK], id % P_COUNT); }
EXPORT uint32_t web_desc_fmt(uint32_t k, uint32_t id) { return web_desc(k, id)->fmt; }
EXPORT int32_t web_desc_min(uint32_t k, uint32_t id) { return web_desc(k, id)->min; }
EXPORT int32_t web_desc_max(uint32_t k, uint32_t id) { return web_desc(k, id)->max; }
EXPORT const char *web_desc_label(uint32_t k, uint32_t id) { return web_desc(k, id)->label; }
/* value v of parameter id as the device shows it (params.c param_format: "55 %", "4.3 ms", an enum's name) */
static char web_fmt_buf[24];
EXPORT const char *web_desc_format(uint32_t k, uint32_t id, int32_t v)
{
    char val[16];
    const char *unit = 0;
    uint32_t n;
    memset(val, 0, sizeof val);
    param_format(web_desc(k, id), v, val, &unit);
    n = (uint32_t)strlen(val);
    memcpy(web_fmt_buf, val, n);
    if (unit && *unit && n + 1u + strlen(unit) < sizeof web_fmt_buf) {
        web_fmt_buf[n++] = ' ';
        memcpy(web_fmt_buf + n, unit, strlen(unit));
        n += (uint32_t)strlen(unit);
    }
    web_fmt_buf[n] = 0;
    return web_fmt_buf;
}
EXPORT const char *web_engine_name(uint32_t e) { return e < NENGINES && eng_ok(e) ? ENGINES[e]->name : 0; }
EXPORT uint32_t web_engine_count(void) { return NENGINES; }
/* power-on scene s (engines.c GB_SCENES, 0 = TROTTO) with its sounds and phrases, as at power-on */
EXPORT void web_scene(uint32_t s) { gb_scene_load(s); }
/* part k of the TROTTO alone: its sound, phrase and level, as gb_scene_load sets them */
EXPORT void web_scene_part(uint32_t k)
{
    const uint8_t *p;
    track_t *t;
    if (k >= NPART)
        return;
    p = GB_SCENES[0].part[k];
    t = &trk[k];
    web_vel_kept[k] = 0;
    set_engine_of(t, p[0]);
    apply_preset_to(t, p[1]);
    es_phrase_load(t, p[2] - ES_PHR(0));
    t->p[P_LEVEL] = GB_SCENES[0].lvl[k] ? GB_SCENES[0].lvl[k] : TP[P_LEVEL].def;
    ui.force = 1;
}
/* the lab's parameters by number (lab.html PIDS: the same order), so the page never holds P_* values of its own */
static const uint8_t WEB_PIDS[] = {P_LEVEL, P_ATK, P_DEC, P_SUS, P_REL, P_ED_FLT, P_VOICE, P_PAN, P_MUTE, P_DIST,
                                   P_CHOR, P_DLY, P_REV, P_SLEN, P_SDIV, P_TEMP, P_RING, P_E0, P_M1SRC, P_M1DST,
                                   P_M1AMT, P_M2SRC, P_M2DST, P_M2AMT, P_M3SRC, P_M3DST, P_M3AMT, P_M4SRC, P_M4DST,
                                   P_M4AMT};
EXPORT int32_t web_pid(uint32_t n) { return n < sizeof WEB_PIDS ? WEB_PIDS[n] : -1; }
/* a channel message from Web MIDI, as a USB-MIDI packet arrives (usb.c midi_enqueue): 0 = the ring was full */
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    if (status < 0x80u || status >= 0xF0u)
        return 1;
    return (uint32_t)midi_enqueue((status >> 4) | status << 8 | (d1 & 127u) << 16 | (d2 & 127u) << 24, 1);
}

/* the LEDs: bit b = the button labelled b lit (bit 14: PLAY's green), keys bit k; the dim glow and the breath alike */
static uint32_t led_on(const uint8_t *l, uint32_t id)
{
    uint32_t c, r;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][c] == (int8_t)id)
                return (l[c] >> r) & 1u;
    return 0;
}
static uint32_t led_buttons(const uint8_t *l)
{
    uint32_t b, m = 0;
    for (b = 0; b < NB; b++)
        m |= led_on(l, panel.btn[b]) << b;
    return m;
}
static uint32_t led_keys(const uint8_t *l)
{
    uint32_t k, m = 0;
    for (k = 0; k < 27u; k++)
        m |= led_on(l, 14u + k) << k;
    return m;
}

/* The hardware lights the key that was physically pressed; in the browser, also light the visible keys of the
 * chord that press generated. Keep this overlay here rather than in ui_input.c so physical FM-1 behaviour is
 * unchanged. kb_chord / kb_chord2 contain the notes actually started, so MONO, kits, voicings and notes clipped at
 * the MIDI limits need no special cases. As play_leds does, use the lowest key when quantisation maps several keys
 * to the same note, and omit chord tones outside the drawn keyboard. */
static uint32_t web_note_key(const track_t *t, uint32_t note)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (kb_map(t, k) == note)
            return 1u << k;
    return 0;
}
static uint32_t web_chord_keys(void)
{
    uint32_t held = web_key_mask & ~kb_layer, k, i, m = 0;
    if (keys_own())
        return 0;                                      /* do not draw over a layer, grid, NAME or SLICES map */
    for (k = 0; k < 27u; k++) {
        if (!((held >> k) & 1u))
            continue;
        if (kb_chn[k] > 1u)
            for (i = 0; i < kb_chn[k]; i++)
                m |= web_note_key(&trk[kb_trk[k] % NTRK], kb_chord[k][i]);
        if (kb_trk2[k] && kb_chn2[k] > 1u)
            for (i = 0; i < kb_chn2[k]; i++)
                m |= web_note_key(&trk[(kb_trk2[k] - 1u) % NTRK], kb_chord2[k][i]);
    }
    return m;
}
EXPORT uint32_t web_lit_buttons(void)
{
    return led_buttons(fm1_led) | ((fm1_led[LED_PLAY_GREEN >> 3] >> (LED_PLAY_GREEN & 7u)) & 1u) << 14;
}
EXPORT uint32_t web_lit_keys(void) { return led_keys(fm1_led) | web_chord_keys(); }
EXPORT uint32_t web_dim_buttons(void) { return led_buttons(fm1_led_dim); }
EXPORT uint32_t web_dim_keys(void) { return led_keys(fm1_led_dim); }
EXPORT uint32_t web_dim_level(void) { return web_dim_lo; }
EXPORT uint32_t web_breath_buttons(void) { return led_buttons(fm1_led_breath); }   /* #119: dark .. ~60 % of lit */
EXPORT uint32_t web_breath_keys(void) { return led_keys(fm1_led_breath); }
EXPORT uint32_t web_mid_keys(void) { return led_keys(fm1_led_mid); }   /* the DRUM grid's beats: a steady mid level */
/* the power-on sweep (or 1.2's idle animation) running: each LED's level now (hal/fm1_led_anim.h: 0..64 /64 of lit, | 0x80 the glow under
 * it), 0..13 the buttons by label, 14..40 the keys F3..G5; 0 when it is not */
EXPORT uint8_t *web_anim_levels(void)
{
    static uint8_t lv[FM1_ANIM_NBTN + FM1_ANIM_NKEY];
    uint32_t f = web_anim_frame(), i;
    if (!fm1_led_anim_on())
        return 0;
    for (i = 0; i < FM1_ANIM_NBTN + FM1_ANIM_NKEY; i++)
        lv[i] = (uint8_t)(web_anim_idle ? fm1_idle_level : fm1_anim_level)(f, i < FM1_ANIM_NBTN ? (i < NB ? panel.btn[i] : i) : i,
                                                                          web_anim_glow);
    return lv;
}

/* for the test and the bench (web/emu/emu_test.mjs), a heavy song straight into the tracks (as hostsim's
 * renders set them): T1 FM6 PAD, T2 PHYS DRONE STRING (SYMP), T3 GRAIN CLOUD PAD, 4-note chords on all 16
 * steps, held; T4 DRUM, kick snare and hats. PLAY is the page's (or the test's) to press */
EXPORT void web_test_heavy(void)
{
    static const uint8_t ENG[3][2] = {{12, 4}, {ENGI_PHYS, 7}, {8, 0}};
    static const uint8_t CH[4][4] = {{48, 55, 60, 64}, {45, 52, 57, 60}, {41, 48, 53, 57}, {43, 50, 55, 59}};
    uint32_t p, i;
    for (p = 0; p < 3u; p++) {
        track_t *t = &trk[p];
        host_preset(t, ENG[p][0], ENG[p][1]);
        t->p[P_VOICE] = V_POLY;
        t->p[P_SUS] = 127;
        for (i = 0; i < 16u; i++) {
            uint8_t n[4], k;
            for (k = 0; k < 4u; k++)
                n[k] = (uint8_t)(CH[i / 4u][k] + 12u * p);
            put_step(t, i, 4, n, ST_NOTE, 0);
        }
    }
    host_drums(&trk[3]);
    for (i = 0; i < 16u; i++) {
        uint8_t n[4];
        uint32_t k = 0;
        if (i % 4u == 0u)
            n[k++] = 36;
        if (i == 4u || i == 12u)
            n[k++] = 38;
        n[k++] = i % 2u ? 42 : 46;
        put_step(&trk[3], i, k, n, ST_NOTE, 0);
    }
    ui.force = 1;
}

EXPORT uint32_t web_test_user_preset(uint32_t i) { return (uint32_t)up_used(i); }

EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_playing(void) { return song.playing; }
EXPORT uint32_t web_sample_rate(void) { return FS; }
