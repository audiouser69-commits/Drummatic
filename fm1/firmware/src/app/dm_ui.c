/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the panel: keys, buttons, knobs, LEDs and the screen.
 *
 *   white keys 1..16   the 16 steps of the page (OCT- / OCT+: the page); a tap adds or removes a trig;
 *                      held (one or more): KNOB 1..4 set the lanes of those steps (PRESETS: the lanes' group)
 *   black keys 1..8    select a voice; 9 MUTE, 10 SOLO (tap: the voice; held with voice keys: those voices);
 *                      11 DADO (tap: the page's dice; held with voice keys: those voices)
 *   REC                the keys' mode: STEPS -> PADS (white 1..8 play voices 1..8, 9..16 the voice in pitch)
 *                      -> REC (the pads write trigs while it plays) -> STEPS
 *   PLAY               start / stop
 *   EDIT FX ENV LFO SCL SEQ HOME ARP GLO SAVE   the page groups (a tap again: the next page)
 *   HOME held          the menu;  SAVE held + a knob: lock / unlock its control;  SCL held + a knob: MAP (the MOD
 *                      page's source modulates that control: the turn sets the depth)
 *   SELECT tempo, ALGORITHM the voice, PRESETS the page of the group (or the lanes' group), MASTER the volume */

#define LC_TOP 24
#define LC_ROWS 15
#define LC_BOT 144

static uint16_t vcol(uint32_t v)
{
    static const uint16_t C[DM_NV] = {RGB(255, 112, 88), RGB(255, 170, 60), RGB(240, 220, 80), RGB(120, 220, 110),
                                      RGB(72, 214, 190), RGB(92, 168, 255), RGB(170, 130, 255), RGB(250, 120, 200)};
    return ux.mono ? T_THEME : C[v % DM_NV];
}

enum { KM_STEPS, KM_PADS, KM_REC };
static struct {
    uint8_t sel, page, spage, keys, lgrp, lane, msrc, mtgt, ltgt, kslot, vslot, npos, oct, menu;
    uint8_t confirm, confirm_k;              /* an action waiting for OCT+ (A_*, 0xFF: none) */
    uint32_t held, edited;                   /* steps held (bit per white key), a knob turned while held */
    uint32_t bdown[NB];                      /* when each button went down (0: up) */
    uint8_t blk_held, blk_used;              /* black keys held (bit per black key 0..10); used as modifier */
    uint8_t save_used, scl_used, home_used;
    char msg[40];
    uint32_t msg_t, hot_t, hot_col;
    uint32_t sig_top, sig_row[DM_NV], sig_bot;
    uint32_t seen[DM_NV], flash[DM_NV];
    uint8_t force, dirty, asave;              /* dirty: changed since the slot; asave: the autosave is due */
    uint32_t dirty_ms, last_input;
    uint8_t clip_ok;
    dm_vkit_t clip;                          /* the voice clipboard (COPY / PASTE) */
    uint8_t hits[DM_NV][64], esteps[DM_NV];   /* the pattern and the steps playing (the Performance's offsets in) */
    uint32_t hsig[DM_NV];
    uint8_t kit_used[DM_NKITS], vpre_used[DM_NVPRE];
    char kit_names[DM_NKITS][12];
    uint8_t slots_known;
} u;

static void ui_msg(const char *s)
{
    str_cpy(u.msg, s, sizeof u.msg);
    u.msg_t = fm1_ms + 1500u;
}
static void ui_dirty(void)
{
    u.dirty = 1;
    u.asave = 1;
    u.dirty_ms = fm1_ms;
}

/* ---------------------------------------------------------- keys --- */
static int key_black(uint32_t k) { return (int)((0x54Au >> ((k + 5u) % 12u)) & 1u); }
static uint32_t key_place(uint32_t k)
{
    uint32_t i, n = 0;
    for (i = 0; i < k; i++)
        n += (uint32_t)(key_black(i) == key_black(k));
    return n;
}
static uint32_t white_key(uint32_t p)       /* the key of white place p */
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (!key_black(k) && key_place(k) == p)
            return k;
    return 0;
}
static uint32_t black_key(uint32_t p)
{
    uint32_t k;
    for (k = 0; k < 27u; k++)
        if (key_black(k) && key_place(k) == p)
            return k;
    return 1;
}

/* ------------------------------------------------------- values --- */
static dm_vkit_t *selv(void) { return &dm_kit.v[u.sel % DM_NV]; }
static uint32_t seq_len(const dm_vkit_t *v) { return dm_seq_length(v->steps, v->length, v->snap); }
static uint32_t npages_steps(const dm_vkit_t *v) { uint32_t n = (seq_len(v) + 15u) / 16u; return n ? n : 1u; }

static float pd_get(const pd_t *d)
{
    dm_vkit_t *v = selv();
    switch (d->kind) {
    case PK_T: return v->base[d->id];
    case PK_VF: return *(float *)((uint8_t *)v + d->id);
    case PK_VU8: return (float)*((uint8_t *)v + d->id);
    case PK_VI8: return (float)*((int8_t *)v + d->id);
    case PK_GF: return *(float *)((uint8_t *)&dm_kit.g + d->id);
    case PK_GU8: return (float)*((uint8_t *)&dm_kit.g + d->id);
    case PK_PF: return dm_kit.g.perf[d->id];
    case PK_UI:
        switch (d->id) {
        case U_LANE: return u.lane;
        case U_LSYNC: return v->lane_sync[u.lane];
        case U_LLEN: return v->lane_len[u.lane];
        case U_MSRC: return u.msrc;
        case U_MTGT: return u.mtgt;
        case U_MDEPTH: return v->depth[u.msrc][u.mtgt];
        case U_LTGT: return u.ltgt;
        case U_LON: return (float)((v->locks >> u.ltgt) & 1u);
        case U_KSLOT: return u.kslot;
        case U_VSLOT: return u.vslot;
        case U_NPOS: return u.npos;
        case U_NCHAR: return (float)(uint8_t)dm_kit.name[u.npos];
        case U_PAL: return dm_set.palette;
        case U_LEDS: return dm_set.leds;
        case U_USB: return dm_set.usb_fixed;
        case U_AUTO: return (float)!(dm_set.rsv[0] & 1u);
        case U_CLK: return dm_m.clk;
        }
        return 0;
    default: return 0;
    }
}

/* a value as text */
static void ui_num(char *b, int32_t v)
{
    char t[12];
    uint32_t n = 0, a = (uint32_t)(v < 0 ? -v : v), i = 0;
    do {
        t[n++] = (char)('0' + a % 10u);
        a /= 10u;
    } while (a && n < 11u);
    if (v < 0)
        b[i++] = '-';
    while (n)
        b[i++] = t[--n];
    b[i] = 0;
}
static void ui_fix(char *b, float x, int dec)       /* x with dec decimals */
{
    int32_t m = dec == 2 ? 100 : dec == 1 ? 10 : 1, w;
    float r = x * (float)m;
    w = (int32_t)(r < 0.0f ? r - 0.5f : r + 0.5f);
    if (w < 0) {
        *b++ = '-';
        w = -w;
    }
    ui_num(b, w / m);
    if (dec) {
        uint32_t n = str_len(b), f = (uint32_t)(w % m);
        b[n++] = '.';
        if (dec == 2)
            b[n++] = (char)('0' + f / 10u);
        b[n++] = (char)('0' + f % 10u);
        b[n] = 0;
    }
}
static void ui_time(char *b, float s)
{
    if (s < 0.01f) {
        ui_fix(b, s * 1000.0f, 1);
        str_cpy(b + str_len(b), "MS", 4);
    } else if (s < 1.0f) {
        ui_num(b, (int32_t)(s * 1000.0f + 0.5f));
        str_cpy(b + str_len(b), "MS", 4);
    } else {
        ui_fix(b, s, 2);
        str_cpy(b + str_len(b), "S", 4);
    }
}
static void pd_text(const pd_t *d, float v, char *b, uint32_t n)
{
    b[0] = 0;
    if (d->kind == PK_T) {
        float x = dm_denorm(d->id, v);
        switch (d->fmt) {
        case F_ST: ui_num(b, (int32_t)dm_roundf(x)); str_cpy(b + str_len(b), " ST", 4); return;
        case F_HZ:
            if (x >= 1000.0f) {
                ui_fix(b, x * 0.001f, x >= 10000.0f ? 1 : 2);
                str_cpy(b + str_len(b), "K", 4);
            } else
                ui_num(b, (int32_t)(x + 0.5f));
            return;
        case F_SEC: ui_time(b, x); return;
        case F_DB:
            if (x <= -59.9f) {
                str_cpy(b, "-INF", n);
                return;
            }
            ui_fix(b, x, 1);
            return;
        case F_PAN:
            if (dm_absf(x) < 0.005f) {
                str_cpy(b, "C", n);
                return;
            }
            b[0] = x < 0.0f ? 'L' : 'R';
            ui_num(b + 1, (int32_t)(dm_absf(x) * 100.0f + 0.5f));
            return;
        case F_SEND:
            if (x <= 0.0005f) {
                str_cpy(b, "-INF", n);
                return;
            }
            ui_fix(b, 20.0f * dm_log2(x * x) * 0.30103f, 1);
            return;
        case F_X: {                                   /* an LFO speed: the division when synced, else Hz */
            const dm_vkit_t *vv = selv();
            int sync = d->id == T_L1SPD ? vv->l1sync : vv->l2sync;
            if (sync) {
                str_cpy(b, DM_LFODIV_NAME[dm_lfo_div(x)], n);
                return;
            }
            x = dm_lfo_hz(x);
            ui_fix(b, x, x < 10.0f ? 2 : x < 100.0f ? 1 : 0);
            str_cpy(b + str_len(b), "HZ", 4);
            return;
        }
        default: ui_num(b, (int32_t)(x * 100.0f + (x < 0 ? -0.5f : 0.5f))); return;
        }
    }
    if (d->kind == PK_UI) {
        const dm_vkit_t *vv = selv();
        switch (d->id) {
        case U_LANE: str_cpy(b, DM_LANE[u.lane].name, n); return;
        case U_LSYNC: str_cpy(b, DM_LSYNC_NAME[vv->lane_sync[u.lane] % LS_COUNT], n); return;
        case U_MSRC: str_cpy(b, DM_SNAME[u.msrc], n); return;
        case U_MTGT:
        case U_LTGT: {
            uint32_t t = d->id == U_MTGT ? u.mtgt : u.ltgt;
            if (t >= T_P1 && t <= T_P4)
                str_cpy(b, DM_ENG_INFO[vv->engine % ENG_COUNT].k[t - T_P1], n);
            else if (t >= T_E1A && t <= T_E2D) {
                b[0] = 'E';
                b[1] = (char)(t <= T_E1D ? '1' : '2');
                b[2] = ' ';
                str_cpy(b + 3, DM_TNAME[t], n - 3u);
            } else if (t >= T_L1SPD && t <= T_L2AMP) {
                str_cpy(b, t <= T_L1AMP ? "L1 " : "L2 ", n);
                str_cpy(b + 3, DM_TNAME[t], n - 3u);
            } else
                str_cpy(b, DM_TNAME[t], n);
            return;
        }
        case U_MDEPTH: ui_num(b, (int32_t)(v * 100.0f + (v < 0 ? -0.5f : 0.5f))); return;
        case U_LON: str_cpy(b, v > 0.5f ? "LOCKED" : "FREE", n); return;
        case U_KSLOT:
        case U_VSLOT: ui_num(b, (int32_t)v + 1); return;
        case U_NPOS: ui_num(b, (int32_t)v + 1); return;
        case U_NCHAR: {
            char c = dm_kit.name[u.npos];
            b[0] = c ? c : '_';
            b[1] = 0;
            return;
        }
        case U_PAL: str_cpy(b, UI_PALETTES[dm_set.palette % NPALETTES].name, n); return;
        case U_LEDS: str_cpy(b, dm_set.leds ? "DARK" : "GLOW", n); return;
        case U_USB: str_cpy(b, dm_set.usb_fixed ? "FIXED" : "MASTER", n); return;
        case U_AUTO: str_cpy(b, v > 0.5f ? "ON" : "OFF", n); return;
        case U_CLK: str_cpy(b, dm_m.clk ? "EXT" : "INT", n); return;
        default: ui_num(b, (int32_t)v); return;
        }
    }
    if (d->kind == PK_ACT) {
        if (d->id == A_NROUTE) {
            uint32_t s, t, c = 0;
            for (s = 0; s < DS_N; s++)
                for (t = 0; t < DT_N; t++)
                    c += selv()->depth[s][t] != 0.0f;
            ui_num(b, (int32_t)c);
            return;
        }
        str_cpy(b, u.confirm == d->id ? "OCT+ ?" : d->id == A_INIT && u.confirm == A_REF ? "DAEL ?" : "TURN >", n);
        return;
    }
    switch (d->fmt) {
    case F_NAMES: str_cpy(b, d->names[dm_clampi((int)v, 0, d->n - 1)], n); return;
    case F_ONOFF: str_cpy(b, v > 0.5f ? "ON" : "OFF", n); return;
    case F_INT: ui_num(b, (int32_t)v); return;
    case F_BIP: ui_num(b, (int32_t)(v * 100.0f + (v < 0 ? -0.5f : 0.5f))); return;
    case F_DB: ui_fix(b, v, 1); return;
    case F_SEC: ui_time(b, v); return;
    case F_MS: ui_time(b, v * 0.001f); return;
    case F_BPM: ui_fix(b, v, v < 100.0f ? 1 : 0); return;
    default: ui_num(b, (int32_t)(v * 100.0f + 0.5f)); return;
    }
}
static float pd_frac(const pd_t *d, float v)          /* 0..1 for the gauge */
{
    switch (d->kind) {
    case PK_T: return v;
    case PK_PF: return (v + 1.0f) * 0.5f;
    case PK_UI:
        if (d->id == U_MDEPTH)
            return (v + 1.0f) * 0.5f;
        return -1.0f;
    case PK_ACT: return -1.0f;
    default: return d->hi > d->lo ? (v - d->lo) / (d->hi - d->lo) : 0.0f;
    }
}

/* a click on a knob */
static void ui_action(uint32_t id, int32_t s);
static int32_t accel(int32_t s) { int32_t a = s < 0 ? -s : s; return a >= 3 ? s * 4 : a == 2 ? s * 2 : s; }
static void pd_turn(const pd_t *d, int32_t s)
{
    dm_vkit_t *v = selv();
    float step;
    if (d->kind == PK_NONE || !s)
        return;
    if (d->kind == PK_ACT) {
        ui_action(d->id, s);
        return;
    }
    ui_dirty();
    switch (d->kind) {
    case PK_T: {
        float x = v->base[d->id];
        if (d->id == T_PITCH) {                         /* whole semitones */
            float st = dm_roundf(dm_denorm(T_PITCH, x)) + (float)s;
            v->base[d->id] = dm_norm(T_PITCH, dm_clampf(st, -48.0f, 48.0f));
        } else
            v->base[d->id] = dm_clamp01(x + (float)accel(s) * 0.01f);
        return;
    }
    case PK_VF: case PK_GF: case PK_PF: {
        float *p = d->kind == PK_VF ? (float *)((uint8_t *)v + d->id) : d->kind == PK_GF ?
                   (float *)((uint8_t *)&dm_kit.g + d->id) : &dm_kit.g.perf[d->id];
        float lo = d->kind == PK_PF ? -1.0f : d->lo, hi = d->kind == PK_PF ? 1.0f : d->hi;
        step = d->step > 0.0f ? d->step : (hi - lo) * 0.01f;
        if (d->fmt == F_MS || d->fmt == F_SEC) {         /* times: in proportion */
            float k = dm_exp2((float)accel(s) * 0.06f);
            *p = dm_clampf(*p * k, lo, hi);
            return;
        }
        *p = dm_clampf(*p + step * (float)accel(s), lo, hi);
        return;
    }
    case PK_VU8: case PK_GU8: {
        uint8_t *p = d->kind == PK_VU8 ? (uint8_t *)v + d->id : (uint8_t *)&dm_kit.g + d->id;
        int x = (int)*p + (d->fmt == F_INT ? (int)accel(s) : (int)s);
        *p = (uint8_t)dm_clampi(x, (int)d->lo, (int)d->hi);
        return;
    }
    case PK_VI8: {
        int8_t *p = (int8_t *)v + d->id;
        *p = (int8_t)dm_clampi((int)*p + (int)accel(s), (int)d->lo, (int)d->hi);
        if (d->id == VOFF(steps) && v->fill > v->steps)
            v->fill = v->steps;
        return;
    }
    case PK_UI:
        switch (d->id) {
        case U_LANE: u.lane = (uint8_t)dm_clampi((int)u.lane + s, 0, DM_NLANE - 1); return;
        case U_LSYNC: v->lane_sync[u.lane] = (uint8_t)dm_clampi((int)v->lane_sync[u.lane] + s, 0, LS_COUNT - 1); return;
        case U_LLEN: v->lane_len[u.lane] = (uint8_t)dm_clampi((int)v->lane_len[u.lane] + accel(s), 1, DM_MAXSTEPS); return;
        case U_MSRC: u.msrc = (uint8_t)dm_clampi((int)u.msrc + s, 0, DS_N - 1); return;
        case U_MTGT: u.mtgt = (uint8_t)dm_clampi((int)u.mtgt + s, 0, DT_N - 1); return;
        case U_MDEPTH: v->depth[u.msrc][u.mtgt] = dm_clampf(v->depth[u.msrc][u.mtgt] + 0.01f * (float)accel(s), -1.0f, 1.0f); return;
        case U_LTGT: u.ltgt = (uint8_t)dm_clampi((int)u.ltgt + s, 0, DT_N - 1); return;
        case U_LON:
            if (s > 0)
                v->locks |= 1u << u.ltgt;
            else
                v->locks &= ~(1u << u.ltgt);
            return;
        case U_KSLOT: u.kslot = (uint8_t)dm_clampi((int)u.kslot + s, 0, DM_NKITS - 1); u.confirm = 0xFF; return;
        case U_VSLOT: u.vslot = (uint8_t)dm_clampi((int)u.vslot + s, 0, DM_NVPRE - 1); u.confirm = 0xFF; return;
        case U_NPOS: u.npos = (uint8_t)dm_clampi((int)u.npos + s, 0, 10); return;
        case U_NCHAR: {
            static const char CS[] = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-#";
            int i = 0, c = dm_kit.name[u.npos] ? dm_kit.name[u.npos] : ' ';
            while (CS[i] && CS[i] != c)
                i++;
            i = (i + s + (int)(sizeof CS - 1u) * 8) % (int)(sizeof CS - 1u);
            dm_kit.name[u.npos] = CS[i] == ' ' ? ' ' : CS[i];
            {   /* no hole before the end: blanks up to the cursor */
                uint32_t k;
                for (k = 0; k < u.npos; k++)
                    if (!dm_kit.name[k])
                        dm_kit.name[k] = ' ';
            }
            return;
        }
        case U_PAL: dm_set.palette = (uint8_t)((dm_set.palette + NPALETTES * 4u + (uint32_t)s) % NPALETTES);
            palette_set(dm_set.palette);
            u.force = 1;
            return;
        case U_LEDS: dm_set.leds = (uint8_t)(s > 0); return;
        case U_USB: dm_set.usb_fixed = (uint8_t)(s > 0); usb_full = dm_set.usb_fixed; return;
        case U_AUTO: dm_set.rsv[0] = (uint8_t)((dm_set.rsv[0] & ~1u) | (s > 0 ? 0u : 1u)); return;
        case U_CLK: dm_m.clk = (uint8_t)(s > 0 ? DM_CLK_EXT : DM_CLK_INT); dm_set.clk = dm_m.clk; return;
        }
        return;
    default: return;
    }
}

/* the target a knob turns (for MAP and the locks): -1 when it is not a voice target */
static int pd_target(const pd_t *d) { return d->kind == PK_T ? (int)d->id : -1; }

/* ------------------------------------------------------- actions --- */
static void clip_copy(void)
{
    u.clip = *selv();
    u.clip_ok = 1;
    ui_msg("VOICE COPIED");
}
static void ui_action(uint32_t id, int32_t s)
{
    dm_vkit_t *v = selv();
    uint32_t i, n;
    switch (id) {
    case A_ROT_LANE:
    case A_ROT_TRIG: {                                   /* one step a click, either way */
        static int8_t tmp[DM_MAXSTEPS];
        int32_t k, dir = s > 0 ? 1 : -1, reps = s > 0 ? s : -s;
        n = id == A_ROT_LANE && v->lane_sync[u.lane] != LS_SYNC ? v->lane_len[u.lane] : seq_len(v);
        if (n < 2u)
            return;
        ui_dirty();
        for (k = 0; k < reps; k++) {
            if (id == A_ROT_LANE) {
                for (i = 0; i < n; i++)
                    tmp[i] = v->lane[u.lane][i];
                for (i = 0; i < n; i++)
                    v->lane[u.lane][((int32_t)i + dir + (int32_t)n) % (int32_t)n] = tmp[i];
            } else {
                for (i = 0; i < n; i++)
                    tmp[i] = (int8_t)dm_manual(v, i);
                for (i = 0; i < n; i++)
                    dm_manual_set(v, (uint32_t)(((int32_t)i + dir + (int32_t)n) % (int32_t)n), tmp[i]);
            }
        }
        return;
    }
    case A_CLR_LANE:
        if (s > 0) {
            dm_lane_clear(v, u.lane);
            ui_dirty();
            ui_msg("LANE CLEARED");
        }
        return;
    case A_CLR_TRIG:
        if (s > 0) {
            for (i = 0; i < 4u; i++)
                v->manual[i] = 0;
            ui_dirty();
            ui_msg("TRIGS CLEARED");
        }
        return;
    case A_COPY:
        if (s > 0)
            clip_copy();
        return;
    case A_PASTE:
        if (s > 0 && u.clip_ok) {                     /* the sound; the sequence, the mix and the locked controls stay */
            dm_voice_copy_sound(v, &u.clip);
            ui_dirty();
            ui_msg("VOICE PASTED");
        } else if (s > 0)
            ui_msg("NOTHING COPIED");
        return;
    case A_RESET:
        if (s > 0) {
            u.confirm = (uint8_t)id;
            ui_msg("OCT+: RESET THE VOICE");
        }
        return;
    case A_INIT:                                         /* NEW: right INIT, right again DAEL REF; left: none */
        if (s > 0) {
            u.confirm = (uint8_t)(u.confirm == A_INIT || u.confirm == A_REF ? A_REF : A_INIT);
            ui_msg(u.confirm == A_INIT ? "OCT+: NEW KIT (INIT)" : "OCT+: NEW KIT (DAEL REFERENCE)");
        } else
            u.confirm = 0xFF;
        return;
    case A_COMMIT:
        if (s > 0) {
            dm_perf_commit(&dm_kit);
            ui_dirty();
            ui_msg("PERFORMANCE COMMITTED");
        }
        return;
    case A_PFRESET:
        if (s > 0) {
            dm_perf_reset(&dm_kit);
            ui_dirty();
            ui_msg("PERFORMANCE RESET");
        }
        return;
    case A_PRESET:
        if (s > 0) {
            u.page = (uint8_t)PG_VOICES;
            u.force = 1;
        }
        return;
    default: return;
    }
}
static void ui_confirm(void)                            /* OCT+ */
{
    uint32_t id = u.confirm;
    u.confirm = 0xFF;
    if (id == A_RESET) {
        dm_voice_reset_sound(selv(), u.sel);
        ui_dirty();
        ui_msg("VOICE RESET");
    } else if (id == A_INIT || id == A_REF) {
        if (id == A_REF)
            dm_kit_reference(&dm_kit);
        else
            dm_kit_defaults(&dm_kit);
        ui_dirty();
        ui_msg(id == A_REF ? "DAEL REFERENCE KIT" : "NEW KIT");
    }
}

/* the dice of the page (black key 11) */
static uint32_t rng_seed = 0x2545F491u;
static void ui_dado(void)
{
    dm_rng_t r;
    const page_t *pg = &PAGES[u.page];
    r.s = rng_seed = dm_hash32(rng_seed + fm1_ms);
    ui_dirty();
    if (pg->btn == B_ARP) {
        dm_perf_randomize(&dm_kit, &r);
        ui_msg("DADO: PERFORMANCE");
    } else if (pg->btn == B_SEQ) {
        dm_lane_randomize(selv(), u.lane, &r);
        ui_msg("DADO: LANE");
    } else if (pg->btn == B_GLO) {
        dm_randomize_all(&dm_kit, &r);
        ui_msg("DADO: ALL VOICES");
    } else {
        dm_randomize_voice(&dm_kit, u.sel, &r);
        ui_msg("DADO: VOICE");
    }
}

/* ------------------------------------------------------ kits, slots --- */
static void slots_scan(void)
{
    uint32_t k;
    for (k = 0; k < DM_NKITS; k++) {
        u.kit_used[k] = (uint8_t)kit_used(k);
        kit_name(k, u.kit_names[k]);
    }
    for (k = 0; k < DM_NVPRE; k++)
        u.vpre_used[k] = (uint8_t)vpre_used(k);
    u.slots_known = 1;
}
static void kit_save_ui(void)
{
    int rc;
    if (u.kit_used[u.kslot] && u.confirm != 0xFE) {
        u.confirm = 0xFE;                               /* (0xFE: overwrite armed) */
        ui_msg("USED: OCT+ AGAIN TO OVERWRITE");
        return;
    }
    u.confirm = 0xFF;
    if (!dm_kit.name[0]) {
        str_cpy(dm_kit.name, "KIT ", sizeof dm_kit.name);
        ui_num(dm_kit.name + 4, u.kslot + 1);
    }
    rc = kit_save(u.kslot);
    if (rc == -6)
        ui_msg("KIT TOO BIG: CLEAR SOME LANES");
    else if (rc)
        ui_msg("SAVE FAILED");
    else {
        dm_set.last_kit = u.kslot;
        settings_save();
        u.dirty = 0;
        u.asave = 1;
        u.dirty_ms = fm1_ms;
        ui_msg("KIT SAVED");
    }
    slots_scan();
}
static void kit_load_ui(uint32_t k)
{
    if (!u.kit_used[k]) {
        ui_msg("EMPTY SLOT");
        return;
    }
    if (kit_load_into(k, &dm_kit))
        ui_msg("LOAD FAILED");
    else {
        u.kslot = (uint8_t)k;
        dm_set.last_kit = (uint8_t)k;
        u.dirty = 0;
        u.asave = 1;                                    /* (the autosave follows: the next power-on starts here) */
        u.dirty_ms = fm1_ms;
        ui_msg("KIT LOADED");
    }
}
static void vpre_save_ui(void)
{
    int rc;
    if (u.vpre_used[u.vslot] && u.confirm != 0xFE) {
        u.confirm = 0xFE;
        ui_msg("USED: OCT+ AGAIN TO OVERWRITE");
        return;
    }
    u.confirm = 0xFF;
    rc = vpre_save(u.vslot, selv());
    ui_msg(rc ? "SAVE FAILED" : "VOICE SAVED");
    slots_scan();
}
static void vpre_load_ui(uint32_t k)
{
    if (!u.vpre_used[k]) {
        ui_msg("EMPTY SLOT");
        return;
    }
    u.vslot = (uint8_t)k;
    if (vpre_load(k, selv()))
        ui_msg("LOAD FAILED");
    else {
        ui_dirty();
        ui_msg("VOICE LOADED");
    }
}

/* ------------------------------------------------------- the input --- */
static int page_in_group(uint32_t p, uint32_t btn) { return p < PG_MENU && PAGES[p].btn == btn; }
static void page_button(uint32_t b)
{
    uint32_t p, first = NPAGES;
    for (p = 0; p < PG_MENU; p++)
        if (PAGES[p].btn == b) {
            first = p;
            break;
        }
    if (first == NPAGES)
        return;
    u.confirm = 0xFF;
    if (page_in_group(u.page, b)) {                      /* again: the next page of the group */
        p = u.page + 1u;
        u.page = (uint8_t)(page_in_group(p, b) ? p : first);
    } else
        u.page = (uint8_t)first;
    u.force = 1;
}
static void page_step(int32_t s)                         /* PRESETS: within the group */
{
    uint32_t b = PAGES[u.page].btn, p = u.page;
    if (u.page == PG_MENU)
        return;
    while (s > 0 && page_in_group(p + 1u, b)) {
        p++;
        s--;
    }
    while (s < 0 && p > 0u && page_in_group(p - 1u, b)) {
        p--;
        s++;
    }
    u.page = (uint8_t)p;
}

static void play_pad(uint32_t place)                    /* PADS / REC: white key place 0..15 */
{
    dm_trig_t t;
    uint32_t v;
    static const int8_t SEMI[8] = {0, 2, 4, 5, 7, 9, 11, 12};
    t.vel = 100.0f / 127.0f;
    t.pitch = 0;
    t.envscale = 1;
    t.pan = t.var = t.sdly = t.srev = 0;
    t.macro2 = -1;
    t.accent = 0;
    if (place < 8u)
        v = place;
    else {
        v = u.sel;
        t.pitch = (float)(SEMI[place - 8u] + 12 * ((int)u.oct - 1));
    }
    {
        uint32_t f = irq_save();
        add_event(0, v, &t, 0);
        irq_restore(f);
    }
    if (u.keys == KM_REC && dm_m.playing && place < 8u) {
        int16_t st = dm_m.play_step[v];
        if (st >= 0) {
            dm_manual_set(&dm_kit.v[v], (uint32_t)st, 1);
            ui_dirty();
        }
    }
}

static uint32_t ui_now_btn(uint32_t b) { return (fm1_in.buttons >> panel.btn[b]) & 1u; }

static void ui_input(void)
{
    uint32_t released, pressed = fm1_input_edges(&released), notes = fm1_in.notes, nedge = fm1_input_note_edges();
    uint32_t b, k, hold_ms = 500u;
    int32_t s;
    dm_vkit_t *v = selv();
    if (pressed || nedge)
        u.last_input = fm1_ms;
    /* buttons */
    for (b = 0; b < NB; b++) {
        uint32_t bit = 1u << panel.btn[b];
        if (pressed & bit) {
            u.bdown[b] = fm1_ms | 1u;
            if (b == B_SAVE)
                u.save_used = 0;
            if (b == B_SCL)
                u.scl_used = 0;
            if (b == B_HOME)
                u.home_used = 0;
            switch (b) {
            case B_PLAY:
                if (dm_m.clk == DM_CLK_EXT && !dm_m.playing)
                    ui_msg("CLOCK EXT: WAITING FOR START");
                dm_m.want_play = !dm_m.playing;
                break;
            case B_REC:
                u.keys = (uint8_t)((u.keys + 1u) % 3u);
                ui_msg(u.keys == KM_PADS ? "PADS: 1-8 VOICES, 9-16 PITCH" : u.keys == KM_REC ? "REC: PADS WRITE TRIGS" : "STEPS");
                break;
            case B_OCTDN:
                if (u.page == PG_KITS || u.page == PG_VOICES || u.confirm != 0xFF) {
                    u.confirm = 0xFF;
                    if (u.page == PG_KITS || u.page == PG_VOICES)
                        u.page = 0;
                } else if (u.keys != KM_STEPS)
                    u.oct = (uint8_t)(u.oct ? u.oct - 1u : 0u);
                else if (u.spage)
                    u.spage--;
                break;
            case B_OCTUP:
                if (u.confirm != 0xFF && u.confirm != 0xFE)
                    ui_confirm();
                else if (u.page == PG_KITS)
                    kit_save_ui();
                else if (u.page == PG_VOICES)
                    vpre_save_ui();
                else if (u.keys != KM_STEPS)
                    u.oct = (uint8_t)(u.oct < 3u ? u.oct + 1u : 3u);
                else if (u.spage + 1u < npages_steps(v))
                    u.spage++;
                break;
            case B_HOME:
            case B_SAVE:
            case B_SCL:
                break;                                   /* (on release: they are modifiers when held) */
            default:
                if (u.page == PG_MENU) {
                    u.page = 0;
                    settings_save();
                }
                page_button(b);
                break;
            }
        }
        if (released & bit) {
            uint32_t held = u.bdown[b] ? fm1_ms - (u.bdown[b] & ~1u) : 0u;
            u.bdown[b] = 0;
            if (b == B_HOME && !u.home_used) {
                if (u.page == PG_MENU) {
                    u.page = 0;
                    settings_save();
                } else if (held < hold_ms)
                    page_button(B_HOME);
            }
            if (b == B_SAVE && !u.save_used && held < hold_ms) {
                if (!u.slots_known)
                    slots_scan();
                page_button(B_SAVE);
            }
            if (b == B_SCL && !u.scl_used && held < hold_ms)
                page_button(B_SCL);
            u.force = 1;
        }
    }
    if (u.bdown[B_HOME] && !u.home_used && fm1_ms - (u.bdown[B_HOME] & ~1u) >= hold_ms && u.page != PG_MENU) {
        u.home_used = 1;
        u.page = (uint8_t)PG_MENU;
        u.force = 1;
    }
    /* black keys */
    for (k = 0; k < 27u; k++) {
        uint32_t bit = 1u << k, p;
        if (!key_black(k))
            continue;
        p = key_place(k);
        if (nedge & bit) {
            if (p < 8u) {
                uint32_t mods = ((notes >> black_key(8)) & 1u) | ((notes >> black_key(9)) & 1u) << 1 |
                                ((notes >> black_key(10)) & 1u) << 2;
                if (mods & 1u) {
                    dm_kit.v[p].mute = !dm_kit.v[p].mute;
                    u.blk_used |= 1u;
                } else if (mods & 2u) {
                    dm_kit.v[p].solo = !dm_kit.v[p].solo;
                    u.blk_used |= 2u;
                } else if (mods & 4u) {
                    dm_rng_t r;
                    r.s = rng_seed = dm_hash32(rng_seed + fm1_ms);
                    dm_randomize_voice(&dm_kit, p, &r);
                    u.blk_used |= 4u;
                    ui_msg("DADO: VOICE");
                } else {
                    u.sel = (uint8_t)p;
                    if (u.spage >= npages_steps(&dm_kit.v[p]))
                        u.spage = 0;
                    u.held = 0;
                }
                ui_dirty();
            } else
                u.blk_used &= (uint8_t)~(1u << (p - 8u));
        }
    }
    for (k = 8; k < 11u; k++) {                          /* MUTE / SOLO / DADO released: a tap acts */
        uint32_t key = black_key(k), bit = 1u << key;
        static uint32_t was;
        if ((was & bit) && !(notes & bit) && !(u.blk_used & (1u << (k - 8u)))) {
            if (k == 8u)
                v->mute = !v->mute;
            else if (k == 9u)
                v->solo = !v->solo;
            else
                ui_dado();
            ui_dirty();
        }
        if (!(notes & bit))
            u.blk_used &= (uint8_t)~(1u << (k - 8u));
        was = (was & ~bit) | (notes & bit);
    }
    /* white keys */
    {
        uint32_t held = 0, p;
        static uint32_t was_white;
        for (k = 0; k < 27u; k++) {
            if (key_black(k))
                continue;
            p = key_place(k);
            if ((notes >> k) & 1u)
                held |= 1u << p;
            if ((nedge >> k) & 1u) {
                if (u.page == PG_KITS)
                    kit_load_ui(p);
                else if (u.page == PG_VOICES)
                    vpre_load_ui(p);
                else if (u.keys != KM_STEPS)
                    play_pad(p);
            }
        }
        if (u.keys == KM_STEPS && u.page != PG_KITS && u.page != PG_VOICES) {
            uint32_t up = was_white & ~held;
            if (!u.held)
                u.edited = 0;
            for (p = 0; p < 16u; p++)
                if ((up >> p) & 1u && !u.edited) {      /* a tap: a trig in or out */
                    uint32_t st = u.spage * 16u + p;
                    if (st < seq_len(v)) {
                        dm_manual_set(v, st, !dm_manual(v, st));
                        ui_dirty();
                    }
                }
            u.held = held;
            if (!held)
                u.edited = 0;
        } else
            u.held = 0;
        was_white = held;
    }
    /* knobs */
    s = panel_enc(EN_SELECT);
    if (s && dm_m.clk == DM_CLK_INT) {
        dm_kit.g.bpm = dm_clampf(dm_roundf(dm_kit.g.bpm) + (float)accel(s), 40.0f, 300.0f);
        ui_dirty();
    }
    s = panel_enc(EN_ALGO);
    if (s) {
        u.sel = (uint8_t)dm_clampi((int)u.sel + s, 0, DM_NV - 1);
        u.held = 0;
        if (u.spage >= npages_steps(selv()))
            u.spage = 0;
    }
    v = selv();
    s = panel_enc(EN_PRESET);
    if (s) {
        if (u.held)
            u.lgrp = (uint8_t)dm_clampi((int)u.lgrp + s, 0, 2);
        else
            page_step(s);
        u.confirm = 0xFF;
    }
    for (k = 0; k < 4u; k++) {
        const pd_t *d = &PAGES[u.page].k[k];
        s = panel_enc(EN_K1 + k);
        if (!s)
            continue;
        u.hot_col = k;
        u.hot_t = fm1_ms + 800u;
        if (u.held) {                                     /* steps held: their lanes */
            uint32_t l = LGRP[u.lgrp][k], p;
            if (l == 0xFFu)
                continue;
            for (p = 0; p < 16u; p++)
                if ((u.held >> p) & 1u) {
                    uint32_t st = u.spage * 16u + p;
                    int x = v->lane[l][st] + (l == L_VEL || l == L_SHIFT || l == L_ENV || l == L_PAN || l == L_PROB ||
                                              l == L_SENDS || l == L_MACRO2 ? accel(s) : s);
                    v->lane[l][st] = (int8_t)dm_clampi(x, DM_LANE[l].lo, DM_LANE[l].hi);
                }
            u.edited = 1;
            ui_dirty();
            continue;
        }
        if (u.bdown[B_SAVE]) {                            /* SAVE held: lock / unlock */
            int t = pd_target(d);
            u.save_used = 1;
            if (d->kind == PK_PF) {
                dm_kit.g.perf_lock ^= 1u << d->id;
                ui_msg((dm_kit.g.perf_lock >> d->id) & 1u ? "OUT OF THE DADO" : "IN THE DADO");
            } else if (t >= 0) {
                v->locks ^= 1u << t;
                ui_msg((v->locks >> t) & 1u ? "LOCKED" : "UNLOCKED");
            }
            continue;
        }
        if (u.bdown[B_SCL] && u.page != PG_MENU) {         /* SCL held: MAP the source onto it */
            int t = pd_target(d);
            u.scl_used = 1;
            if (t >= 0) {
                u.mtgt = (uint8_t)t;
                v->depth[u.msrc][t] = dm_clampf(v->depth[u.msrc][t] + 0.01f * (float)accel(s), -1.0f, 1.0f);
                ui_dirty();
            }
            continue;
        }
        pd_turn(d, s);
    }
}

/* -------------------------------------------------------------- LEDs --- */
static void led_set(uint32_t id, int lit, int dim)
{
    uint32_t p, r;
    for (p = 0; p < FM1_NCOL; p++)
        for (r = 1; r < 5u; r++)
            if (FM1_KEYMAP[r][p] == (int8_t)id) {
                uint8_t m = (uint8_t)(1u << r);
                fm1_led[p] = (uint8_t)(lit ? fm1_led[p] | m : fm1_led[p] & ~m);
                fm1_led_dim[p] = (uint8_t)(dim && !dm_set.leds ? fm1_led_dim[p] | m : fm1_led_dim[p] & ~m);
            }
}
static void ui_leds(void)
{
    const dm_vkit_t *v = selv();
    uint32_t k, b, L = seq_len(v);
    if (fm1_led_anim_on())
        return;
    for (k = 0; k < 27u; k++) {
        uint32_t p = key_place(k), id = 14u + k;
        if (key_black(k)) {
            if (p < 8u) {
                int fl = u.flash[p] != 0;
                led_set(id, (p == u.sel) ^ fl, !dm_kit.v[p].mute);
            } else if (p == 8u)
                led_set(id, v->mute, 1);
            else if (p == 9u) {
                uint32_t any = 0, i;
                for (i = 0; i < DM_NV; i++)
                    any |= dm_kit.v[i].solo;
                led_set(id, (int)any, 1);
            } else
                led_set(id, 0, 1);
            continue;
        }
        if (u.page == PG_KITS || u.page == PG_VOICES) {
            int used = u.page == PG_KITS ? u.kit_used[p] : u.vpre_used[p];
            led_set(id, p == (u.page == PG_KITS ? u.kslot : u.vslot), used);
            continue;
        }
        if (u.keys != KM_STEPS) {
            int fl = p < 8u ? u.flash[p] != 0 : 0;
            led_set(id, fl || (p >= 8u && 0), 1);
            continue;
        }
        {
            uint32_t st = u.spage * 16u + p;
            uint32_t es = u.esteps[u.sel] ? u.esteps[u.sel] : 1u;
            int hit = st < L && (dm_manual(v, st) || u.hits[u.sel][st % es]);
            int head = dm_m.playing && dm_m.play_step[u.sel] == (int16_t)st;
            led_set(id, (st < L) && (hit ^ head), st < L);
        }
    }
    for (b = 0; b < NB; b++) {
        uint32_t id = panel.btn[b];
        int lit = 0;
        if (b == B_PLAY)
            lit = dm_m.playing;
        else if (b == B_REC)
            lit = u.keys == KM_PADS ? 1 : u.keys == KM_REC ? (int)((fm1_ms >> 8) & 1u) : 0;
        else if (b == B_OCTDN)
            lit = u.keys == KM_STEPS ? u.spage > 0u : u.oct < 1u;
        else if (b == B_OCTUP)
            lit = u.keys == KM_STEPS ? u.spage + 1u < npages_steps(v) : u.oct > 1u;
        else
            lit = PAGES[u.page].btn == b;
        led_set(id, lit, 1);
    }
}

/* ------------------------------------------------------------ screen --- */
static uint32_t sig(const void *p, uint32_t n, uint32_t h)
{
    const uint8_t *b = p;
    while (n--)
        h = (h ^ *b++) * 16777619u;
    return h;
}
static void hits_update(void)
{
    uint32_t v;
    for (v = 0; v < DM_NV; v++) {
        const dm_vkit_t *x = &dm_kit.v[v];
        uint32_t h = (uint32_t)(uint8_t)x->steps | (uint32_t)(uint8_t)x->fill << 8 | (uint32_t)(uint8_t)x->pattern << 16 |
                     (uint32_t)(uint8_t)x->rotate << 24;
        h ^= (uint32_t)x->invert * 0x9E3779B1u;
        h = sig(dm_kit.g.perf + PF_STEPS, 3u * sizeof(float), h);
        if (h != u.hsig[v]) {
            u.hsig[v] = h;
            u.esteps[v] = (uint8_t)dm_machine_hits(v, u.hits[v], 64);
        }
    }
}
static const char *const SHORT[ENG_COUNT] = {"KICK", "SNARE", "HAT", "CLAP", "FM", "MODAL", "COWBL", "ZAP", "PILLW",
                                             "TRIPW", "RADTR", "KOMB", "LPG", "BUILD", "ADDTV", "CHAOS", "VOSIM",
                                             "SUMSY", "RESON", "COMBI", "TERRN", "SHAME", "ENTGL", "HAYWR", "SCRAP"};
static void draw_top(void)
{
    char b[20];
    uint32_t h = sig(dm_kit.name, sizeof dm_kit.name, 7u), cpu = (dm_m.cpu_q8 * 100u + 128u) / 256u;
    float bpm = dm_m.bpm_now;
    h = sig(&bpm, sizeof bpm, h);
    { uint8_t pl = dm_m.playing; h = sig(&pl, 1, h); }
    h = sig(&cpu, sizeof cpu, h);
    h = sig(&u.dirty, 1, h);
    { uint8_t ck = dm_m.clk; h = sig(&ck, 1, h); }
    if (!u.force && h == u.sig_top)
        return;
    u.sig_top = h;
    cv_begin(240, LC_TOP, T_BG);
    cv_text_fit(8, 3, &AF_M, dm_kit.name[0] ? dm_kit.name : "DRUMMATIC", dm_kit.name[0] ? T_TEXT : T_THEME, T_BG, 110);
    if (u.dirty)
        cv_rect(dm_kit.name[0] ? 8 + text_w(&AF_M, dm_kit.name) + 3 : 120, 8, 4, 4, T_MID);
    ui_fix(b, bpm, bpm < 100.0f ? 1 : 0);
    cv_text_r(186, 3, &AF_M, b, T_THEME, T_BG);
    cv_text(190, 7, &AF_S, dm_m.clk ? "EXT" : "BPM", T_MID);
    cv_rect(228, 8, 8, 8, dm_m.playing ? T_ACCENT : T_LINE);
    ui_num(b, (int32_t)cpu);
    str_cpy(b + str_len(b), "%", 4);
    cv_text_r(150, 7, &AF_S, b, cpu > 80u ? T_REC : T_DIM, T_BG);
    cv_rule(0, LC_TOP - 1, 240, 1);
    cv_blit(0, 0);
}
static void draw_row(uint32_t r)
{
    const dm_vkit_t *v = &dm_kit.v[r];
    uint32_t L = seq_len(v), st, x, h, sel = r == u.sel, i;
    int16_t head = dm_m.play_step[r];
    uint16_t c = vcol(r), bg = sel ? T_SURF : T_BG;
    uint8_t lv = (uint8_t)dm_clampi((int)(dm_m.level[r] * 24.0f), 0, 12);
    char b[4];
    h = sig(v->manual, sizeof v->manual, 11u + r);
    h = sig(u.hits[r], 16, h);
    h = sig(&head, 2, h);
    h = sig(&lv, 1, h);
    st = (uint32_t)u.flash[r] | sel << 8 | u.spage << 9 | (uint32_t)v->mute << 13 | (uint32_t)v->solo << 14 |
         (uint32_t)v->engine << 16 | (uint32_t)(u.held & 0xFFFFu) * sel;
    h = sig(&st, 4, h);
    h = sig(&L, 4, h);
    i = v->steps;
    h = sig(&i, 4, h);
    if (!u.force && h == u.sig_row[r])
        return;
    u.sig_row[r] = h;
    cv_begin(240, LC_ROWS, bg);
    b[0] = (char)('1' + r);
    b[1] = 0;
    cv_text_on(5, 0, &AF_S, b, v->mute ? T_DIM : u.flash[r] ? T_ACCENT : c, bg);
    cv_text_on(18, 0, &AF_S, SHORT[v->engine % ENG_COUNT], v->mute ? T_DIM : v->solo ? T_ACCENT : T_MID, bg);
    for (i = 0; i < 16u; i++) {
        uint32_t s2 = u.spage * 16u + i, steps = u.esteps[r] ? u.esteps[r] : 1u;
        int hit = s2 < L && (dm_manual(v, s2) || u.hits[r][s2 % steps]);
        int man = s2 < L && dm_manual(v, s2);
        int hd = dm_m.playing && head == (int16_t)s2;
        uint16_t col = hd ? T_ACCENT : hit ? (man ? T_TEXT : c) : s2 < L ? T_LINE : bg;
        x = 66u + i * 10u;
        if (sel && ((u.held >> i) & 1u))
            cv_frame((int32_t)x - 1, 1, 10, 13, T_ACCENT);
        cv_rect((int32_t)x, hit ? 3 : 6, 8, hit ? 9 : 3, col);
    }
    cv_rect(230, 13 - lv, 4, lv ? lv : 0, c);
    cv_blit(0, LC_TOP + r * LC_ROWS);
}
static void draw_bottom(void)
{
    char b[24];
    uint32_t k, h;
    const page_t *pg = &PAGES[u.page];
    const dm_vkit_t *v = selv();
    float vals[4];
    h = sig(&u.page, 1, 3u);
    h = sig(&u.sel, 1, h);
    h = sig(&u.held, 4, h);
    h = sig(&u.lgrp, 1, h);
    for (k = 0; k < 4u; k++) {
        vals[k] = pd_get(&pg->k[k]);
        h = sig(&vals[k], 4, h);
    }
    if (u.held) {
        uint32_t p = 0;
        while (!((u.held >> p) & 1u))
            p++;
        h = sig(&v->lane, sizeof v->lane[0] * DM_NLANE, h);
        h = sig(&p, 4, h);
    }
    k = (u.msg_t && fm1_ms < u.msg_t) | (u.hot_t && fm1_ms < u.hot_t) << 1 | u.hot_col << 2 | (uint32_t)u.confirm << 4 |
        (uint32_t)u.keys << 12 | (uint32_t)u.spage << 14 | (uint32_t)u.bdown[B_SCL] * 0 | (u.bdown[B_SCL] != 0) << 20 |
        (u.bdown[B_SAVE] != 0) << 21 | (uint32_t)u.msrc << 22;
    h = sig(&k, 4, h);
    h = sig(u.msg, sizeof u.msg, h);
    h = sig(&v->engine, 1, h);
    h = sig(&v->locks, 4, h);
    h = sig(&u.kslot, 1, h);
    h = sig(&u.vslot, 1, h);
    h = sig(dm_kit.name, 12, h);
    h = sig(v->depth, sizeof v->depth, h);
    if (!u.force && h == u.sig_bot)
        return;
    u.sig_bot = h;
    /* strip 1: the title and the knobs' names */
    cv_begin(240, 48, T_BG);
    cv_rule(0, 0, 240, 1);
    b[0] = (char)('1' + u.sel);
    b[1] = ' ';
    str_cpy(b + 2, DM_ENG_INFO[v->engine % ENG_COUNT].name, sizeof b - 2u);
    cv_text(8, 5, &AF_M, b, vcol(u.sel));
    if (u.held) {
        static const char *const GN[3] = {"STEP: VEL PITCH", "STEP: SHIFT ENV", "STEP: SENDS M2"};
        cv_text_r(232, 5, &AF_M, GN[u.lgrp], T_ACCENT, T_BG);
    } else
        cv_text_r(232, 5, &AF_M, u.page == PG_MENU ? "MENU" : pg->name,
                  u.bdown[B_SCL] ? T_ACCENT : T_THEME, T_BG);
    for (k = 0; k < 4u; k++) {
        const pd_t *d = &pg->k[k];
        const char *lab = d->label;
        int32_t x = (int32_t)k * 60;
        int t = pd_target(d);
        if (u.held) {
            uint32_t l = LGRP[u.lgrp][k];
            lab = l == 0xFFu ? "" : DM_LANE[l].name;
        } else if (d->kind == PK_T && !lab)
            lab = DM_ENG_INFO[v->engine % ENG_COUNT].k[d->id - T_P1];
        else if (d->kind == PK_PF)
            lab = DM_PF_NAME[d->id];
        if (!lab || !lab[0])
            continue;
        cv_text_in(x, 30, 60, &AF_S, lab, !u.held && t >= 0 && ((v->locks >> t) & 1u) ? T_REC :
                   (!u.held && d->kind == PK_PF && ((dm_kit.g.perf_lock >> d->id) & 1u)) ? T_REC : T_MID, T_BG);
    }
    cv_blit(0, LC_BOT);
    /* strip 2: the values, the gauges, the line under them */
    cv_begin(240, 48, T_BG);
    for (k = 0; k < 4u; k++) {
        const pd_t *d = &pg->k[k];
        int32_t x = (int32_t)k * 60;
        uint16_t vc = u.hot_t && fm1_ms < u.hot_t && u.hot_col == k ? T_ACCENT : T_TEXT;
        float fr;
        if (u.held) {
            uint32_t l = LGRP[u.lgrp][k], p = 0;
            if (l == 0xFFu)
                continue;
            while (!((u.held >> p) & 1u))
                p++;
            ui_num(b, v->lane[l][u.spage * 16u + p]);
            cv_text_in(x, 0, 60, &AF_M, b, T_ACCENT, T_BG);
            fr = (float)(v->lane[l][u.spage * 16u + p] - DM_LANE[l].lo) / (float)(DM_LANE[l].hi - DM_LANE[l].lo);
            cv_rect(x + 6, 24, 48, 3, T_LINE);
            cv_rect(x + 6, 24, (int32_t)(48.0f * dm_clamp01(fr)), 3, T_ACCENT);
            continue;
        }
        if (d->kind == PK_NONE)
            continue;
        if (u.bdown[B_SCL] && pd_target(d) >= 0) {         /* MAP: the depths of the source */
            float dd = v->depth[u.msrc][pd_target(d)];
            ui_num(b, (int32_t)(dd * 100.0f + (dd < 0 ? -0.5f : 0.5f)));
            cv_text_in(x, 0, 60, &AF_M, b, dd != 0.0f ? T_ACCENT : T_DIM, T_BG);
            continue;
        }
        pd_text(d, vals[k], b, sizeof b);
        if (text_w(&AF_M, b) <= 58)
            cv_text_in(x, 0, 60, &AF_M, b, vc, T_BG);
        else
            cv_text_in(x, 3, 60, &AF_S, b, vc, T_BG);
        fr = pd_frac(d, vals[k]);
        if (fr >= 0.0f) {
            cv_rect(x + 6, 24, 48, 3, T_LINE);
            cv_rect(x + 6, 24, (int32_t)(48.0f * dm_clamp01(fr)), 3, vc == T_ACCENT ? T_ACCENT : vcol(u.sel));
            if (d->kind == PK_T) {                         /* a dot: the control is modulated */
                uint32_t s2;
                for (s2 = 0; s2 < DS_N; s2++)
                    if (v->depth[s2][d->id] != 0.0f) {
                        cv_rect(x + 56, 23, 3, 5, T_THEME);
                        break;
                    }
            }
        }
    }
    if (u.msg_t && fm1_ms < u.msg_t)
        cv_text_fit(8, 32, &AF_S, u.msg, T_THEME, T_BG, 224);
    else if (u.page == PG_KITS) {
        char nb[24];
        str_cpy(nb, "SLOT ", sizeof nb);
        ui_num(nb + 5, u.kslot + 1);
        str_cpy(nb + str_len(nb), ": ", sizeof nb - str_len(nb));
        str_cpy(nb + str_len(nb), u.kit_used[u.kslot] ? (u.kit_names[u.kslot][0] ? u.kit_names[u.kslot] : "USED") : "EMPTY",
                sizeof nb - str_len(nb));
        cv_text_fit(8, 32, &AF_S, nb, T_TEXT, T_BG, 150);
        cv_text_r(232, 32, &AF_S, "KEY LOAD  OCT+ SAVE", T_DIM, T_BG);
    } else if (u.page == PG_VOICES)
        cv_text(8, 32, &AF_S, "KEY: LOAD INTO VOICE   OCT+: SAVE IT", T_DIM);
    else if (u.keys != KM_STEPS)
        cv_text(8, 32, &AF_S, u.keys == KM_PADS ? "PADS  1-8 VOICES  9-16 PITCH" : "REC  PADS WRITE TRIGS",
                u.keys == KM_REC ? T_REC : T_THEME);
    else if (u.page == 12u) {                              /* MOD: the routes of the source */
        char r[48];
        uint32_t t, n = 0;
        r[0] = 0;
        for (t = 0; t < DT_N && n < 40u; t++)
            if (v->depth[u.msrc][t] != 0.0f) {
                const char *nm = t >= T_P1 && t <= T_P4 ? DM_ENG_INFO[v->engine % ENG_COUNT].k[t - T_P1] : DM_TNAME[t];
                if (n)
                    r[n++] = ' ';
                str_cpy(r + n, nm, sizeof r - n);
                n = str_len(r);
            }
        cv_text_fit(8, 32, &AF_S, n ? r : "SCL HELD + KNOB: MAP", n ? T_THEME : T_DIM, T_BG, 224);
    } else {
        char sb[24];
        uint32_t np = npages_steps(v);
        str_cpy(sb, "STEPS ", sizeof sb);
        ui_num(sb + 6, (int32_t)(u.spage * 16u + 1u));
        str_cpy(sb + str_len(sb), "-", sizeof sb - str_len(sb));
        ui_num(sb + str_len(sb), (int32_t)(u.spage * 16u + 16u));
        if (np > 1u) {
            str_cpy(sb + str_len(sb), " /", sizeof sb - str_len(sb));
            ui_num(sb + str_len(sb), (int32_t)seq_len(v));
        }
        cv_text(8, 32, &AF_S, sb, T_DIM);
    }
    cv_blit(0, LC_BOT + 48);
}
static void ui_draw(void)
{
    uint32_t v;
    for (v = 0; v < DM_NV; v++) {
        if (dm_m.trig_count[v] != u.seen[v]) {
            u.seen[v] = dm_m.trig_count[v];
            u.flash[v] = 5;
        } else if (u.flash[v])
            u.flash[v]--;
    }
    hits_update();
    draw_top();
    for (v = 0; v < DM_NV; v++)
        draw_row(v);
    draw_bottom();
    u.force = 0;
}

/* ------------------------------------------------------------ actions from MIDI channel 11 --- */
static void ui_machine_actions(void)
{
    int a;
    dm_rng_t r;
    while ((a = dm_machine_action()) != DA_NONE) {
        r.s = rng_seed = dm_hash32(rng_seed + fm1_ms + (uint32_t)a);
        if (a == DA_PREV_KIT || a == DA_NEXT_KIT || a == DA_RANDOM_KIT) {
            uint32_t k, n = 0, list[DM_NKITS], cur = 0;
            if (!u.slots_known)
                slots_scan();
            for (k = 0; k < DM_NKITS; k++)
                if (u.kit_used[k]) {
                    if (k == dm_set.last_kit)
                        cur = n;
                    list[n++] = k;
                }
            if (!n)
                continue;
            k = a == DA_RANDOM_KIT ? dm_rng_next(&r) % n : (cur + (a == DA_NEXT_KIT ? 1u : n - 1u)) % n;
            kit_load_ui(list[k]);
        } else if (a == DA_RANDOMIZE_ALL)
            dm_randomize_all(&dm_kit, &r);
        else if (a == DA_PERF_RANDOM)
            dm_perf_randomize(&dm_kit, &r);
        else if (a == DA_PERF_RESET)
            dm_perf_reset(&dm_kit);
        else if (a == DA_PERF_COMMIT)
            dm_perf_commit(&dm_kit);
        else if (a >= DA_RANDOMIZE_V0 && a < DA_RANDOMIZE_V0 + DM_NV)
            dm_randomize_voice(&dm_kit, (uint32_t)(a - DA_RANDOMIZE_V0), &r);
        ui_dirty();
    }
}

/* the autosave: the kit changed, the machine stopped, nothing touched for 4 s */
static void ui_autosave(void)
{
    if (!u.asave || (dm_set.rsv[0] & 1u) || dm_m.playing || fm1_ms - u.dirty_ms < 4000u || fm1_ms - u.last_input < 4000u)
        return;
    if (kit_save(0xFFu) == 0)
        u.asave = 0;
    else
        u.dirty_ms = fm1_ms;                              /* (try again later) */
}

static void ui_init(void)
{
    uint32_t i;
    for (i = 0; i < sizeof u; i++)
        ((uint8_t *)&u)[i] = 0;
    u.confirm = 0xFF;
    u.oct = 1;
    u.force = 1;
    for (i = 0; i < DM_NV; i++)
        u.hsig[i] = 0xFFFFFFFFu;
}
