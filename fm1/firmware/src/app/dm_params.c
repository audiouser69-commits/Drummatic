/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the pages: what KNOB 1..4 turn on each page, how a value reads, how a click moves it. */
#include <stddef.h>

enum {
    PK_NONE, PK_T,          /* a voice's modulation target (normalised base) */
    PK_VF,                  /* a voice float (lo..hi) */
    PK_VU8,                 /* a voice uint8 (0..n-1, names or a number) */
    PK_VI8,                 /* a voice int8 (lo..hi) */
    PK_GF,                  /* a global float (lo..hi) */
    PK_GU8,                 /* a global uint8 */
    PK_PF,                  /* a Performance control (-1..1) */
    PK_UI,                  /* the UI's own state (lane, MOD source / target, lock target, slots, name) */
    PK_ACT,                 /* an action: turned right */
};
enum {
    F_PCT, F_BIP, F_HZ, F_SEC, F_DB, F_ST, F_INT, F_NAMES, F_ONOFF, F_VOICE, F_MS, F_BPM, F_X, F_PAN, F_SEND,
};
typedef struct {
    const char *label;       /* 0: the engine's knob name (id = 0..3) */
    uint8_t kind, fmt, n;    /* n: the names' count (F_NAMES) */
    uint16_t id;             /* target / offset / control / UI id */
    float lo, hi, step;      /* step: per click (0: (hi - lo) / 100) */
    const char *const *names;
} pd_t;

#define VOFF(f) ((uint16_t)offsetof(dm_vkit_t, f))
#define GOFF(f) ((uint16_t)offsetof(dm_gkit_t, f))

static const char *const NM_ONOFF[2] = {"OFF", "ON"};
static const char *const NM_ENV[2] = {"ENV 1", "ENV 2"};
static const char *const NM_LWAVE[LW_COUNT] = {"WAVETBL", "TRI TILT", "PULSE", "SMTH RND"};
static const char *const NM_LRESET[LR_COUNT] = {"FREE", "ALWAYS", "SOFT", "S&H", "1 SHOT"};
static const char *const NM_REV[4] = {"DARK", "BRIGHT", "HALL", "TIN CAN"};
static const char *const NM_CLIP[4] = {"NONE", "SOFT", "HARD", "FOLD"};
static const char *const NM_CLK[2] = {"INT", "EXT"};
static const char *const NM_SYNC2[2] = {"FREE", "SYNC"};
static const char *const NM_PRE[2] = {"PRE", "POST"};
static const char *const NM_VOICE9[9] = {"---", "1", "2", "3", "4", "5", "6", "7", "8"};
static const char *const NM_ENG[ENG_COUNT] = {"KICK", "SNARE", "HAT", "CLAP", "FM", "MODAL", "COWBELL", "ZAP", "PILLOW",
                                              "TRIPWIRE", "RADIATOR", "KOMBUCHA", "LPG", "BUILDER", "ADDITIVE", "CHAOS",
                                              "VOSIM", "SUMSYN", "RESON", "COMBINAT", "TERRAIN", "SHAME", "ENTANGLE",
                                              "HAYWIRE", "SCRAPYRD"};

/* UI ids */
enum { U_LANE, U_LSYNC, U_LLEN, U_MSRC, U_MTGT, U_MDEPTH, U_LTGT, U_LON, U_KSLOT, U_NPOS, U_NCHAR, U_VSLOT,
       U_PAL, U_LEDS, U_USB, U_AUTO, U_CLK };
/* actions */
enum { A_ROT_LANE, A_ROT_TRIG, A_CLR_LANE, A_CLR_TRIG, A_COPY, A_PASTE, A_RESET, A_COMMIT, A_PFRESET, A_PRESET, A_INIT,
       A_NROUTE, A_REF };

typedef struct { uint8_t btn; const char *name; pd_t k[4]; } page_t;

#define T(t, l, f) {l, PK_T, f, 0, t, 0, 1, 0, 0}
#define TP(i) {0, PK_T, F_PCT, 0, (uint16_t)(T_P1 + (i)), 0, 1, 0, 0}
#define VF(f, l, lo, hi, fm) {l, PK_VF, fm, 0, VOFF(f), lo, hi, 0, 0}
#define VE(f, l, nm, n) {l, PK_VU8, F_NAMES, n, VOFF(f), 0, (float)((n) - 1), 1, nm}
#define VI(f, l, lo, hi) {l, PK_VI8, F_INT, 0, VOFF(f), lo, hi, 1, 0}
#define VU(f, l, lo, hi) {l, PK_VU8, F_INT, 0, VOFF(f), lo, hi, 1, 0}
#define GF(f, l, lo, hi, fm, st) {l, PK_GF, fm, 0, GOFF(f), lo, hi, st, 0}
#define GE(f, l, nm, n) {l, PK_GU8, F_NAMES, n, GOFF(f), 0, (float)((n) - 1), 1, nm}
#define PF(i) {0, PK_PF, F_BIP, 0, i, -1, 1, 0, 0}
#define UI(id, l, fm) {l, PK_UI, fm, 0, id, 0, 0, 1, 0}
#define AC(id, l) {l, PK_ACT, F_NAMES, 0, id, 0, 0, 1, 0}
#define NONE {0, PK_NONE, 0, 0, 0, 0, 0, 0, 0}

static const page_t PAGES[] = {
    /* EDIT: the sound */
    {B_EDIT, "SYNTH", {TP(0), TP(1), TP(2), TP(3)}},
    {B_EDIT, "ENGINE", {VE(engine, "ENGINE", NM_ENG, ENG_COUNT), T(T_PITCH, "PITCH", F_ST), VE(stereo, "STEREO", NM_ONOFF, 2),
                        VE(env2, "AMP ENV", NM_ENV, 2)}},
    {B_EDIT, "VOICE", {AC(A_COPY, "COPY"), AC(A_PASTE, "PASTE"), AC(A_RESET, "RESET"), AC(A_PRESET, "PRESETS")}},
    /* FX: TONE */
    {B_FX, "TONE", {VE(fmode, "FILTER", DM_FNAME, FM_COUNT), T(T_CUTOFF, "CUTOFF", F_HZ), T(T_RES, "RESO", F_PCT),
                    T(T_DIST, "DISTORT", F_PCT)}},
    {B_FX, "TONE 2", {VE(dmode, "DIST", DM_DNAME, DM_COUNT), VE(post, "ORDER", NM_PRE, 2), NONE, NONE}},
    /* ENV */
    {B_ENV, "ENV 1", {T(T_E1A, "ATTACK", F_SEC), T(T_E1H, "HOLD", F_SEC), T(T_E1D, "DECAY", F_SEC),
                      VF(e1curve, "CURVE", 0, 1, F_PCT)}},
    {B_ENV, "ENV 2", {T(T_E2A, "ATTACK", F_SEC), T(T_E2H, "HOLD", F_SEC), T(T_E2D, "DECAY", F_SEC),
                      VF(e2curve, "CURVE", 0, 1, F_PCT)}},
    {B_ENV, "VELOCITY", {VF(e1vel, "ENV1 VEL", 0, 1, F_PCT), VF(e2vel, "ENV2 VEL", 0, 1, F_PCT), NONE, NONE}},
    /* LFO */
    {B_LFO, "LFO 1", {T(T_L1SPD, "SPEED", F_X), T(T_L1AMP, "AMP", F_PCT), VF(l1shape, "SHAPE", 0, 1, F_PCT),
                      VE(l1wave, "WAVE", NM_LWAVE, LW_COUNT)}},
    {B_LFO, "LFO 1+", {VE(l1sync, "SYNC", NM_SYNC2, 2), VE(l1bip, "BIPOLAR", NM_ONOFF, 2),
                       VE(l1reset, "RESET", NM_LRESET, LR_COUNT), NONE}},
    {B_LFO, "LFO 2", {T(T_L2SPD, "SPEED", F_X), T(T_L2AMP, "AMP", F_PCT), VF(l2shape, "SHAPE", 0, 1, F_PCT),
                      VE(l2wave, "WAVE", NM_LWAVE, LW_COUNT)}},
    {B_LFO, "LFO 2+", {VE(l2sync, "SYNC", NM_SYNC2, 2), VE(l2bip, "BIPOLAR", NM_ONOFF, 2),
                       VE(l2reset, "RESET", NM_LRESET, LR_COUNT), NONE}},
    /* SCL: MOD */
    {B_SCL, "MOD", {UI(U_MSRC, "SOURCE", F_NAMES), UI(U_MTGT, "TARGET", F_NAMES), UI(U_MDEPTH, "DEPTH", F_BIP),
                    AC(A_NROUTE, "ROUTES")}},
    {B_SCL, "MACRO", {VF(macro1, "MACRO 1", 0, 1, F_PCT), VF(macro2, "MACRO 2", 0, 1, F_PCT),
                      VF(variation, "VARIAT", 0, 1, F_PCT), NONE}},
    {B_SCL, "LOCKS", {UI(U_LTGT, "TARGET", F_NAMES), UI(U_LON, "LOCK", F_ONOFF), NONE, NONE}},
    /* SEQ */
    {B_SEQ, "EUCLID", {VI(steps, "STEPS", 1, 64), VI(fill, "FILL", 0, 64), VI(pattern, "PATTERN", 1, 64),
                       VI(rotate, "ROTATE", -32, 32)}},
    {B_SEQ, "TIME", {VE(speed, "SPEED", DM_SPEED_NAME, DM_NSPEED), VE(mult, "MULT", DM_MULT_NAME, DM_NMULT),
                     VU(length, "LENGTH", 1, 128), VE(snap, "SNAP", NM_ONOFF, 2)}},
    {B_SEQ, "GROOVE", {VF(shift, "SHIFT", -1, 1, F_BIP), VF(swing, "SWING", -1, 1, F_BIP), VF(shape, "SHAPE", -1, 1, F_BIP),
                       VF(density, "DENSITY", -1, 1, F_BIP)}},
    {B_SEQ, "MORE", {VE(invert, "INVERT", NM_ONOFF, 2), VE(reset_on, "RESET", NM_ONOFF, 2), VU(reset_len, "RES LEN", 1, 128),
                     GF(humanize, "HUMANIZE", 0, 1, F_PCT, 0)}},
    {B_SEQ, "LANE", {UI(U_LANE, "LANE", F_NAMES), UI(U_LSYNC, "MODE", F_NAMES), UI(U_LLEN, "LENGTH", F_INT),
                     AC(A_ROT_LANE, "ROTATE")}},
    {B_SEQ, "TOOLS", {AC(A_ROT_TRIG, "ROT TRIG"), AC(A_CLR_LANE, "CLR LANE"), AC(A_CLR_TRIG, "CLR TRIG"), NONE}},
    /* HOME: MIX */
    {B_HOME, "MIX", {T(T_GAIN, "GAIN", F_DB), T(T_PAN, "PAN", F_PAN), T(T_DSEND, "DELAY", F_SEND), T(T_RSEND, "REVERB", F_SEND)}},
    {B_HOME, "ROUTING", {VE(choke, "CHOKE", NM_VOICE9, 9), VE(link, "TRIGGER", NM_VOICE9, 9), VE(pf_enable, "PERF", NM_ONOFF, 2),
                         VE(pf_flip, "FLIP", NM_ONOFF, 2)}},
    /* ARP: PERFORM */
    {B_ARP, "PF ENV", {PF(PF_ENVA), PF(PF_ENVH), PF(PF_ENVD), PF(PF_ENVC)}},
    {B_ARP, "PF SYNTH", {PF(PF_SYNPITCH), PF(PF_SYN1), PF(PF_SYN2), PF(PF_SYN3)}},
    {B_ARP, "PF MOD", {PF(PF_SYN4), PF(PF_MACRO1), PF(PF_MACRO2), PF(PF_VAR)}},
    {B_ARP, "PF TONE", {PF(PF_MODAMP), PF(PF_DISTORT), PF(PF_CUTOFF), PF(PF_RES)}},
    {B_ARP, "PF SEQ", {PF(PF_STEPS), PF(PF_FILL), PF(PF_PATTERN), PF(PF_DENSITY)}},
    {B_ARP, "PF SEQ 2", {PF(PF_PROB), PF(PF_RATCHET), GF(pf_depth, "DEPTH", -1, 1, F_BIP, 0), GE(pf_enabled, "ENABLED", NM_ONOFF, 2)}},
    {B_ARP, "PF GLOBAL", {GF(pf_rnd, "DADO", 0, 1, F_PCT, 0), AC(A_COMMIT, "COMMIT"), AC(A_PFRESET, "RESET"), NONE}},
    /* GLO */
    {B_GLO, "TEMPO", {GF(bpm, "BPM", 40, 300, F_BPM, 1), UI(U_CLK, "CLOCK", F_NAMES), GF(humanize, "HUMANIZE", 0, 1, F_PCT, 0),
                      GF(rnd_depth, "DADO", 0, 1, F_PCT, 0)}},
    {B_GLO, "DELAY", {GE(d_sync, "SYNC", NM_SYNC2, 2), GE(d_div, "DIVISION", DM_LFODIV_NAME, DM_NLFODIV),
                      GF(d_time_ms, "TIME", 1, 1480, F_MS, 0), GF(d_fb, "FEEDBACK", 0, 1, F_PCT, 0)}},
    {B_GLO, "DELAY 2", {GF(d_glitch, "GLITCH", 0, 1, F_PCT, 0), GF(d_fbtilt, "FB TILT", -1, 1, F_BIP, 0),
                        GF(d_duck, "DUCKING", 0, 1, F_PCT, 0), GF(d_srate, "S.RATE", 0, 1, F_PCT, 0)}},
    {B_GLO, "DELAY 3", {GE(d_stereo, "STEREO", NM_ONOFF, 2), GE(d_pong, "PONG", NM_ONOFF, 2), GF(d_torev, "> REVERB", 0, 1, F_PCT, 0),
                        GF(d_gain_db, "GAIN", -24, 12, F_DB, 0.5f)}},
    {B_GLO, "REVERB", {GE(r_mode, "MODE", NM_REV, 4), GF(r_decay, "DECAY", 0.1f, 20, F_SEC, 0), GF(r_pre_ms, "PREDELAY", 0.1f, 499, F_MS, 0),
                       GF(r_gain_db, "GAIN", -24, 12, F_DB, 0.5f)}},
    {B_GLO, "REVERB 2", {GE(r_sync, "PRE SYNC", NM_SYNC2, 2), GE(r_div, "PRE DIV", DM_LFODIV_NAME, DM_NLFODIV),
                         GF(r_srate, "S.RATE", 0, 1, F_PCT, 0), GF(r_intilt, "IN TILT", -1, 1, F_BIP, 0)}},
    {B_GLO, "REVERB 3", {GF(r_outtilt, "OUT TILT", -1, 1, F_BIP, 0), NONE, NONE, NONE}},
    {B_GLO, "MASTER", {GF(m_max, "MAXIMIZE", 0, 1, F_PCT, 0), GF(m_low, "LOW", -18, 18, F_DB, 0.5f), GF(m_mid, "MID", -18, 18, F_DB, 0.5f),
                       GF(m_high, "HIGH", -18, 18, F_DB, 0.5f)}},
    {B_GLO, "MASTER 2", {GF(m_gain, "GAIN", -36, 12, F_DB, 0.5f), GE(m_clip, "CLIP", NM_CLIP, 4), NONE, NONE}},
    /* SAVE */
    {B_SAVE, "KITS", {UI(U_KSLOT, "SLOT", F_INT), UI(U_NPOS, "LETTER", F_INT), UI(U_NCHAR, "CHAR", F_NAMES), AC(A_INIT, "NEW")}},
    {B_SAVE, "VOICES", {UI(U_VSLOT, "SLOT", F_INT), NONE, NONE, NONE}},
    /* HOME held: the menu */
    {B_HOME, "MENU", {UI(U_PAL, "PALETTE", F_NAMES), UI(U_LEDS, "LEDS", F_NAMES), UI(U_USB, "USB OUT", F_NAMES),
                      UI(U_AUTO, "AUTOSV", F_ONOFF)}},
};
#define NPAGES (sizeof PAGES / sizeof PAGES[0])
#define PG_MENU (NPAGES - 1u)
#define PG_KITS (NPAGES - 3u)
#define PG_VOICES (NPAGES - 2u)

/* the lanes a held step edits: three groups (PRESETS picks one) */
static const uint8_t LGRP[3][4] = {{L_VEL, L_PITCH, L_RATCH, L_PROB}, {L_SHIFT, L_ENV, L_PAN, L_VAR}, {L_SENDS, L_MACRO2, 0xFF, 0xFF}};
