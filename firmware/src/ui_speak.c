/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE: what the screen shows, for a screen reader on the computer (fm1-reader: its page, or the computer's own
 * reader). ui.c includes announce.c, the protocol; this says what changed, once a UI frame (ui_speak, from the main
 * loop), in the screen's own text: the firmware's words file (firmware/words.tsv, published on the site) tells the
 * reader how to say it ("ATK 12 ms": "attack 12 milliseconds"). Nothing is built while no reader listens.
 *   everything (a reader starts, or asks WHERE): the track and its sound, the screen, its knobs, play or stop, the BPM
 *   the screen changed (a page, the menu, a dialog, a layer, NAME, the track): its name and its knobs, once it settles
 *   a knob turned: its card ("CUT 64"), once it settles; other cards a press or a turn changed with it
 *   MENU: the row moved to, or its value, with its place ("SCREEN OFF NEVER, 6 of 13")
 *   messages: as they appear (a warning, ANN_KEEP: heard whole); PLAY and STOP; the sound the PRESETS knob picks
 *   NAME: each letter typed, then the name
 * The cards' text comes from draw_column (sp_card_put), so every page says what it draws; the rest from the
 * functions that draw it (page_title, confirm_text, help_page, menu_vname). Its RAM is in the pool, its text built
 * there (sp_buf), never on the stack or in the audio interrupt. */
#define SPK_N (ANN_MAX + 1u)
static struct {
    char buf[SPK_N], tmp[SPK_N];
    char card[4][40];                      /* this frame's cards, as drawn (sp_card_put) */
    char said[4][40];                      /* .. as last said or taken in */
    char msg[24], sound[20], name[NM_LEN + 1u];
    uint8_t drawn;                         /* bit c: card c drawn this frame */
    uint8_t started, msg_t, play, row, menu_v, bpm_t;
    int16_t bpm;
    uint32_t ctx, trk_eng;
} sp __attribute__((section(".pool")));

/* ------------------------------------------------------------------ text */
static int sp_starts(const char *s, const char *p)
{
    while (*p)
        if (*s++ != *p++)
            return 0;
    return 1;
}
static void sp_cat(char *b, const char *s)
{
    uint32_t n = str_len(b);
    while (*s && n + 1u < SPK_N)
        b[n++] = *s++;
    b[n] = 0;
}
static void sp_num(char *b, int32_t v)
{
    char d[12];
    fmt_int(d, v);
    sp_cat(b, d);
}
/* a keycap's label in words: a knob, the keys, or a button ("[GLO]": "GLO BUTTON") */
static void sp_key(char *b, const char *l)
{
    sp_cat(b, l);
    if (l[0] == 'K' && l[1] >= '1' && l[1] <= '4')
        return;                            /* K1, K1-4: the words file says "knob 1", "knobs 1 to 4" */
    if (str_eq(l, "KEYS"))
        return;
    sp_cat(b, str_eq(l, "PRESETS") || str_eq(l, "SELECT") || str_eq(l, "ALGO") || str_eq(l, "MASTER") ? " KNOB" : " BUTTON");
}
/* s as the reader takes it: icons and other bytes out, keycaps in words ("[SAVE] HOLD TO UNDO": "SAVE BUTTON: HOLD TO
 * UNDO"; a hint's "[K1]SLOT[K2]SOURCE": "K1: SLOT, K2: SOURCE"); hint: its pairs apart with commas */
static void sp_clean(char *b, const char *s, int hint)
{
    uint32_t n, pairs = 0;
    while (*s) {
        int32_t id = kc_tag(s, &n);
        if (id >= 0) {
            uint32_t k = str_len(b);
            if (pairs && hint)
                sp_cat(b, ", ");
            else if (k && b[k - 1u] != ' ')
                sp_cat(b, " ");
            sp_key(b, KC[id].label);
            s += n;
            while (*s == ' ')
                s++;
            if (*s)
                sp_cat(b, ": ");
            pairs++;
            continue;
        }
        if (*s >= ' ' && *s < 127) {
            char c[2] = {*s, 0};
            sp_cat(b, c);
        }
        s++;
    }
}
/* a name's character in words (NAME: its letters are spelled, its signs said) */
static const char *sp_char(char c, char *one)
{
    static const char SIGN[] = " -._/#+", *const WORD[] = {"SPACE", "DASH", "DOT", "UNDERSCORE", "SLASH", "HASH", "PLUS"};
    uint32_t i;
    for (i = 0; SIGN[i]; i++)
        if (c == SIGN[i])
            return WORD[i];
    one[0] = c;
    one[1] = 0;
    return one;
}
static void sp_spell(char *b, const char *s)
{
    char one[2];
    for (; *s; s++) {
        if (str_len(b) && b[str_len(b) - 1u] != ' ')
            sp_cat(b, " ");
        sp_cat(b, sp_char(*s, one));
    }
}

/* draw_column: card c's text this frame */
static void sp_card_put(uint32_t c, const char *label, const char *val, const char *unit)
{
    char *b;
    if (c >= 4u || !ann_on())
        return;
    b = sp.card[c];
    b[0] = 0;
    if (((ui.home && home_levels()) || ui.layer) && str_eq(label, "LEVEL")) {   /* HOME's levels, GLO's: the track's
                                                        * (its number is an icon) */
        str_cpy(b, "T1 ", 4);
        b[1] = (char)('1' + c);
    }
    str_cpy(b + str_len(b), label, 40 - str_len(b));
    if (str_eq(val, "--") && !ui.home && ((act_cols() >> c) & 1u))
        val = "";                                      /* (an action page: an action not picked, its name alone) */
    else if (str_eq(val, "--") || str_eq(val, "-"))  /* (nothing there, or none) */
        val = "NONE";
    if (val[0]) {
        if (b[0])
            str_cpy(b + str_len(b), val[0] == '.' ? " 0" : " ", 40 - str_len(b));   /* (".41 Hz": 0.41) */
        str_cpy(b + str_len(b), val, 40 - str_len(b));
    }
    if (unit[0] && !str_eq(unit, label)) {
        if (unit[0] != '/')                            /* ("1" "/36": "1/36", one of 36) */
            str_cpy(b + str_len(b), " ", 40 - str_len(b));
        str_cpy(b + str_len(b), unit, 40 - str_len(b));
    }
    sp.drawn |= (uint8_t)(1u << c);
}
/* the cards drawn, joined: "CUT 64, RES 20" (and the cards taken in as said) */
static void sp_cards(char *b)
{
    uint32_t c;
    for (c = 0; c < 4u; c++) {
        char t[40];
        t[0] = 0;
        if ((sp.drawn >> c) & 1u)
            sp_clean(t, sp.card[c], 0);
        str_cpy(sp.said[c], sp.card[c], sizeof sp.said[c]);
        if (!t[0])
            continue;
        sp_cat(b, str_len(b) && b[str_len(b) - 1u] != ' ' ? ", " : "");
        sp_cat(b, t);
    }
}

/* ------------------------------------------------------------------ what is shown */
static void sp_menu_row(char *b, uint32_t row)
{
    uint32_t t = MI_TAB[row % MI_COUNT], first = mtab_first(t);
    sp_cat(b, MI_NAME[row]);
    sp_cat(b, " ");
    if (row == MI_LCLEAR) {                            /* LEARN CLEAR: how many CCs are learned */
        if (ml_count()) {
            sp_num(b, (int32_t)ml_count());
            sp_cat(b, " CC");
        } else {
            sp_cat(b, "NONE");
        }
    } else if (row >= MI_VALUES) {                     /* CALIBRATION, INFO, ABOUT: OCT+ opens them */
        sp_cat(b, "OPENS WITH OCT+");
    } else if (row == MI_HOLD) {
        sp_cat(b, "0.");
        sp_num(b, (int32_t)(HOLD_MS[settings_hold % 4u] / 100u));
        sp_cat(b, " s");
    } else {
        sp_cat(b, menu_vname(row, menu_get(row)));
        if (row == MI_TUNE)
            sp_cat(b, " ct");
    }
    sp_cat(b, ", ");
    sp_num(b, (int32_t)(row - first + 1u));
    sp_cat(b, " of ");
    sp_num(b, (int32_t)mtab_rows(t));
}
/* the screen: its name, then what is on it. full: SP_HEAD the track and its sound first, SP_TAIL the steps that
 * are on, play or stop and the BPM after (WHERE: both) */
#define SP_HEAD 1
#define SP_TAIL 2
static void sp_screen(char *b, int full)
{
    char a[24], c[24];
    b[0] = 0;
    if ((full & SP_HEAD) && !ui.menu && !ui.uboot) {
        const engine_t *e = ENGINES[TSEL->eng_req % NENGINES];
        char pn[16];
        sound_name(TSEL, pn);
        sp_cat(b, "T");
        sp_num(b, (int32_t)song.sel + 1);
        sp_cat(b, " ");
        sp_cat(b, e->name);
        if (pn[0]) {
            sp_cat(b, ": ");
            sp_cat(b, pn);
        }
        sp_cat(b, ". ");
    }
    if (ui.uboot) {
        sp_cat(b, "UPDATE MODE IN ");
        sp_num(b, ui.uboot);
        sp_cat(b, ". LET GO OF OCT- AND OCT+ TO CANCEL");
    } else if (ui.menu == 1) {
        sp_cat(b, "MENU, ");
        sp_cat(b, MTAB_NAME[MI_TAB[ui.menu_sel % MI_COUNT] % MTAB_COUNT]);
        sp_cat(b, " TAB: ");
        sp_menu_row(b, ui.menu_sel);
    } else if (ui.menu == 2) {
        sp_cat(b, "ABOUT AND CREDITS. ESTAMPIE " ESTAMPIE_VERSION);
    } else if (ui.menu == 3) {
        static const char *const USB_N[5] = {"OFF", "WAITING", "BUS", "STARTING", "MIDI"};
        sp_cat(b, "INFO. FIRMWARE " FELUCCA_VERSION ", USB ");
        sp_cat(b, USB_N[info_usb()]);
        sp_cat(b, ", CPU ");
        sp_num(b, info.cpu);
        sp_cat(b, " %");
    } else if (ui.confirm) {
        confirm_text(a, c);
        sp_cat(b, a);
        if (c[0]) {
            sp_cat(b, " ");
            sp_cat(b, c);
            sp_cat(b, ".");
        }
        sp_cat(b, " OCT+ BUTTON: YES. OCT- BUTTON: NO.");
    } else if (seq_counting()) {
        sp_cat(b, "COUNT-IN");
    } else if (name_on()) {
        sp_cat(b, nm.kind == NK_USER_SAVE || nm.kind == NK_USER_RENAME ? "NAME THE SOUND. " : "NAME THE PROJECT. ");
        if (nm.len)
            sp_spell(b, nm.s);
        else {
            sp_cat(b, "EMPTY, SAVED AS ");
            sp_cat(b, nm.ph);
        }
    } else if (ui.layer) {
        const layer_t *ly = &LAYERS[ui.layer % LAYER_N];
        uint32_t k;
        sp_clean(b, layer_head(), 0);
        sp_cat(b, " LAYER");
        for (k = 0; k < 3u; k++)
            if (ly->foot[k].act && ly->foot[k].act[0]) {
                sp_cat(b, k ? ", " : ". ");
                sp_key(b, KC[ly->foot[k].key].label);
                sp_cat(b, ": ");
                sp_cat(b, ly->foot[k].act);
            }
        if (sp.drawn) {                                /* (a layer with knobs: SCL, SEQ TOOLS, the EDIT layer) */
            sp_cat(b, ". ");
            sp_cards(b);
        }
    } else {
        if (ui.home)
            sp_cat(b, "HOME");
        else {
            uint32_t k;
            page_title(a);                             /* "ENV 1/2": "ENV (1/2)", the reader's "page 1 of 2" */
            if (cur_page()->graph == GR_ROLL && !str_eq(a, cur_page()->title)) {   /* (the roll: its scale) */
                sp_cat(b, cur_page()->title);
                sp_cat(b, ", ");
            }
            for (k = str_len(a); k && a[k - 1u] != ' '; k--)
                ;
            if (k && a[k] >= '1' && a[k] <= '9' && a[k + 1u] == '/') {
                a[k - 1u] = 0;
                sp_cat(b, a);
                sp_cat(b, " (");
                sp_cat(b, a + k);
                sp_cat(b, ")");
            } else {
                sp_cat(b, a);
            }
        }
        sp_cat(b, ". ");
        sp_cards(b);
        if (act_cols()) {                              /* an action page: what OCT- does (its cards say OCT+'s) */
            foot_hint(a, c);                           /* ("OCT+ LOAD", "OCT- BACK") */
            sp_cat(b, ". OCT- BUTTON: ");
            sp_cat(b, c + 5);
        }
        if (ui.help_t && !ui.ml.on && help_page()) {  /* MENU > HELP ON: the page's hint, as shown on entering it */
            sp_cat(b, ". HELP: ");
            sp_clean(b, help_page(), 1);
        }
    }
    if ((full & SP_TAIL) && !ui.menu && !ui.uboot) {
        const track_t *t = TSEL;                       /* the footer's steps: those of the bank shown that are on */
        uint32_t i, j, on = 0, end = ui.bank * 16u + 16u;
        if (end > (uint32_t)t->p[P_SLEN])
            end = (uint32_t)t->p[P_SLEN];
        if (end > NSTEP)
            end = NSTEP;
        sp_cat(b, ". STEPS ON:");
        for (i = ui.bank * 16u; i < end; i = j) {     /* runs of three or more as "6-16": 6 to 16 */
            for (j = i; j < end && step_on(&seq_steps(t)[j]); j++)
                ;
            if (j == i) {
                j++;
                continue;
            }
            sp_cat(b, on++ ? ", " : " ");
            sp_num(b, (int32_t)i + 1);
            if (j - i >= 2u) {
                sp_cat(b, j - i == 2u ? ", " : "-");
                sp_num(b, (int32_t)j);
            }
        }
        if (!on)
            sp_cat(b, " NONE");
        sp_cat(b, ". ");
        sp_cat(b, song.playing ? "PLAYING, " : "STOPPED, ");
        sp_num(b, song.g[G_BPM]);
        sp_cat(b, " BPM");
    }
}
/* the screen as a context: changes when another screen, page, track or engine is shown */
static uint32_t sp_context(void)
{
    uint32_t h = ui.uboot * 3u + ui.menu * 7u + (ui.menu == 1 ? MI_TAB[ui.menu_sel % MI_COUNT] * 11u : 0u);
    h = h * 31u + ui.confirm * 13u + (uint32_t)seq_counting() * 17u + nm.kind * 19u + ui.layer * 23u;
    h = h * 31u + ui.home * 29u + (ui.home ? ui_home_view * 37u : ui.page * 41u) + song.sel * 43u;
    return h * 31u + (TSEL->eng_req % NENGINES) * 47u + (uint32_t)(grid_on() && !detail_on()) * 53u;
}
/* a message shown: a warning when it says something did not happen */
static int sp_warn(const char *m)
{
    static const char *const W[] = {"NO ", "NOT ", "FULL", "STOP TO", "FAIL", "ERROR", "CAN'T", "EMPTY", "TOO ", "OFF:"};
    uint32_t i, k;
    for (i = 0; i < NELEM(W); i++)
        for (k = 0; m[k]; k++)
            if (sp_starts(m + k, W[i]))
                return 1;
    return 0;
}

/* ------------------------------------------------------------------ once a UI frame (main loop) */
static void ui_speak(uint32_t now)
{
    uint32_t ctx, c, where, after = ANN_NOW, moved = 0;
    char one[2];
    if (!ann_on()) {
        sp.started = 0;
        sp.drawn = 0;
        ann_tick(now);
        return;
    }
    ctx = sp_context();
    where = (uint32_t)ann_where();
    if (ui.msg_t && (!str_eq(ui.msg, sp.msg) || ui.msg_t > sp.msg_t)) {   /* a message, as it appears */
        sp.buf[0] = 0;
        sp_clean(sp.buf, ui.msg, 0);
        if (sp.buf[0]) {
            ann_say(sp.buf, 0, ANN_NOW | (sp_warn(ui.msg) ? ANN_KEEP : 0u));
            after = 0;                                 /* (what it changed waits until it has been said) */
        }
    }
    str_cpy(sp.msg, ui.msg_t ? ui.msg : "", sizeof sp.msg);
    sp.msg_t = ui.msg_t;
    if (!sp.started || where) {                        /* a new reader, or WHERE: everything, now */
        sp_screen(sp.buf, SP_HEAD | SP_TAIL);
        ann_say(sp.buf, 0, ANN_NOW);
        sp.started = 1;
        sp.ctx = ctx;
    } else if (ctx != sp.ctx) {                        /* another screen: once it settles (another track or engine:
                                                        * its sound first) */
        uint32_t trk_eng = song.sel | (TSEL->eng_req % NENGINES) << 4;
        sp_screen(sp.buf, trk_eng != sp.trk_eng ? SP_HEAD : 0);
        ann_later(sp.buf, after, 200u, now);
        sp.ctx = ctx;
        moved = 1;
    } else if (ui.menu == 1 && (ui.menu_sel != sp.row || menu_get(ui.menu_sel) != sp.menu_v)) {
        sp.buf[0] = 0;                                 /* MENU: the row, or its value */
        sp_menu_row(sp.buf, ui.menu_sel);
        ann_later(sp.buf, after, 120u, now);
    } else if (name_on() && !str_eq(nm.s, sp.name)) {  /* NAME: the letter typed, else the name as it is */
        uint32_t n = str_len(nm.s);
        sp.buf[0] = 0;
        if (n > str_len(sp.name) && sp_starts(nm.s, sp.name))
            sp_cat(sp.buf, sp_char(nm.s[n - 1u], one));
        else if (n)
            sp_spell(sp.buf, nm.s);
        else
            sp_cat(sp.buf, "EMPTY");
        ann_later(sp.buf, after, 120u, now);
    } else if (!ui.menu && !ui.confirm && !name_on() && sp.drawn) {
        /* the knob turned: its card; with a press or a turn just now, the other cards it changed too (a card that
         * changes by itself, a value automation moves, is not said) */
        uint32_t hot = ui.hot_t ? ui.hot_col : 4u, input = now - scrn.idle < 400u;
        sp.buf[0] = 0;
        if (hot < 4u && ((sp.drawn >> hot) & 1u) && !str_eq(sp.card[hot], sp.said[hot])) {
            sp_clean(sp.buf, sp.card[hot], 0);
            str_cpy(sp.said[hot], sp.card[hot], sizeof sp.said[hot]);
        }
        for (c = 0; c < 4u; c++)
            if (((sp.drawn >> c) & 1u) && !str_eq(sp.card[c], sp.said[c])) {
                if (input) {
                    sp_cat(sp.buf, sp.buf[0] ? ", " : "");
                    sp_clean(sp.buf, sp.card[c], 0);
                }
                str_cpy(sp.said[c], sp.card[c], sizeof sp.said[c]);
            }
        if (sp.buf[0])
            ann_later(sp.buf, after, 120u, now);
    }
    if (!ui.menu && !name_on()) {                      /* the sound the PRESETS knob (or a load) picked */
        char pn[16];
        sound_name(TSEL, pn);
        if (!str_eq(pn, sp.sound) && !moved && sp.sound[0] && pn[0] && now - scrn.idle < 400u)
            ann_later(pn, after, 200u, now);
        str_cpy(sp.sound, pn, sizeof sp.sound);
    }
    if (sp.bpm != song.g[G_BPM] && ui.bpm_t) {         /* the BPM, turned */
        sp.buf[0] = 0;
        sp_num(sp.buf, song.g[G_BPM]);
        sp_cat(sp.buf, " BPM");
        ann_later(sp.buf, after, 120u, now);
    }
    sp.bpm = song.g[G_BPM];
    if (song.playing != sp.play) {                     /* PLAY, STOP (from the panel: now; from a clock: in turn) */
        ann_say(song.playing ? "PLAY" : "STOP", 0, now - scrn.idle < 400u ? ANN_NOW : 0u);
        sp.play = song.playing;
    }
    sp.trk_eng = song.sel | (TSEL->eng_req % NENGINES) << 4;
    sp.row = ui.menu_sel;
    sp.menu_v = (uint8_t)menu_get(ui.menu_sel);
    str_cpy(sp.name, nm.s, sizeof sp.name);
    sp.drawn = 0;
    ann_tick(now);
}

#if FELUCCA_OTA
/* the main loop, before the editor and the updater look: a screen reader's SysEx (announce.c) */
static void ui_speak_in(void)
{
    const uint8_t *p;
    uint32_t n;
    if (ota_frame_get(&p, &n) && ann_frame(p, n, fm1_ms))
        ota_frame_done();
}
#endif
