/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — boot and the main loop. The boot order, the timer ISR and the guards follow Felucca's main.c
 * (Leo Kuroshita, GPL-3.0): WDT first, the boot-loop guard, the fatal vectors and guards; then the LCD, the input scan
 * (TIMER5, 10 kHz), the audio (ALNK0). */
extern uint32_t _data_start[], _data_end[], _data_load[], _bss_start[], _bss_end[];
extern uint32_t _pool_start[], _pool_end[], _rt_start[], _rt_end[], _rt_load[];

void fm1_timer5_irq(void)
{
    static uint32_t sub, owed, last, acc;
    uint32_t t0 = fm1_ticks(), usb_due = sub % 5u == 0u;
    fm1_timer5_ack();
    fm1_input_tick();
    acc += t0 - last;
    last = t0;
    while (acc >= 1000u * FM1_TICKS_PER_US) {
        acc -= 1000u * FM1_TICKS_PER_US;
        fm1_ms++;
    }
    if (usb_due)
        owed |= 1u;
#if FELUCCA_UART
    if (sub % 5u == 2u)
        owed |= 2u;
#endif
    if (++sub == 10u)
        sub = 0;
    if (in_audio) {                              /* nested in the render: only the scan (and USB audio) */
#if FELUCCA_UAC
        if (usb_due)
            uac_service();
#endif
        t5_nested_ticks += fm1_ticks() - t0;
        return;
    }
    if (owed & 1u)
        usb_poll();
#if FELUCCA_UART
    if (owed & 2u)
        uart_midi_poll();
#endif
    owed = 0;
}
extern void isr_timer5(void);

static void hexs(char *b, uint32_t v)
{
    uint32_t i;
    for (i = 0; i < 8u; i++)
        b[i] = "0123456789ABCDEF"[(v >> (28u - 4u * i)) & 15u];
    b[8] = 0;
}
static void fm1_fault(const fm1_crash_t *c)
{
    char b[12];
    uint32_t t0;
    fm1_audio_stop();
    lcd_fill(0, 0, 240, 240, UI_CRASH_BG);
    draw_text_line(0, 8, 240, &AF_M, "DRUMMATIC CRASH", UI_CRASH_INK, UI_CRASH_BG, 1);
    hexs(b, c->vec);
    draw_text_line(10, 40, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    hexs(b, c->pc);
    draw_text_line(10, 60, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    hexs(b, c->emu);
    draw_text_line(10, 84, 220, &AF_M, b, UI_CRASH_INK, UI_CRASH_BG, 0);
    t0 = fm1_ticks();
    while ((uint32_t)(fm1_ticks() - t0) < 4000u * 1000u * FM1_TICKS_PER_US)
        ;
    fm1_reboot();
}

/* the MASTER pot: 0..1023 -> gain, a dB curve (-60 dB .. 0 dB, the bottom silent) */
static float master_of_pot(uint32_t a)
{
    float x = (float)(a > 1000u ? 1000u : a) * (1.0f / 1000.0f);
    if (x < 0.01f)
        return 0.0f;
    return dm_db2gain(-60.0f * (1.0f - x) * (1.0f - x));
}
static int32_t master_knob = 512 * 16;
static void master_poll(void)
{
    int32_t a = fm1_adc_read(FM1_ADC_MASTER);
    if (a >= 0) {
        master_knob += (a * 16 - master_knob) / 8;
        master_gain = master_of_pot((uint32_t)(master_knob / 16));
    }
}

/* the battery and USB: the LED glow follows MENU > LEDS only */
static void draw_splash(void)
{
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(0, 96, 240, &AF_L, "DRUMMATIC", T_THEME, 1);
    draw_text_box(0, 136, 240, &AF_S, "FM-1  " DM_VERSION, T_MID, 1);
}

/* ota.c's SysEx frames F0 7D 41 4D ..: not used by Drummatic (yet) */
static void remote_frame(const uint8_t *p, uint32_t n) { (void)p; (void)n; }

static void persist_boot(void)
{
    uint32_t f = irq_save();
    flash_ok = FL_FAR(fl_jedec_ram)() == 0x856014u;       /* the expected 1 MiB part, else RAM only */
    irq_restore(f);
}

static void boot_kit(void)
{
    dm_kit_defaults(&dm_kit);
    if (!flash_ok)
        return;
    if (!(dm_set.rsv[0] & 1u) && kit_load_into(0xFFu, &dm_kit) == 0)
        return;                                          /* the autosave */
    if (dm_set.last_kit < DM_NKITS && kit_load_into(dm_set.last_kit, &dm_kit) == 0)
        u.kslot = dm_set.last_kit;
}

static void fm1_main(void)
{
    uint32_t ms;
    persist_boot();
#if FELUCCA_OTA
    if (flash_ok)
        ota_boot_cleanup();                      /* the staged loader and its record left by an update (Felucca main.c):
                                                  * else the next reset may enter the loader again */
#endif
    settings_load();
    palette_set(dm_set.palette % NPALETTES);
    lcd_init();
    draw_splash();
    fm1_input_init();
    fm1_adc_init();
    panel_init();
    ui_init();
    dm_machine_init();
    boot_kit();
    dm_m.clk = dm_set.clk;
    usb_full = dm_set.usb_fixed;
    master_poll();
    master_gain = master_of_pot((uint32_t)(master_knob / 16));
    soft_fade_in();
    audio_init();
    usb_start();
#if FELUCCA_UART
    uart_midi_init();
#endif
    if (!(dm_set.leds))
        fm1_led_anim_start(1);
    fm1_timer5_start(isr_timer5, 4);
    fm1_guard_lock_top();
    fm1_irq_enable_all();
    for (ms = 0; ms < 500u; ms += 10u) {
        fm1_delay_ms(10);
        master_poll();
    }
    lcd_fill(0, 0, 240, 240, T_BG);
    slots_scan();
    for (;;) {
        uint32_t m = fm1_ms;
        fm1_wdt_feed();
        usb_retry(fm1_ms);
        if (fm1_ms > 30000u && bootguard.pending) {
            bootguard.pending = 0;
            bootguard.failed = 0;
        }
        master_poll();
        {   /* OCT- + OCT+ held 5 s: UBOOT (the way back to any firmware) */
            static uint32_t t0;
            uint32_t both = (1u << panel.btn[B_OCTDN]) | (1u << panel.btn[B_OCTUP]);
            if ((fm1_in.buttons & both) != both)
                t0 = fm1_ms;
            else if (fm1_ms - t0 > 5000u) {
                fm1_audio_stop();
                lcd_fill(0, 0, 240, 240, T_BG);
                draw_text_box(0, 110, 240, &AF_M, "UBOOT", T_THEME, 1);
                usb_detach();
                fm1_delay_ms(30);
                bootguard.pending = 0;
                fm1_enter_uboot();
            }
        }
#if FELUCCA_OTA
        ota_service();
        if (usb.ota_req) {
            usb.ota_req = 0;
            if (flash_ok)
                ota_session();
            lcd_fill(0, 0, 240, 240, T_BG);
            u.force = 1;
        }
#endif
        if (usb.uboot_req) {
            fm1_audio_stop();
            lcd_fill(0, 0, 240, 240, T_BG);
            draw_text_box(0, 110, 240, &AF_M, "UBOOT (USB)", T_THEME, 1);
            fm1_delay_ms(20);
            usb_detach();
            fm1_delay_ms(30);
            bootguard.pending = 0;
            fm1_enter_uboot();
        }
        ui_input();
        ui_machine_actions();
        ui_leds();
        ui_draw();
        ui_autosave();
        settings_poll();
        while (fm1_ms - m < 15u)
            ;
    }
}

void fm1_cstart(void)
{
    uint32_t *s, *d, p3, src, wdt;
    fm1_time_init();
    fm1_reset_reason();
    p3 = fm1_boot.p3_rst;
    src = fm1_boot.rst_src;
    wdt = fm1_boot.wdt_con;
    fm1_wdt_arm(0x0D);
    if (bootguard.magic != BOOTGUARD_MAGIC) {
        bootguard.magic = BOOTGUARD_MAGIC;
        bootguard.failed = 0;
        bootguard.pending = 0;
    }
    if (bootguard.pending)
        bootguard.failed++;
    bootguard.pending = 1;
    if (bootguard.failed >= 2u) {                 /* two boots in a row died in their first 30 s: UBOOT */
        bootguard.failed = 0;
        bootguard.pending = 0;
        fm1_enter_uboot();
    }
    fm1_irq_init();
    for (d = _bss_start; d < _bss_end; d++)
        *d = 0;
    for (d = _pool_start; d < _pool_end; d++)
        *d = 0;
    for (s = _data_load, d = _data_start; d < _data_end; s++, d++)
        *d = *s;
    for (s = _rt_load, d = _rt_start; d < _rt_end; s++, d++)
        *d = *s;
    fm1_mailbox_clear();
    fm1_guard_enable(FM1_GUARD_STACK | FM1_GUARD_WRITE | FM1_GUARD_BUS | FM1_GUARD_PC);
    fm1_boot.p3_rst = (uint8_t)p3;
    fm1_boot.rst_src = src;
    fm1_boot.wdt_con = (uint8_t)wdt;
    fm1_main();
    for (;;)
        ;
}
