/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 in the browser: the firmware's own sources (the audio engine, the panel's UI, the kit store) against
 * a simulated FM-1, compiled to WebAssembly and run in an AudioWorklet. After Felucca's web/emu/felucca_web.c
 * (Leo Kuroshita, GPL-3.0; itself after charlesvestal's X0X): the device clock runs with the audio the worklet asks
 * for, an I2S half is rendered as the DMA would ask for it, the main loop runs every 15 ms, the panel is the printed
 * labels, the screen an RGB565 frame, the flash a 1 MiB NOR the page keeps.
 *   web/build.sh  ->  build/web/{drummatic.wasm, worklet.js, index.html} */
#include <stdint.h>
#define FELUCCA_WEB 1
#define FELUCCA_UAC 0
#define FELUCCA_UART 0
#define FELUCCA_OTA 0
#define FELUCCA_CDC 0

void *memset(void *d, int c, unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);
#define __attribute__(x)
#include "../firmware/src/app/dm_core.h"
#undef __attribute__
#define FM1_TICKS_PER_US 24u
static uint32_t web_ticks;
static uint32_t fm1_ticks(void) { return web_ticks; }

/* ---------------------------------------------------------------- libc --- */
#define memset dm_web_memset
#define memcpy dm_web_memcpy
#define memcmp dm_web_memcmp
#include "../firmware/src/platform/libc.c"
#undef memset
#undef memcpy
#undef memcmp
void *memset(void *d, int c, unsigned long n) { return dm_web_memset(d, c, (unsigned)n); }
void *memcpy(void *d, const void *s, unsigned long n) { return dm_web_memcpy(d, s, (unsigned)n); }
int memcmp(const void *a, const void *b, unsigned long n) { return dm_web_memcmp(a, b, (unsigned)n); }

/* ------------------------------------------------------------ the panel --- */
#define FM1_NCOL 11u
static const int8_t FM1_KEYMAP[6][FM1_NCOL] = {      /* as hal/fm1_input.h */
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
    { 5, 11,  4, 10,  3,  9,  2,  8, -1, -1, -1},
    {34, 35, 36, 37, 38, 40, 39, 13,  7,  6, 12},
    {23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33},
    { 0,  1, 15, 14, 17, 16, 19, 18, 20, 21, 22},
    {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1},
};
static uint8_t fm1_led[FM1_NCOL], fm1_led_dim[FM1_NCOL];
static struct { uint32_t notes, buttons; } fm1_in;
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
static int fm1_led_anim_on(void) { return 0; }

/* ------------------------------------------------------------ the screen --- */
static uint16_t web_fb[240 * 240];
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
#define __attribute__(x)
#include "../firmware/src/platform/gfx.c"
#include "../firmware/src/app/dm_panel.c"
#undef __attribute__

/* ------------------------------------------------------------ the engine --- */
#define DM_POOL
#include "../firmware/src/dm/dm_math.c"
#include "../firmware/src/dm/dm_engines.c"
#include "../firmware/src/dm/dm_voice.c"
#include "../firmware/src/dm/dm_fx.c"
#include "../firmware/src/dm/dm_kit.c"
#include "../firmware/src/dm/dm_machine.c"

/* the MIDI ring of usb.c, as the audio ISR reads it */
#define MQ 64u
static uint32_t midi_in_q[MQ];
static volatile uint32_t mi_w, mi_r;
static volatile uint8_t midi_in_overflow;

/* ----------------------------------------------------- the audio "ISR" --- */
#define FM1_AUDIO_HALF 1u
#define MIDI_SRC(at) 1u                    /* (one source: Web MIDI) */
static uint32_t web_half;
static uint8_t fm1_audio_pending(void) { return FM1_AUDIO_HALF; }
static void fm1_audio_ack_aux(uint8_t p) { (void)p; }
static uint32_t fm1_audio_free_half(void) { return web_half; }
static void fm1_audio_ack_half(void) {}
static void fm1_audio_init(int32_t *buf, uint32_t words, void (*isr)(void), uint32_t prio)
{ (void)buf; (void)words; (void)isr; (void)prio; }
void isr_alnk0(void) {}
#define __attribute__(x)
#include "../firmware/src/app/dm_audio.c"
#undef __attribute__

/* -------------------------------------------------------------- the flash --- */
static uint8_t nor[0x100000];
static uint32_t web_flash_writes;
static uint8_t flash_ok = 1;
static uint32_t irq_save(void) { return 0; }
static void irq_restore(uint32_t f) { (void)f; }
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
static int st_prog(uint32_t off, const void *src, uint32_t n)
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
#include "../firmware/src/app/dm_store.c"
#include "../firmware/src/app/dm_params.c"
#include "../firmware/src/app/dm_ui.c"

/* ------------------------------------------------------------ the device --- */
static uint32_t web_master_adc = 700, web_booted, web_last_frame;
static float master_of_pot(uint32_t a)               /* (dm_main.c) */
{
    float x = (float)(a > 1000u ? 1000u : a) * (1.0f / 1000.0f);
    if (x < 0.01f)
        return 0.0f;
    return dm_db2gain(-60.0f * (1.0f - x) * (1.0f - x));
}

#define RING 4096u
static float ring[RING * 2];
static uint32_t ring_w, ring_r, web_samples_due;

static void web_frame(void)
{
    master_gain = master_of_pot(web_master_adc);
    ui_input();
    ui_machine_actions();
    ui_leds();
    ui_draw();
    ui_autosave();
    settings_poll();
}
static void web_ms(void)
{
    fm1_ms++;
    web_ticks += 1000u * FM1_TICKS_PER_US;
    web_samples_due += FS;
    while (web_samples_due >= 1000u * HALF_FRAMES) {
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
    if (!web_booted)
        return;
    if (fm1_ms - web_last_frame >= 15u) {
        web_last_frame = fm1_ms;
        web_frame();
    } else
        ui_input();
}

#define EXPORT __attribute__((used, visibility("default")))
static float out_l[1024], out_r[1024];
EXPORT float *web_out_l(void) { return out_l; }
EXPORT float *web_out_r(void) { return out_r; }
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
EXPORT uint8_t *web_nor(void) { return nor; }
EXPORT uint32_t web_nor_size(void) { return sizeof nor; }
EXPORT void web_nor_erase(void) { memset(nor, 0xFF, sizeof nor); }
EXPORT uint32_t web_flash_writes_count(void) { return web_flash_writes; }
EXPORT void web_boot(void)
{
    settings_load();
    palette_set(dm_set.palette % NPALETTES);
    panel_init();
    ui_init();
    dm_machine_init();
    dm_kit_defaults(&dm_kit);
    if ((dm_set.rsv[0] & 1u) || kit_load_into(0xFFu, &dm_kit) != 0) {   /* the autosave, else the last kit */
        if (dm_set.last_kit < DM_NKITS && kit_load_into(dm_set.last_kit, &dm_kit) == 0)
            u.kslot = dm_set.last_kit;
    }
    dm_m.clk = dm_set.clk;
    master_gain = master_of_pot(web_master_adc);
    soft_fade_in();
    audio_init();
    lcd_fill(0, 0, 240, 240, T_BG);
    slots_scan();
    web_last_frame = fm1_ms;
    web_booted = 1;
}
/* the buttons by their printed labels (bit = B_*), the keys as fm1_in.notes */
static uint32_t web_btn_labels, web_key_mask;
EXPORT void web_buttons(uint32_t labels)
{
    uint32_t m = 0, b, ch = labels ^ web_btn_labels;
    for (b = 0; b < NB; b++)
        if ((labels >> b) & 1u)
            m |= 1u << panel.btn[b];
    for (b = 0; b < NB; b++)
        if ((ch >> b) & 1u) {
            if ((labels >> b) & 1u)
                web_pressed |= 1u << panel.btn[b];
            else
                web_released |= 1u << panel.btn[b];
        }
    fm1_in.buttons = m;
    web_btn_labels = labels;
}
EXPORT void web_keys(uint32_t mask)
{
    web_notes_pressed |= mask & ~web_key_mask;
    fm1_in.notes = mask;
    web_key_mask = mask;
}
EXPORT void web_enc(uint32_t role, int32_t n)
{
    if (role < NE)
        web_enc_steps[panel.enc[role] % 7u] += n * panel.dir[role];
}
EXPORT void web_master(uint32_t v) { web_master_adc = v > 1023u ? 1023u : v; }
EXPORT uint32_t web_midi(uint32_t status, uint32_t d1, uint32_t d2)
{
    uint32_t cin = (status >> 4) & 15u;
    if (status >= 0xF8u)
        cin = 0xFu;
    if (mi_w - mi_r >= MQ)
        return 0;
    midi_in_q[mi_w % MQ] = cin | status << 8 | (d1 & 0x7Fu) << 16 | (d2 & 0x7Fu) << 24;
    mi_w++;
    return 1;
}
static uint32_t led_bits(const uint8_t *a, int keys)
{
    uint32_t c, r, m = 0;
    for (c = 0; c < FM1_NCOL; c++)
        for (r = 1; r < 5u; r++)
            if ((a[c] >> r) & 1u) {
                int id = FM1_KEYMAP[r][c];
                if (id < 0)
                    continue;
                if (keys && id >= 14)
                    m |= 1u << (id - 14);
                else if (!keys && id < 14) {
                    uint32_t b = panel_btn_of((uint32_t)id);
                    if (b < NB)
                        m |= 1u << b;
                }
            }
    return m;
}
EXPORT uint32_t web_lit_buttons(void) { return led_bits(fm1_led, 0); }
EXPORT uint32_t web_lit_keys(void) { return led_bits(fm1_led, 1); }
EXPORT uint32_t web_dim_buttons(void) { return led_bits(fm1_led_dim, 0); }
EXPORT uint32_t web_dim_keys(void) { return led_bits(fm1_led_dim, 1); }
EXPORT uint32_t web_dim_level(void) { return 0; }
EXPORT uint32_t web_breath_buttons(void) { return 0; }
EXPORT uint32_t web_breath_keys(void) { return 0; }
EXPORT uint16_t *web_screen(void) { return web_fb; }
EXPORT uint32_t web_screen_draws(void) { return web_draws; }
EXPORT uint32_t web_now_ms(void) { return fm1_ms; }
EXPORT uint32_t web_sample_rate(void) { return FS; }
EXPORT uint32_t web_playing(void) { return dm_m.playing; }
/* tests (web/emu_test.mjs): the state */
EXPORT uint32_t web_sel(void) { return u.sel; }
EXPORT uint32_t web_page(void) { return u.page; }
EXPORT const char *web_page_name(void) { return PAGES[u.page].name; }
EXPORT uint32_t web_manual(uint32_t v, uint32_t w) { return dm_kit.v[v % DM_NV].manual[w & 3u]; }
EXPORT int32_t web_vbyte(uint32_t v, uint32_t off) { return off < sizeof(dm_vkit_t) ? ((uint8_t *)&dm_kit.v[v % DM_NV])[off] : -1; }
EXPORT float web_base(uint32_t v, uint32_t t) { return t < DT_N ? dm_kit.v[v % DM_NV].base[t] : -1.0f; }
EXPORT float web_level(uint32_t v) { return dm_m.level[v % DM_NV]; }
EXPORT uint32_t web_trigs(uint32_t v) { return dm_m.trig_count[v % DM_NV]; }
EXPORT int32_t web_lane(uint32_t v, uint32_t l, uint32_t s) { return dm_kit.v[v % DM_NV].lane[l % DM_NLANE][s % DM_MAXSTEPS]; }
EXPORT float web_bpm(void) { return dm_kit.g.bpm; }
EXPORT float web_perf(uint32_t i) { return i < PF_N ? dm_kit.g.perf[i] : 0.0f; }
EXPORT const char *web_kit_name(void) { return dm_kit.name; }
EXPORT const char *web_msg(void) { return u.msg_t && fm1_ms < u.msg_t ? u.msg : ""; }
EXPORT float web_depth(uint32_t v, uint32_t s, uint32_t t) { return dm_kit.v[v % DM_NV].depth[s % DS_N][t % DT_N]; }
EXPORT uint32_t web_locks(uint32_t v) { return dm_kit.v[v % DM_NV].locks; }
EXPORT uint32_t web_off(uint32_t k)          /* field offsets of dm_vkit_t: 0 fill 1 engine 2 mute 3 solo 4 steps */
{
    static const uint16_t O[5] = {offsetof(dm_vkit_t, fill), offsetof(dm_vkit_t, engine), offsetof(dm_vkit_t, mute),
                                  offsetof(dm_vkit_t, solo), offsetof(dm_vkit_t, steps)};
    return k < 5u ? O[k] : 0u;
}
