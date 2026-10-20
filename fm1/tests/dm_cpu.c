/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the cost of a half buffer in a few scenarios, for valgrind (instructions per sample):
 *   valgrind --tool=callgrind --callgrind-out-file=/dev/null ./dm_cpu SCENARIO HALVES
 * The count of HALVES=2 is subtracted from that of HALVES=N+2 (setup), divided by N * 128. */
#include <stdio.h>
#include <stdlib.h>
#include "../firmware/src/dm/dm_machine.h"

static void every_step(uint32_t v) { uint32_t i; for (i = 0; i < 4u; i++) dm_kit.v[v].manual[i] = 0xFFFFFFFFu; }

int main(int argc, char **argv)
{
    int sc = argc > 1 ? atoi(argv[1]) : 0, n = argc > 2 ? atoi(argv[2]) : 100, i;
    uint32_t v;
    static float out[2 * DM_HALF];
    dm_machine_init();
    dm_kit_defaults(&dm_kit);
    switch (sc) {
    case 0: break;                                           /* the power-on kit */
    case 1:                                                  /* 8 voices, every 1/16, each its heaviest engine */
    case 2:                                                  /* the same, STEREO, comb filters, MEGA GLITCH, sends */
    case 3: {                                                /* 8 x ADDITIVE 24 partials stereo, comb, glitch */
        static const uint8_t E[8] = {ENG_ADDITIVE, ENG_SCRAPYARD, ENG_MODAL, ENG_RESON, ENG_HAT, ENG_CHAOS, ENG_SUMSYN, ENG_HAYWIRE};
        for (v = 0; v < DM_NV; v++) {
            dm_vkit_t *k = &dm_kit.v[v];
            k->engine = sc == 3 ? ENG_ADDITIVE : E[v];
            k->base[T_P3] = 1.0f;                            /* (ADDITIVE: every partial) */
            k->base[T_P1] = 0.1f;
            k->base[T_E1D] = dm_norm(T_E1D, 2.0f);
            k->base[T_CUTOFF] = 0.6f;
            k->base[T_DIST] = 0.4f;
            every_step(v);
            if (sc >= 2) {
                k->stereo = 1;
                k->fmode = v & 1 ? FM_COMB_FBP : FM_SCREAM_BP;
                k->dmode = v & 1 ? DM_MEGAGLITCH : DM_TUBE;
                k->base[T_DSEND] = 0.5f;
                k->base[T_RSEND] = 0.5f;
                k->depth[S_LFO1][T_CUTOFF] = 0.2f;
                k->depth[S_ENV1][T_P2] = 0.3f;
            }
        }
        dm_kit.g.m_max = 0.5f;
        dm_kit.g.m_low = 2.0f;
        dm_kit.g.m_high = 2.0f;
        break;
    }
    case 4: break;                                           /* stopped, silent: the idle cost */
    }
    dm_m.want_play = sc != 4;
    for (i = 0; i < n; i++)
        dm_machine_render(out);
    printf("%d %d %f\n", sc, n, out[0]);
    return 0;
}
