/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — what the platform files (Felucca's usb.c, storage, ota, lcd, gfx) expect from the application,
 * and the application's shared state. */
#ifndef DM_CORE_H
#define DM_CORE_H

#define FS 44100u
#define HALF_FRAMES 128u                 /* I2S half buffer (2.9 ms): one dm_machine_render */
#define RING_PUBLISH() __asm__ volatile("" ::: "memory")

static volatile uint32_t fm1_ms;          /* milliseconds since boot (TIMER4, the TIMER5 ISR) */
struct { uint32_t magic, failed, pending; } bootguard __attribute__((section(".noinit")));
#define BOOTGUARD_MAGIC 0x42475244u

/* the build's identity (tools/build.py) */
#ifndef FELUCCA_ID
#define FELUCCA_ID "FM-1_900"
#endif
#ifndef DM_VERSION
#define DM_VERSION "v0.1"
#endif
#endif
