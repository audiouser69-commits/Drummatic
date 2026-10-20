/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the firmware, one compilation unit (as Felucca's felucca.c). The audio engine (dm/) is plain C;
 * the platform (platform/, ../hal) is Felucca's (Leo Kuroshita, Hügelton Instruments, GPL-3.0): the LCD, the
 * anti-aliased text, USB (MIDI, audio, the update entry), TRS MIDI, the flash store hooks, the update session. */
#ifndef FELUCCA_FLASH
#define FELUCCA_FLASH 1
#endif
#ifndef FELUCCA_OTA
#define FELUCCA_OTA 1
#endif
#define FELUCCA_OTA_DRYRUN 0
#define FELUCCA_CDC 0                    /* no serial console: a plain USB-MIDI + audio device */
#ifndef FELUCCA_UAC
#define FELUCCA_UAC 1                    /* USB audio input: the master output */
#endif
#define FELUCCA_UAC_TONE 0
#ifndef FELUCCA_UART
#define FELUCCA_UART 1                   /* TRS MIDI IN */
#endif
#define FELUCCA_ICONS 0

#define FM1_FPU_QUIET 1
#include <stdint.h>
#include "fm1_time.h"
#include "fm1_sys.h"
#include "fm1_irq.h"
#include "fm1_guard.h"
#include "fm1_input.h"
#include "fm1_timer.h"
#include "fm1_audio.h"
#include "fm1_adc.h"
#include "fm1_lcd_hw.h"
#include "app/dm_core.h"

#include "platform/libc.c"
#include "platform/lcd.c"
#include "platform/gfx.c"
#include "app/dm_panel.c"

/* the audio engine */
#define DM_POOL __attribute__((section(".pool")))
#include "dm/dm_math.c"
#include "dm/dm_engines.c"
#include "dm/dm_voice.c"
#include "dm/dm_fx.c"
#include "dm/dm_kit.c"
#include "dm/dm_machine.c"

#include "fm1_flash.h"
#include "platform/usb.c"
#if FELUCCA_UART
#include "platform/midi_uart.c"
#endif
#include "app/dm_audio.c"
static uint8_t flash_ok;
#include "platform/storage_hw.c"
#include "app/dm_store.c"
#if FELUCCA_OTA
#include "platform/ota.c"
#include "platform/ota_hw.c"
#endif
#include "app/dm_params.c"
#include "app/dm_ui.c"
#include "app/dm_main.c"
