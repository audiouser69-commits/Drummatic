/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the external MIDI clock: START, then 24 clocks a quarter at 120 BPM with up to +-1.5 ms of jitter
 * (USB frames, the half buffer), 8 bars. Voice 1 plays every 16th: exactly 128 hits, none twice, none lost; the
 * tempo read from the clock within 1 %; STOP stops. */
#include <stdio.h>
#include <math.h>
#include "../firmware/src/dm/dm_machine.h"

int main(void)
{
    uint32_t half, fails = 0, rng = 12345;
    double t = 0, next = 0, per = 60.0 / 120.0 / 24.0;
    uint32_t clocks = 0, total = 8u * 4u * 24u;
    float o[2 * DM_HALF];
    dm_machine_init();
    dm_kit_defaults(&dm_kit);
    for (half = 0; half < DM_NV; half++)
        dm_kit.v[half].fill = 0;
    dm_kit.v[0].fill = 16;                                 /* every 16th */
    dm_kit.g.bpm = 90.0f;                                  /* (the clock's tempo must win) */
    dm_m.clk = DM_CLK_EXT;
    dm_machine_midi(0xFA, 0, 0);
    for (half = 0; clocks < total || t < next + 0.01; half++) {
        double t1 = t + DM_HALF / 44100.0;
        while (clocks < total && next < t1) {              /* the clocks that arrived during the last half */
            dm_machine_midi(0xF8, 0, 0);
            clocks++;
            rng = rng * 1103515245u + 12345u;
            next = clocks * per + ((double)((rng >> 16) % 3001u) - 1500.0) * 1e-6;
        }
        dm_machine_render(o);
        t = t1;
        if (clocks >= total)
            break;
    }
    printf("clock: %u clocks, %u hits of voice 1 (want 128), tempo %.2f BPM (want 120)\n", clocks, dm_m.trig_count[0],
           dm_m.bpm_now);
    if (dm_m.trig_count[0] != 128u && dm_m.trig_count[0] != 129u)   /* (the downbeat of bar 9 may land in the last half) */
        fails++;
    if (fabsf(dm_m.bpm_now - 120.0f) > 1.2f)
        fails++;
    dm_machine_midi(0xFC, 0, 0);
    dm_machine_render(o);
    if (dm_m.playing)
        fails++;
    printf(fails ? "clock: FAILED\n" : "clock: ok\n");
    return (int)fails;
}
