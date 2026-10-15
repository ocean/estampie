/* SPDX-License-Identifier: GPL-3.0-only */
/* ESTAMPIE audition: a power-on scene (engines.c GB_SCENES) through the whole mix, as the device renders it (hostsim.c):
 * the scene's sounds, phrases or patterns, tempo, reverb and tape, set up as ui.c gb_scene_load does, then PLAY.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -o build/host/estampie_demo tests/estampie_demo.c -lm
 *   build/host/estampie_demo OUTDIR [SCENE] [SECONDS]   -> OUTDIR/scene_NAME.wav (default TROTTO, 24 s and the tail)
 *   SOLO=n (1..4): only part n's steps play               -> OUTDIR/scene_NAME_n.wav
 * Not a test: an ear check while the scenes and sounds are made. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static void scene(const char *dir, uint32_t s, uint32_t seconds, uint32_t solo)
{
    char path[512];
    FILE *f;
    uint32_t t, i, frames = (seconds + 4u) * FS;
    int32_t o[2 * CTL];
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    song.g[G_BPM] = GB_SCENES[s].bpm;
    song.g[G_RTYPE] = GB_SCENES[s].rtype;
    song.g[G_TAPE] = GB_SCENES[s].tape;
    rev_clear();
    hall_clear();
    fx.rtype = GB_SCENES[s].rtype;
    for (i = 0; i < NPART; i++) {
        const uint8_t *p = GB_SCENES[s].part[i];
        host_preset(&trk[i], p[0], p[1]);
        trk[i].p[P_SDIV] = 1;
        if (p[2] >= ES_PHR(0)) {
            es_phrase_load(&trk[i], p[2] - ES_PHR(0));
        } else if (p[2]) {                             /* as ui.c load_pat16 (not in the host build) */
            for (t = 0; t < 16u; t++) {
                uint8_t n = PATTERNS[p[2] - 1u].note[t], fl = PATTERNS[p[2] - 1u].flags[t];
                put_step(&trk[i], t, n ? 1u : 0u, &n, (fl & 4u) ? ST_TIE : n ? ST_NOTE : ST_REST, n ? fl & 3u : 0u);
                trk[i].step[t].vel = n ? 96 : 0;
            }
            trk[i].p[P_SLEN] = 16;
        }
        if (GB_SCENES[s].lvl[i])
            trk[i].p[P_LEVEL] = GB_SCENES[s].lvl[i];
    }
    if (solo)                                          /* SOLO=n: the other parts' steps emptied (their sounds stay) */
        for (i = 0; i < NPART; i++)
            if (i + 1u != solo)
                for (t = 0; t < NSTEP; t++)
                    memset(&trk[i].step[t], 0, sizeof trk[i].step[t]);
    if (solo)
        snprintf(path, sizeof path, "%s/scene_%s_%u.wav", dir, GB_SCENES[s].name, solo);
    else
        snprintf(path, sizeof path, "%s/scene_%s.wav", dir, GB_SCENES[s].name);
    if (!(f = fopen(path, "wb"))) {
        perror(path);
        return;
    }
    wav_hdr(f, frames);
    transport_req = 1;
    for (t = 0; t < frames; t += CTL) {
        if (t == seconds * FS)
            transport_req = 2;                         /* stop, then the tail */
        mix_block(o, CTL);
        for (i = 0; i < CTL; i++)
            wav_put(f, o[2 * i], o[2 * i + 1]);
    }
    fclose(f);
    printf("estampie demo: %s (%u BPM, %s)\n", path, GB_SCENES[s].bpm, N_RTYPE[GB_SCENES[s].rtype]);
}

int main(int argc, char **argv)
{
    uint32_t s = 0, k;
    if (argc < 2) {
        fprintf(stderr, "usage: estampie_demo OUTDIR [SCENE] [SECONDS]\n");
        return 2;
    }
    if (argc > 2) {
        for (k = 0; k < GB_NSCENE && strcmp(GB_SCENES[k].name, argv[2]); k++)
            ;
        if (k == GB_NSCENE) {
            fprintf(stderr, "estampie demo: no scene %s\n", argv[2]);
            return 2;
        }
        s = k;
    }
    scene(argv[1], s, argc > 3 ? (uint32_t)atoi(argv[3]) : 24u, getenv("SOLO") ? (uint32_t)atoi(getenv("SOLO")) : 0u);
    return 0;
}
