// SPDX-License-Identifier: GPL-3.0-only
// ESTAMPIE sound lab: the candidate sounds (tests/sound_lab.c's, the audition clips' values) for lab.html. eng is the
// engine number (engines.c ENGINES[]; null: the TROTTO's own sound in that part), part the TROTTO's part it takes
// (0 the tune, 1 the lute, 2 the long notes, 3 the tabor), pat a hand-drum line instead of the part's phrase.
export const CANDIDATES = [
 {
  "id": "ref_tune",
  "group": "As now",
  "name": "TROTTO GURDY",
  "idea": "The scene's tune: GURDY, COUP 1/4.",
  "eng": 14,
  "part": 0,
  "pat": null,
  "e": [
   9,
   70,
   55,
   110,
   100,
   1,
   55,
   15
  ],
  "env": [
   2,
   64,
   127,
   20
  ],
  "fenv": 0,
  "mono": 1,
  "fx": [
   8,
   0,
   0,
   40
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "ref_lute",
  "group": "As now",
  "name": "LUTE",
  "idea": "The scene's lute: PHYS STRNG.",
  "eng": 9,
  "part": 1,
  "pat": null,
  "e": [
   1,
   48,
   55,
   70,
   30,
   127,
   0,
   0
  ],
  "env": [
   0,
   100,
   127,
   70
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   15,
   20,
   80
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "ref_long",
  "group": "As now",
  "name": "TENOR RECORD",
  "idea": "The scene's long notes: SAMPLE, the CC0 tenor recorder.",
  "eng": null,
  "part": 2,
  "pat": null,
  "e": [
   0
  ],
  "env": [
   0
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "ref_drum",
  "group": "As now",
  "name": "DANCE KIT",
  "idea": "The scene's tabor: DRUM, ESTAMPIE's DANCE kit.",
  "eng": null,
  "part": 3,
  "pat": null,
  "e": [
   0
  ],
  "env": [
   0
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "citole",
  "group": "Plucked strings",
  "name": "CITOLE",
  "idea": "A small bright quill-plucked lute: STRNG near the bridge, a little bridge buzz.",
  "eng": 9,
  "part": 1,
  "pat": null,
  "e": [
   1,
   26,
   104,
   62,
   12,
   127,
   0,
   34
  ],
  "env": [
   0,
   100,
   127,
   55
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   10,
   15,
   60
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "gittern",
  "group": "Plucked strings",
  "name": "GITTERN",
  "idea": "The lute's small, rounder cousin: STRNG, softer pick, plucked mid-string.",
  "eng": 9,
  "part": 1,
  "pat": null,
  "e": [
   1,
   40,
   78,
   66,
   36,
   120,
   0,
   16
  ],
  "env": [
   0,
   100,
   127,
   60
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   15,
   15,
   65
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "psaltery",
  "group": "Plucked strings",
  "name": "PSALTERY",
  "idea": "Plucked psaltery: SYMP strings an octave up ringing along, long and bright.",
  "eng": 9,
  "part": 1,
  "pat": null,
  "e": [
   3,
   4,
   100,
   96,
   70,
   120,
   0,
   20
  ],
  "env": [
   0,
   100,
   127,
   95
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   20,
   15,
   75
  ],
  "temp": 1,
  "ring": 0
 },
 {
  "id": "brayharp",
  "group": "Plucked strings",
  "name": "BRAY HARP",
  "idea": "A gothic harp with bray pins: SYMP fifths, the pins' buzz (BUZZ).",
  "eng": 9,
  "part": 1,
  "pat": null,
  "e": [
   3,
   20,
   82,
   86,
   50,
   120,
   72,
   10
  ],
  "env": [
   0,
   100,
   127,
   90
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   15,
   15,
   75
  ],
  "temp": 1,
  "ring": 0
 },
 {
  "id": "kora",
  "group": "Beyond",
  "name": "KORA",
  "idea": "Kora: SYMP with a major chord ringing, a harp's attack and a light rattle.",
  "eng": 9,
  "part": 1,
  "pat": null,
  "e": [
   3,
   56,
   92,
   80,
   76,
   127,
   36,
   28
  ],
  "env": [
   0,
   100,
   127,
   85
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   10,
   25,
   60
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "crumhorn",
  "group": "Reeds and winds",
  "name": "CRUMHORN",
  "idea": "A capped reed, cylindrical bore: TRIO squares in unison, a nasal resonance.",
  "eng": 6,
  "part": 0,
  "pat": null,
  "e": [
   1,
   0,
   0,
   3,
   0,
   58,
   62,
   64
  ],
  "env": [
   3,
   70,
   120,
   20
  ],
  "fenv": 0,
  "mono": 1,
  "fx": [
   6,
   0,
   0,
   45
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "rausch",
  "group": "Reeds and winds",
  "name": "RAUSCHPFEIFE",
  "idea": "The loud capped shawm: TRIO saws an octave and a twelfth up, wide open.",
  "eng": 6,
  "part": 0,
  "pat": null,
  "e": [
   0,
   12,
   19,
   5,
   0,
   104,
   22,
   64
  ],
  "env": [
   2,
   70,
   120,
   18
  ],
  "fenv": 0,
  "mono": 1,
  "fx": [
   10,
   0,
   0,
   40
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "shawm2",
  "group": "Reeds and winds",
  "name": "BRIGHT SHAWM",
  "idea": "The SHAWM preset brighter and more biting, for outdoors.",
  "eng": 6,
  "part": 0,
  "pat": null,
  "e": [
   1,
   12,
   19,
   4,
   1,
   104,
   78,
   20
  ],
  "env": [
   4,
   70,
   118,
   35
  ],
  "fenv": 0,
  "mono": 1,
  "fx": [
   12,
   0,
   0,
   40
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "trumpet",
  "group": "Reeds and winds",
  "name": "HERALD TRUMPET",
  "idea": "A natural trumpet: ANALOG saw, the filter opening on each note.",
  "eng": 0,
  "part": 0,
  "pat": null,
  "e": [
   0,
   6,
   0,
   0,
   52,
   28,
   46,
   70
  ],
  "env": [
   6,
   80,
   110,
   30
  ],
  "fenv": 46,
  "mono": 1,
  "fx": [
   8,
   0,
   10,
   55
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "pipes",
  "group": "Beyond",
  "name": "UILLEANN",
  "idea": "Uilleann pipes: GURDY without buzz, a sweeter chanter and a quiet drone, RING on.",
  "eng": 14,
  "part": 0,
  "pat": null,
  "e": [
   9,
   58,
   30,
   78,
   0,
   0,
   96,
   12
  ],
  "env": [
   6,
   64,
   127,
   30
  ],
  "fenv": 0,
  "mono": 1,
  "fx": [
   0,
   12,
   15,
   60
  ],
  "temp": 0,
  "ring": 1
 },
 {
  "id": "chant",
  "group": "Organ and chant",
  "name": "CANTOR",
  "idea": "A single voice singing the tune: VOICE on 'o', a breath, a little wander.",
  "eng": 5,
  "part": 0,
  "pat": null,
  "e": [
   104,
   110,
   10,
   -3,
   70,
   26,
   66,
   12
  ],
  "env": [
   28,
   90,
   118,
   60
  ],
  "fenv": 0,
  "mono": 1,
  "fx": [
   0,
   10,
   10,
   85
  ],
  "temp": 1,
  "ring": 0
 },
 {
  "id": "portative",
  "group": "Organ and chant",
  "name": "PORTATIVE",
  "idea": "A small organ on the player's knee: WHEEL flutes, top up, a chiff.",
  "eng": 7,
  "part": 2,
  "pat": null,
  "e": [
   0,
   -4,
   0,
   4,
   0,
   70,
   6,
   0
  ],
  "env": [
   4,
   64,
   127,
   30
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   10,
   0,
   60
  ],
  "temp": 1,
  "ring": 0
 },
 {
  "id": "regal",
  "group": "Organ and chant",
  "name": "REGAL",
  "idea": "A reed organ: WHEEL's REED registration, nasal and buzzy.",
  "eng": 7,
  "part": 2,
  "pat": null,
  "e": [
   10,
   -3,
   2,
   2,
   0,
   40,
   30,
   0
  ],
  "env": [
   3,
   64,
   127,
   30
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   4,
   0,
   0,
   55
  ],
  "temp": 1,
  "ring": 0
 },
 {
  "id": "nakers",
  "group": "Percussion",
  "name": "NAKERS",
  "idea": "Small kettledrums struck with sticks: MEMB, a loaded head, short.",
  "eng": 9,
  "part": 2,
  "pat": "tabor",
  "e": [
   2,
   92,
   100,
   54,
   64,
   127,
   8,
   60
  ],
  "env": [
   0,
   100,
   127,
   40
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   0,
   0,
   30
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "frame",
  "group": "Percussion",
  "name": "FRAME DRUM",
  "idea": "A deep frame drum: MEMB struck near the centre, its pitch falling.",
  "eng": 9,
  "part": 2,
  "pat": "frame",
  "e": [
   2,
   24,
   44,
   70,
   18,
   110,
   26,
   14
  ],
  "env": [
   0,
   100,
   127,
   70
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   0,
   0,
   40
  ],
  "temp": 0,
  "ring": 0
 },
 {
  "id": "bells",
  "group": "Percussion",
  "name": "HAND BELLS",
  "idea": "Hand bells on the downbeats: MODAL, inharmonic, ringing on.",
  "eng": 9,
  "part": 2,
  "pat": "bells",
  "e": [
   0,
   92,
   108,
   110,
   10,
   127,
   0,
   20
  ],
  "env": [
   0,
   100,
   127,
   100
  ],
  "fenv": 0,
  "mono": 0,
  "fx": [
   0,
   10,
   20,
   70
  ],
  "temp": 0,
  "ring": 0
 }
];
// hand-drum lines in 8T, 12 steps (two bars of 6/8): notes, 0 a rest
export const LINES = {"tabor": [57, 0, 64, 57, 0, 64, 57, 64, 64, 57, 0, 69], "frame": [41, 0, 0, 41, 0, 48, 41, 0, 0, 41, 48, 48], "bells": [81, 0, 0, 0, 0, 0, 76, 0, 0, 0, 0, 0]};
