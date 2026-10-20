/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the audio ISR: MIDI in from the rings, a half buffer from the machine (dm_machine_render), the
 * USB audio input (the full level), the MASTER knob, the soft gain (power-on swell, the dip around a flash erase),
 * the I2S buffer. After Felucca's audio.c (Leo Kuroshita, GPL-3.0): the same half-buffer contract with the DMA, the
 * same overload rule (two halves in a row over 85 %: the quietest voice is cut). */
#define OUT_SHIFT 7                       /* Q15 -> the codec's 24 bits, -6 dBFS ceiling as Felucca */
#define HALF_WORDS (HALF_FRAMES * 2u)
#define DAC_TICKS 544u                    /* TIMER4 ticks per frame (24 MHz / 44.1 kHz) */

static int32_t abuf[2u * HALF_WORDS] __attribute__((aligned(4)));
static float aout[2u * HALF_FRAMES];
static int32_t aq[2u * HALF_FRAMES];
static volatile uint8_t audio_live;
static volatile uint32_t audio_halves, audio_load_q8, audio_max_us, audio_late, audio_shed_n;
static volatile uint32_t t5_nested_ticks;
static volatile float master_gain = 0.5f;  /* the MASTER knob (main loop) */
static volatile uint8_t in_audio;
static volatile uint8_t usb_full = 1;              /* MENU > USB OUT: 1 FIXED (full level), 0 MASTER */

/* MANNER:fade-in MANNER:erase-dip — Felucca's SOFT gain (Q24), on the DAC only */
#define SOFT_ONE (1u << 24)
static volatile uint32_t soft_g = SOFT_ONE, soft_tgt = SOFT_ONE, soft_step = SOFT_ONE;
static void soft_set(uint32_t tgt, uint32_t ms)
{
    uint32_t n = FS * ms / 1000u;
    soft_step = n ? SOFT_ONE / n + 1u : SOFT_ONE;
    soft_tgt = tgt;
}
static void soft_fade_in(void) { soft_g = 0; soft_set(SOFT_ONE, 800u); }
static void soft_down_wait(void)
{
    uint32_t t = fm1_ms, h;
    soft_set(0, 10u);
    while (soft_g != 0 && fm1_ms - t < 40u)
        ;
    h = audio_halves;
    while (audio_halves - h < 3u && fm1_ms - t < 60u)
        ;
}
static void soft_up(void) { soft_set(SOFT_ONE, 25u); }

/* the MIDI the rings hold (USB and TRS): to the machine, at the start of the half */
#ifndef MIDI_SRC
#define MIDI_SRC(at) midi_in_source[at]          /* usb.c: 1 USB, 2 TRS */
#endif
static void audio_midi(void)
{
    if (midi_in_overflow) {                        /* a broken stream: drop it */
        mi_r = mi_w;
        midi_in_overflow = 0;
        return;
    }
    while (mi_r != mi_w) {
        uint32_t at = mi_r % MQ, pkt = midi_in_q[at], st = (pkt >> 8) & 0xFFu;
        mi_r++;
        if (st >= 0xF8u) {                           /* clock, START, STOP: from one source (USB or TRS) at a time */
            static uint8_t src;
            static uint32_t last;
            uint8_t from = MIDI_SRC(at);
            if (!src || fm1_ms - last > 500u)
                src = from;
            if (from != src)
                continue;
            last = fm1_ms;
            dm_machine_midi(st, 0, 0);
        }
        else if (st >= 0x80u && st < 0xF0u)
            dm_machine_midi(st, (pkt >> 16) & 0x7Fu, (pkt >> 24) & 0x7Fu);
    }
}

/* overload: the quietest sounding voice is cut (choked: a 3 ms fade) */
static void audio_shed(void)
{
    uint32_t v, best = DM_NV;
    float lv = 1.0e9f;
    for (v = 0; v < DM_NV; v++)
        if (dm_voice_active(&voice[v]) && !voice[v].choking && voice[v].level < lv) {
            lv = voice[v].level;
            best = v;
        }
    if (best < DM_NV) {
        dm_voice_choke(&voice[best]);
        audio_shed_n++;
    }
}

static void audio_half(int32_t *o)
{
    uint32_t i, g0 = soft_g, tgt = soft_tgt, st = soft_step * HALF_FRAMES, g1;
    float mg = master_gain, q0, dq;
    audio_midi();
    dm_machine_render(aout);
    for (i = 0; i < 2u * HALF_FRAMES; i++) {       /* +-1.0 -> Q15, clipped */
        float x = aout[i] * 32767.0f;
        x = x > 32767.0f ? 32767.0f : x > -32767.0f ? x : (x == x ? -32767.0f : 0.0f);   /* (NaN: silence) */
        aq[i] = (int32_t)x;
    }
#if FELUCCA_UAC
    if (!usb_full)                                  /* MENU > USB OUT MASTER: the knob on USB too */
        for (i = 0; i < 2u * HALF_FRAMES; i++)
            aq[i] = (int32_t)((float)aq[i] * mg);
    for (i = 0; i < HALF_FRAMES; i += 32u)          /* the USB input (FIXED: the full level, before the knob) */
        uac_tap(aq + 2u * i, 32u);
    if (!usb_full)
        mg = 1.0f;
#endif
    /* the knob and the soft gain (squared ramp over the half) */
    g1 = g0 < tgt ? (tgt - g0 < st ? tgt : g0 + st) : (g0 - tgt < st ? tgt : g0 - st);
    {
        float a = (float)(g0 >> 12) * (1.0f / 4096.0f), b = (float)(g1 >> 12) * (1.0f / 4096.0f);
        q0 = a * a * mg;
        dq = (b * b * mg - q0) * (1.0f / (float)HALF_FRAMES);
    }
    soft_g = g1;
    for (i = 0; i < HALF_FRAMES; i++) {
        float k = q0 + dq * (float)i;
        o[2u * i] = (int32_t)((float)aq[2u * i] * k) * (1 << OUT_SHIFT);
        o[2u * i + 1u] = (int32_t)((float)aq[2u * i + 1u] * k) * (1 << OUT_SHIFT);
    }
}

#define SHED_TICKS ((HALF_FRAMES * 1000000u / FS) * 85u / 100u * FM1_TICKS_PER_US)
static uint8_t shed_over;

void fm1_alnk0_irq(void)
{
    uint8_t p;
    uint32_t t0;
    in_audio = 1;
    p = fm1_audio_pending();
    t0 = fm1_ticks();
    t5_nested_ticks = 0;
    fm1_audio_ack_aux(p);
    if (p & FM1_AUDIO_HALF) {
        uint32_t half = fm1_audio_free_half(), all, us;
        if ((shed_over & 3u) == 3u) {
            shed_over = 0;
            audio_shed();
        }
#if FELUCCA_UAC
        uac_render_start();
#endif
        audio_half(&abuf[half * HALF_WORDS]);
        fm1_audio_ack_half();
        audio_halves++;
        all = fm1_ticks() - t0;
        shed_over = (uint8_t)(shed_over << 1 | (all > SHED_TICKS));
        us = (all - t5_nested_ticks) / FM1_TICKS_PER_US;
        if (us > audio_max_us)
            audio_max_us = us;
        audio_load_q8 = (audio_load_q8 * 31u + us * 256u / (HALF_FRAMES * 1000000u / FS)) / 32u;
        dm_m.cpu_q8 = audio_load_q8;
        if (fm1_audio_free_half() != half)
            audio_late++;
    }
    in_audio = 0;
}
extern void isr_alnk0(void);

static void audio_init(void)
{
    uint32_t i;
    for (i = 0; i < 2u * HALF_WORDS; i++)
        abuf[i] = 0;
    fm1_audio_init(abuf, HALF_WORDS, isr_alnk0, 3);
    audio_live = 1;
}
