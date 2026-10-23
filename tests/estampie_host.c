/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE's host simulator for the screen reader (fm1-reader: skill/fm1-screen-reader): the browser emulator's
 * firmware (web/emu/felucca_web.c), built natively, run from a script. Each line one command:
 *   wait MS                 run the FM-1 for MS milliseconds
 *   press B / hold B / release B    a button by its printed label (FX SCL ENV LFO EDIT GLO HOME SAVE ARP SEQ PLAY
 *                           REC OCT- OCT+): a tap is 80 ms down and 80 ms up
 *   turn K N                a knob (SELECT ALGO PRESETS K1 K2 K3 K4) N detents (negative: back)
 *   key N / keydown N / keyup N     key N (0 = F3 .. 26 = G5)
 *   listen / bye / where    a reader's HELLO (then again every 2 s), BYE, WHERE
 *   said TEXT               a SAY since the last check holds TEXT (capitals or not), else the script fails
 *   quiet                   no SAY since the last check, else the script fails
 *   print                   the SAYs since the last check
 *   speakdump               every screen, page, knob value, menu row, dialog, layer, NAME and hint, as KIND<TAB>text
 *   messages FILE           each line of FILE as a message on the screen, said as the FM-1 says it: MSG (or WARN)
 *   colour NAME             MENU > COLOUR set to the palette NAME (VELLUM, TAVERN ..)
 *   shot FILE               the screen as it is now, a 240 x 240 PPM (in OUTDIR, if given)
 * ESTAMPIE_SAID_LOG=FILE appends every SAY to FILE as MSG<TAB>text (WARN: ANN_KEEP). Exit 1 if a check failed.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -o build/host/estampie_host tests/estampie_host.c -lm
 *   build/host/estampie_host SCRIPT [OUTDIR] */
#include <stdio.h>
static void host_said(const unsigned char *p, unsigned n);
#define WEB_SAID_HOOK(p, n) host_said((p), (n))
#include "../web/emu/felucca_web.c"
#include <ctype.h>

static FILE *said_log;
static char said_since[16384];             /* the SAYs since the last check, a line each */
static uint32_t listening, hello_t, failed;

static void host_said(const unsigned char *p, unsigned n)
{
    char t[ANN_MAX + 2];
    uint32_t k = 0, i;
    if (n < 8u || p[0] != 0xF0u || p[1] != ANN_H0 || p[4] != ANN_SAY)
        return;
    for (i = 6; i + 1u < n && k < ANN_MAX; i++)
        t[k++] = (char)p[i];
    t[k] = 0;
    if (said_log)
        fprintf(said_log, "%s\t%s\n", (p[5] & ANN_KEEP) ? "WARN" : "MSG", t);
    if (strlen(said_since) + k + 2u < sizeof said_since) {
        strcat(said_since, t);
        strcat(said_since, "\n");
    }
}
static void reader(uint8_t cmd)
{
    uint8_t f[] = {0xF0, ANN_H0, ANN_H1, ANN_H2, cmd, 1, 0xF7};
    memcpy(web_sysex_in(), f, sizeof f);
    web_sysex(cmd == ANN_HELLO ? 7u : 6u);
}
static void run(uint32_t ms)
{
    while (ms--) {
        web_ms();
        if (listening && fm1_ms - hello_t >= 2000u) {   /* a reader says HELLO every 2 s */
            hello_t = fm1_ms;
            reader(ANN_HELLO);
        }
        web_said_clear();
    }
}

static const char *const BTN[NB] = {"FX", "SCL", "ENV", "LFO", "EDIT", "GLO", "HOME", "SAVE", "ARP", "SEQ", "PLAY",
                                    "REC", "OCT-", "OCT+"};
static const char *const ENC[NE] = {"SELECT", "ALGO", "PRESETS", "K1", "K2", "K3", "K4"};
static int find(const char *const *names, uint32_t n, const char *s)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        if (!strcmp(names[i], s))
            return (int)i;
    return -1;
}
static int has(const char *hay, const char *needle)
{
    size_t i, j, n = strlen(needle);
    for (i = 0; hay[i]; i++) {
        for (j = 0; j < n && hay[i + j] && tolower((unsigned char)hay[i + j]) == tolower((unsigned char)needle[j]); j++)
            ;
        if (j == n)
            return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ speakdump */
static void out(const char *kind, const char *s)
{
    if (s[0])
        printf("%s\t%s\n", kind, s);
}
static void draw_now(void)
{
    sp.drawn = 0;
    ui.force = 1;
    ui_draw();
}
/* the cards drawn, one a line, and each card's every value (a name list) or its ends (a number) */
static void dump_cards(int values)
{
    uint32_t c;
    char b[SPK_N];
    for (c = 0; c < 4u; c++) {
        if (!((sp.drawn >> c) & 1u))
            continue;
        b[0] = 0;
        sp_clean(b, sp.card[c], 0);
        out("KNOB", b);
    }
    if (!values || ui.home)
        return;
    for (c = 0; c < 4u; c++) {
        int16_t *vp;
        const param_desc_t *d = page_desc(cur_page(), c, &vp);
        int32_t v, step;
        if (!d || !d->label || d->max <= d->min)
            continue;
        step = d->fmt == F_ENUM ? 1 : (d->max - d->min) / 4 ? (d->max - d->min) / 4 : 1;
        for (v = d->min; v <= d->max; v += step) {
            char val[16];
            const char *unit = "";
            param_format(d, (int16_t)v, val, &unit);
            sp_card_put(c, d->label, val, unit);
            b[0] = 0;
            sp_clean(b, sp.card[c], 0);
            out("KNOB", b);
        }
    }
}
static void speakdump(void)
{
    char b[SPK_N], one[2];
    uint32_t e, i, k, row, v;
    track_t *t = TSEL;
    ann.on = 1;                                        /* (sp_card_put takes the cards only for a reader) */
    for (e = 0; e < NENGINES; e++) {
        const engine_t *en = ENGINES[e];
        if (!eng_ok(e))
            continue;
        set_engine_of(t, e);
        for (k = 0; k < en->npresets; k++)             /* the sounds the PRESETS knob says */
            out("SOUND", en->presets[k].name);
        ui.home = 1;
        for (v = 0; v < HV_COUNT; v++) {
            ui_home_view = (uint8_t)v;
            draw_now();
            sp_screen(b, SP_HEAD | SP_TAIL);
            out("SCREEN", b);
            dump_cards(0);
        }
        ui_home_view = HV_SCOPE;
        ui.home = 0;
        for (i = 0; i < NPAGES; i++) {
            if (!page_visible(i))
                continue;
            ui.page = (uint8_t)i;
            page_entered();
            draw_now();
            sp_screen(b, 0);
            out("SCREEN", b);
            dump_cards(1);
            draw_now();
            ui.help_t = 1;                             /* MENU > HELP ON: the hint with it */
            sp_screen(b, 0);
            out("SCREEN", b);
            ui.help_t = 0;
        }
    }
    set_engine_of(t, ENGI_GURDY);
    ui.home = 1;
    for (row = 0; row < MI_COUNT; row++) {             /* MENU: every row, every value */
        uint32_t n = menu_valued(row) ? menu_n(row) : 1u;
        ui.menu = 1;
        ui.menu_sel = (uint8_t)row;
        sp_screen(b, 0);
        out("SCREEN", b);
        for (v = 0; v < n; v++) {
            uint32_t was = menu_get(row);
            if (menu_valued(row))
                menu_put(row, v);
            b[0] = 0;
            sp_menu_row(b, row);
            out("ROW", b);
            if (menu_valued(row))
                menu_put(row, was);
        }
    }
    for (ui.menu = 2; ui.menu <= 3; ui.menu++) {
        sp_screen(b, 0);
        out("SCREEN", b);
    }
    ui.menu = 0;
    for (k = 1; k <= CF_CLEAN_SOUND; k++) {            /* the dialogs */
        ui.confirm = (uint8_t)k;
        ui.confirm_trk = 0;
        sp_screen(b, 0);
        out("ASK", b);
    }
    ui.confirm = 0;
    for (k = 1; k < LAYER_N; k++) {                    /* the layers */
        ui.layer = (uint8_t)k;
        draw_now();
        sp_screen(b, 0);
        out("SCREEN", b);
    }
    ui.layer = 0;
    for (k = NK_USER_SAVE; k <= NK_PROJ_RENAME; k++) {   /* NAME: empty, then a name, and each character */
        nm.kind = (uint8_t)k;
        nm.len = 0;
        nm.s[0] = 0;
        str_cpy(nm.ph, k <= NK_USER_RENAME ? "GURDY 1" : "PROJECT A", sizeof nm.ph);
        sp_screen(b, 0);
        out("SCREEN", b);
        str_cpy(nm.s, "DANCE 2", sizeof nm.s);
        nm.len = 7;
        sp_screen(b, 0);
        out("SCREEN", b);
    }
    for (k = 0; NM_SET[k]; k++)
        out("CHAR", sp_char(NM_SET[k], one));
    nm.kind = NK_NONE;
    for (k = 0; k < NELEM(HELP_PAGE); k++) {           /* every hint, as HELP ON says it */
        b[0] = 0;
        sp_clean(b, HELP_PAGE[k].hint, 1);
        out("HINT", b);
    }
    {
        static const char *const MORE[] = {H_HOME, H_LEVELS, H_GRID, H_EUCLID, "[K1]TEMPO[GLO]HOLD: TAP"};
        for (k = 0; k < NELEM(MORE); k++) {
            b[0] = 0;
            sp_clean(b, MORE[k], 1);
            out("HINT", b);
        }
        for (k = 1; k < LAYER_N; k++) {
            b[0] = 0;
            sp_clean(b, HELP_LAYER[k], 1);
            out("HINT", b);
        }
    }
    ui.uboot = 3;
    sp_screen(b, 0);
    out("SCREEN", b);
    ui.uboot = 0;
    for (k = 0; k < 2u; k++) {
        song.playing = (uint8_t)k;
        out("MSG", k ? "PLAY" : "STOP");
    }
    song.playing = 0;
    ann.on = 0;
}

/* ------------------------------------------------------------------ the script */
int main(int argc, char **argv)
{
    FILE *f;
    char line[256];
    uint32_t held = 0, keys = 0, n = 0;
    const char *log = getenv("ESTAMPIE_SAID_LOG");
    if (argc < 2 || !(f = fopen(argv[1], "r"))) {
        fprintf(stderr, "usage: estampie_host SCRIPT [OUTDIR]\n");
        return 2;
    }
    if (log && !(said_log = fopen(log, "a")))
        return 2;
    web_nor_erase();
    web_boot();
    run(1200);
    while (fgets(line, sizeof line, f)) {
        char cmd[32] = "", arg[200] = "";
        int a2 = 0, b;
        n++;
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#')
            continue;
        sscanf(line, "%31s %199[^\n]", cmd, arg);
        if (!strcmp(cmd, "wait")) {
            run((uint32_t)atoi(arg));
        } else if (!strcmp(cmd, "press") || !strcmp(cmd, "hold") || !strcmp(cmd, "release")) {
            if ((b = find(BTN, NB, arg)) < 0) {
                fprintf(stderr, "host: line %u: no button %s\n", n, arg);
                return 2;
            }
            if (cmd[0] != 'r')
                held |= 1u << b;
            web_buttons(held);
            run(80);
            if (cmd[0] != 'h') {
                held &= ~(1u << b);
                web_buttons(held);
                run(80);
            }
        } else if (!strcmp(cmd, "turn")) {
            char k[16] = "";
            sscanf(arg, "%15s %d", k, &a2);
            if ((b = find(ENC, NE, k)) < 0) {
                fprintf(stderr, "host: line %u: no knob %s\n", n, k);
                return 2;
            }
            while (a2) {                              /* a detent at a time, as a hand turns it */
                web_enc((uint32_t)b, a2 > 0 ? 1 : -1);
                a2 -= a2 > 0 ? 1 : -1;
                run(30);
            }
        } else if (!strcmp(cmd, "key") || !strcmp(cmd, "keydown") || !strcmp(cmd, "keyup")) {
            uint32_t m = 1u << (atoi(arg) % 27);
            if (strcmp(cmd, "keyup"))
                keys |= m;
            web_keys(keys);
            run(80);
            if (strcmp(cmd, "keydown")) {
                keys &= ~m;
                web_keys(keys);
                run(80);
            }
        } else if (!strcmp(cmd, "listen")) {
            listening = 1;
            hello_t = fm1_ms;
            reader(ANN_HELLO);
            run(50);
        } else if (!strcmp(cmd, "bye")) {
            listening = 0;
            reader(ANN_BYE);
            run(50);
        } else if (!strcmp(cmd, "where")) {
            reader(ANN_WHERE);
            run(50);
        } else if (!strcmp(cmd, "said") || !strcmp(cmd, "quiet")) {
            int ok = cmd[0] == 's' ? has(said_since, arg) : !said_since[0];
            if (!ok) {
                printf("host: line %u: FAIL %s %s; said:\n%s", n, cmd, arg, said_since[0] ? said_since : "(nothing)\n");
                failed++;
            }
            said_since[0] = 0;
        } else if (!strcmp(cmd, "print")) {
            printf("%s", said_since);
            said_since[0] = 0;
        } else if (!strcmp(cmd, "speakdump")) {
            speakdump();
        } else if (!strcmp(cmd, "messages")) {         /* (tools/speak_audit.sh: the sources' messages) */
            FILE *m = fopen(arg, "r");
            char ml[128], b[SPK_N];
            while (m && fgets(ml, sizeof ml, m)) {
                ml[strcspn(ml, "\r\n")] = 0;
                b[0] = 0;
                sp_clean(b, ml, 0);
                out(sp_warn(ml) ? "WARN" : "MSG", b);
            }
            if (m)
                fclose(m);
        } else if (!strcmp(cmd, "colour")) {
            uint32_t i;
            for (i = 0; i < palette_n() && strcasecmp(palette_name(i), arg); i++)
                ;
            if (i == palette_n()) {
                fprintf(stderr, "host: line %u: no palette %s\n", n, arg);
                return 2;
            }
            menu_put(MI_COLOR, i);
            run(100);
        } else if (!strcmp(cmd, "shot")) {             /* RGB565 as the panel gets it, high byte first */
            char path[512];
            FILE *o;
            uint32_t i;
            snprintf(path, sizeof path, "%s%s%s", argc > 2 ? argv[2] : "", argc > 2 ? "/" : "", arg);
            if (!(o = fopen(path, "wb")))
                return 2;
            fprintf(o, "P6\n240 240\n255\n");
            for (i = 0; i < 240u * 240u; i++) {
                uint32_t v = ((web_fb[i] & 0xFFu) << 8) | (web_fb[i] >> 8);
                fputc((int)(((v >> 11) & 31u) * 255u / 31u), o);
                fputc((int)(((v >> 5) & 63u) * 255u / 63u), o);
                fputc((int)((v & 31u) * 255u / 31u), o);
            }
            fclose(o);
        } else {
            fprintf(stderr, "host: line %u: what is %s?\n", n, cmd);
            return 2;
        }
    }
    fclose(f);
    if (said_log)
        fclose(said_log);
    printf("host: %s: %s\n", argv[1], failed ? "FAILED" : "ok");
    return failed ? 1 : 0;
}
