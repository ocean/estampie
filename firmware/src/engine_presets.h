/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 GHOULBOX (on Felucca by Leo Kuroshita, Hügelton Instruments) */
/* GHOULBOX's and ESTAMPIE's own presets for the five Felucca-Engines engines (firmware/engines/, MIT): each
 * engine's preset table ends with a hook, ESTAMPIE_<ENGINE>_PRESETS, that appends these. Kept here, under GHOULBOX's
 * GPL-3.0-only, so the vendored engine files stay as Felucca-Engines publishes them but for that one hook.
 * Append-only: projects and user presets store a preset's number */
#pragma once

#define ESTAMPIE_ANALOG_PRESETS \
    {"CRYPT PAD", {0, 14, 64, 6, 42, 8, 6, 40}, {95, 90, 120, 105}, 6, 0, FX(0, 60, 10, 100), PAT(5)}, \
    {"FROST STR", {4, 9, 50, 0, 58, 5, 0, 48}, {70, 90, 115, 95}, 4, 0, FX(0, 90, 10, 95), PAT(5)}, \
    {"WAR HORN", {0, 6, 64, 0, 30, 18, 12, 64}, {45, 80, 105, 70}, 40, 0, FX(5, 20, 15, 85), PAT(4)}, \
    {"DIRGE BASS", {1, 0, 0, 0, 34, 10, 20, 32}, {10, 80, 110, 60}, 15, 1, FX(10, 0, 0, 40), PAT(8)}, \

#define ESTAMPIE_LOFI_PRESETS \
    {"TOWER LEAD", {0, 1, 64, 0, 0, 30, 0, 70}, {5, 70, 110, 60}, 0, 1, FX(0, 20, 35, 90), PAT(4)}, \
    {"CASIO CHOIR", {0, 4, 92, 0, 0, 20, 0, 80}, {60, 90, 115, 90}, 0, 0, FX(0, 70, 20, 100), PAT(5)}, \
    {"RECORDER", {2, 1, 64, 0, 0, 24, 0, 60}, {12, 70, 115, 50}, 0, 1, FX(0, 20, 25, 90), PAT(4)}, \

#define ESTAMPIE_TRIO_PRESETS \
    {"FANTASY PAD", {0, 12, 7, 14, 0, 55, 20, 64}, {90, 90, 115, 100}, 10, 0, FX(0, 90, 25, 110), PAT(5)}, \
    {"MOURN HORN", {1, 0, -12, 5, 0, 40, 25, 50}, {40, 85, 105, 70}, 35, 0, FX(5, 20, 10, 90), PAT(4)}, \
    {"STRING MACH", {0, 12, 0, 18, 0, 70, 5, 64}, {60, 90, 120, 95}, 0, 0, FX(0, 110, 0, 100), PAT(5)}, \
    {"SHAWM", {1, 12, 19, 4, 1, 85, 70, 25}, {6, 70, 115, 45}, 10, 1, FX(20, 0, 10, 85), PAT(19)}, \

#define ESTAMPIE_WHEEL_PRESETS \
    {"CRYPT ORGAN", {12, 2, 0, -3, 0, 5, 15, 1}, {20, 64, 127, 70}, 0, 0, FX(0, 10, 0, 100), PAT(5)}, \
    {"CATHEDRAL", {15, 3, 0, -2, 0, 0, 10, 0}, {30, 64, 127, 90}, 0, 0, FX(0, 0, 0, 115), PAT(5)}, \
    {"HARMONIUM", {10, 0, 1, -4, 0, 10, 25, 1}, {15, 64, 127, 50}, 0, 0, FX(5, 20, 0, 85), PAT(6)}, \
    {"TOWER FLUTE", {0, 0, -3, -6, 0, 0, 30, 1}, {25, 64, 127, 60}, 0, 1, FX(0, 30, 20, 95), PAT(4)}, \

#define ESTAMPIE_NOISE_PRESETS \
    {"CAVE WIND", {0, 40, 50, 90, 0, 0, 90, 0}, {110, 90, 120, 120}, 0, 0, FX(0, 40, 20, 120), PAT(5)}, \
    {"TORCH", {1, 30, 100, 60, 0, 60, 30, 0}, {20, 90, 127, 80}, 0, 0, FX(0, 0, 0, 70), PAT(5)},
