/* Drummatic Control — firmware del circuito di controllo (Raspberry Pi Pico 2).
 * Si presenta al Raspberry Pi come dispositivo USB-MIDI "Drummatic Control" sul canale 16:
 *   tasti        -> Note On/Off  0..45 (velocity 127)       pulsanti encoder -> note 64..72
 *   encoder 1..8 -> CC 16..23, DATA -> CC 24, valori relativi (1..63 = destra, 65..127 = sinistra)
 *   volume       -> CC 7 (0..127)
 * In ingresso (canale 16): Note On n, velocity v -> colore del LED del tasto n dalla tavolozza
 *   (0 spento; 1..15 colori pieni; +16 = metà luminosità); CC 119 = luminosità generale 0..127;
 *   CC 121 = tutti i LED spenti. Finché il Pi non manda nulla i tasti si illuminano quando premuti.
 * Aggiornamento senza aprire la macchina (SysEx):
 *   F0 7D 44 52 55 50 F7 ("DRUP") -> il Pico si riavvia nel caricatore USB (disco RP2350, come col pulsante BOOTSEL)
 *   F0 7D 44 52 56 F7    ("DRV")  -> risponde F0 7D 44 52 56 <versione> F7
 * Collegamenti: righe GP0-GP6, colonne GP7-GP14, LED GP15, 74HC165 LOAD GP16 / CLK GP17 / DATA GP18, volume GP26.
 */
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/pio.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "tusb.h"
#include "ws2812.pio.h"
#include "keymap.h"

#define MIDI_CH      15          /* canale 16 */
#define FW_VERSION   2           /* 1 = 26/10/2026; 2 = 27/10/2026, aggiornabile via SysEx */
#define PIN_ROW0     0
#define PIN_COL0     7
#define PIN_LED      15
#define PIN_SR_LOAD  16
#define PIN_SR_CLK   17
#define PIN_SR_DATA  18
#define PIN_POT      26
#define MAX_LEVEL    51          /* 20 % del massimo: 46 LED bianchi ~0,35 A, così LED + display restano sotto 1 A dall'USB */

static uint8_t led_color[NUM_LEDS];   /* per LED: indice tavolozza */
static uint8_t key_color[NUM_KEYS];   /* per tasto, come ricevuto dal Pi */
static bool host_drives_leds = false;
static uint8_t brightness = 127;
static bool key_state[80];
static uint8_t key_debounce[80];

static const uint8_t PALETTE[16][3] = {
    {0, 0, 0}, {255, 255, 255}, {255, 0, 0}, {255, 90, 0}, {255, 170, 0}, {255, 255, 0}, {120, 255, 0},
    {0, 255, 0}, {0, 255, 120}, {0, 255, 255}, {0, 120, 255}, {0, 0, 255}, {120, 0, 255}, {255, 0, 255},
    {255, 0, 120}, {255, 140, 60}};

static inline void midi_send3(uint8_t a, uint8_t b, uint8_t c) {
    uint8_t m[3] = {a, b, c};
    if (tud_midi_mounted()) tud_midi_stream_write(0, m, 3);
}

/* ---------------------------------------------------------------- LED */
static void leds_show(PIO pio, uint sm) {
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t v = led_color[i];
        const uint8_t *c = PALETTE[v & 15];
        uint32_t scale = (uint32_t)MAX_LEVEL * brightness / 127;
        if (v & 16) scale /= 2;
        uint8_t r = c[0] * scale / 255, g = c[1] * scale / 255, b = c[2] * scale / 255;
        pio_sm_put_blocking(pio, sm, ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8));
    }
}

static void leds_from_keys(void) {
    for (int i = 0; i < NUM_LEDS; i++) {
        int k = LED_TO_KEY[i];
        if (host_drives_leds) led_color[i] = key_color[k];
        else led_color[i] = key_state[k] ? 3 : 0;        /* arancio quando premuto */
    }
}

static void boot_animation(PIO pio, uint sm) {
    for (int step = 0; step < NUM_LEDS + 8; step++) {
        for (int i = 0; i < NUM_LEDS; i++) led_color[i] = (i <= step && i > step - 8) ? 3 : 0;
        leds_show(pio, sm); sleep_ms(12);
    }
    memset(led_color, 0, sizeof led_color); leds_show(pio, sm);
}

/* ---------------------------------------------------------------- matrice tasti */
static void matrix_init(void) {
    for (int r = 0; r < 7; r++) { gpio_init(PIN_ROW0 + r); gpio_set_dir(PIN_ROW0 + r, GPIO_IN); }
    for (int c = 0; c < 8; c++) { gpio_init(PIN_COL0 + c); gpio_set_dir(PIN_COL0 + c, GPIO_IN); gpio_pull_up(PIN_COL0 + c); }
}

static void matrix_scan(void) {
    for (int r = 0; r < 7; r++) {
        /* riga attiva = uscita bassa, le altre in alta impedenza; i diodi vanno da colonna a riga */
        gpio_set_dir(PIN_ROW0 + r, GPIO_OUT); gpio_put(PIN_ROW0 + r, 0);
        busy_wait_us_32(10);
        uint32_t cols = gpio_get_all();
        gpio_set_dir(PIN_ROW0 + r, GPIO_IN);
        for (int c = 0; c < 8; c++) {
            int k = MATRIX[r][c];
            if (k < 0) continue;
            bool pressed = !(cols & (1u << (PIN_COL0 + c)));
            if (pressed != key_state[k]) {
                if (++key_debounce[k] >= 5) {       /* 5 scansioni consecutive (~5 ms) */
                    key_state[k] = pressed; key_debounce[k] = 0;
                    midi_send3((pressed ? 0x90 : 0x80) | MIDI_CH, (uint8_t)k, pressed ? 127 : 0);
                }
            } else key_debounce[k] = 0;
        }
    }
}

/* ---------------------------------------------------------------- encoder via 74HC165 */
static uint8_t enc_prev[NUM_ENC];
static int8_t enc_acc[NUM_ENC];
static int16_t enc_out[NUM_ENC];
static const int8_t QDEC[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};

static void sr_init(void) {
    gpio_init(PIN_SR_LOAD); gpio_set_dir(PIN_SR_LOAD, GPIO_OUT); gpio_put(PIN_SR_LOAD, 1);
    gpio_init(PIN_SR_CLK); gpio_set_dir(PIN_SR_CLK, GPIO_OUT); gpio_put(PIN_SR_CLK, 0);
    gpio_init(PIN_SR_DATA); gpio_set_dir(PIN_SR_DATA, GPIO_IN);
}

static uint32_t sr_read(void) {
    gpio_put(PIN_SR_LOAD, 0); busy_wait_us_32(1); gpio_put(PIN_SR_LOAD, 1); busy_wait_us_32(1);
    uint32_t v = 0;
    for (int i = 0; i < 24; i++) {
        v = (v << 1) | (gpio_get(PIN_SR_DATA) ? 1 : 0);
        gpio_put(PIN_SR_CLK, 1); busy_wait_us_32(1); gpio_put(PIN_SR_CLK, 0); busy_wait_us_32(1);
    }
    return v;   /* bit 23 = primo letto (posizione 0 della tabella) */
}

static void encoders_scan(void) {
    uint32_t v = sr_read();
    for (int e = 0; e < NUM_ENC; e++) {
        uint8_t a = (v >> (23 - ENC_BIT_A[e])) & 1, b = (v >> (23 - ENC_BIT_B[e])) & 1;
        uint8_t s = (uint8_t)((a << 1) | b);
        enc_acc[e] += QDEC[(enc_prev[e] << 2) | s];
        enc_prev[e] = s;
        /* EC11E 15 impulsi / 30 scatti: uno scatto = 2 transizioni */
        while (enc_acc[e] >= 2) { enc_acc[e] -= 2; enc_out[e]++; }
        while (enc_acc[e] <= -2) { enc_acc[e] += 2; enc_out[e]--; }
    }
}

static void encoders_flush(void) {
    for (int e = 0; e < NUM_ENC; e++) {
        int d = enc_out[e];
        if (!d) continue;
        if (d > 63) d = 63; if (d < -63) d = -63;
        midi_send3(0xB0 | MIDI_CH, (uint8_t)(16 + e), (uint8_t)(d > 0 ? d : 128 + d));
        enc_out[e] = 0;
    }
}

/* ---------------------------------------------------------------- volume */
static int pot_filtered = -1, pot_sent = -1;
static void pot_scan(void) {
    int raw = adc_read();                               /* 0..4095 */
    pot_filtered = pot_filtered < 0 ? raw : (pot_filtered * 7 + raw) / 8;
    int v = pot_filtered * 128 / 4096;
    if (pot_sent < 0 || (v != pot_sent && abs(pot_filtered - pot_sent * 32 - 16) > 24)) {
        pot_sent = v; midi_send3(0xB0 | MIDI_CH, 7, (uint8_t)v);
    }
}

/* ---------------------------------------------------------------- MIDI in */
static uint8_t sx[16];
static int sx_len = -1;                /* -1 = fuori da un SysEx */

static void sysex_done(void) {
    static const uint8_t UPD[] = {0x7D, 0x44, 0x52, 0x55, 0x50}, VER[] = {0x7D, 0x44, 0x52, 0x56};
    if (sx_len == (int)sizeof UPD && !memcmp(sx, UPD, sizeof UPD)) {
        /* caricatore USB: compare il disco RP2350 e il Pi ci copia il nuovo .uf2 */
        uint32_t t0 = time_us_32();
        while (time_us_32() - t0 < 50000) tud_task();      /* lascia uscire i pacchetti USB in coda */
        reset_usb_boot(0, 0);
    } else if (sx_len == (int)sizeof VER && !memcmp(sx, VER, sizeof VER) && tud_midi_mounted()) {
        uint8_t r[] = {0xF0, 0x7D, 0x44, 0x52, 0x56, FW_VERSION, 0xF7};
        tud_midi_stream_write(0, r, sizeof r);
    }
}

static void sysex_bytes(const uint8_t *b, int n) {
    for (int i = 0; i < n; i++) {
        if (b[i] == 0xF0) { sx_len = 0; continue; }
        if (sx_len < 0) continue;
        if (b[i] == 0xF7) { sysex_done(); sx_len = -1; continue; }
        if (b[i] & 0x80 || sx_len >= (int)sizeof sx) { sx_len = -1; continue; }
        sx[sx_len++] = b[i];
    }
}

static bool midi_in(void) {
    bool changed = false;
    uint8_t pkt[4];
    while (tud_midi_available() && tud_midi_packet_read(pkt)) {
        uint8_t cin = pkt[0] & 0x0F, st = pkt[1] & 0xF0, ch = pkt[1] & 0x0F, d1 = pkt[2], d2 = pkt[3];
        /* USB-MIDI: CIN 4 = SysEx 3 byte che continua; 5/6/7 = SysEx che finisce con 1/2/3 byte */
        if (cin == 0x4 || cin == 0x7) { sysex_bytes(pkt + 1, 3); continue; }
        if (cin == 0x6) { sysex_bytes(pkt + 1, 2); continue; }
        if (cin == 0x5) { sysex_bytes(pkt + 1, 1); continue; }
        if ((cin != 0x8 && cin != 0x9 && cin != 0xB) || ch != MIDI_CH) continue;
        if ((st == 0x90 || st == 0x80) && d1 < NUM_KEYS) {
            key_color[d1] = st == 0x90 ? (d2 & 31) : 0; host_drives_leds = true; changed = true;
        } else if (st == 0xB0 && d1 == 119) { brightness = d2; changed = true; }
        else if (st == 0xB0 && d1 == 121) { memset(key_color, 0, sizeof key_color); host_drives_leds = true; changed = true; }
    }
    return changed;
}

int main(void) {
    tusb_init();
    matrix_init(); sr_init();
    adc_init(); adc_gpio_init(PIN_POT); adc_select_input(0);
    PIO pio = pio0; uint sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, PIN_LED, 800000);
    boot_animation(pio, sm);
    for (int e = 0; e < NUM_ENC; e++) {
        uint32_t v = sr_read();
        enc_prev[e] = (uint8_t)((((v >> (23 - ENC_BIT_A[e])) & 1) << 1) | ((v >> (23 - ENC_BIT_B[e])) & 1));
    }
    absolute_time_t next_scan = get_absolute_time(), next_led = next_scan, next_pot = next_scan;
    bool dirty = true;
    while (true) {
        tud_task();
        absolute_time_t now = get_absolute_time();
        if (absolute_time_diff_us(next_scan, now) >= 0) {          /* ogni 1 ms */
            next_scan = delayed_by_us(next_scan, 1000);
            bool before[NUM_KEYS]; memcpy(before, key_state, sizeof before);
            matrix_scan(); encoders_scan(); encoders_flush();
            if (memcmp(before, key_state, sizeof before)) dirty = true;
        }
        if (absolute_time_diff_us(next_pot, now) >= 0) { next_pot = delayed_by_us(next_pot, 5000); pot_scan(); }
        if (midi_in()) dirty = true;
        if (dirty && absolute_time_diff_us(next_led, now) >= 0) {   /* al massimo 100 aggiornamenti al secondo */
            next_led = delayed_by_us(now, 10000);
            leds_from_keys(); leds_show(pio, sm); dirty = false;
        }
    }
}
