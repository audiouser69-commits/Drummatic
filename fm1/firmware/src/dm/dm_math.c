/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the sine table (RAM: the audio ISR reads it every sample; XIP flash would miss the cache). */
#include "dm_math.h"

float dm_sin_tab[DM_SIN_N + 1u];
float dm_tanh_tab[DM_TANH_N + 2u];

void dm_math_init(void)
{
    /* rotation by 2 pi / N in double (once at boot); cos and sin of the step from their Taylor series */
    double x = 6.283185307179586 / (double)DM_SIN_N, x2 = x * x;
    double c = 1.0 - x2 / 2.0 + x2 * x2 / 24.0 - x2 * x2 * x2 / 720.0 + x2 * x2 * x2 * x2 / 40320.0;
    double s = x * (1.0 - x2 / 6.0 + x2 * x2 / 120.0 - x2 * x2 * x2 / 5040.0 + x2 * x2 * x2 * x2 / 362880.0);
    double a = 0.0, b = 1.0;                       /* sin, cos */
    uint32_t i;
    for (i = 0; i <= DM_SIN_N; i++) {
        double na, nb;
        dm_sin_tab[i] = (float)a;
        if ((i & 127u) == 127u) {                  /* renormalise now and then */
            double r = a * a + b * b;
            r = 1.5 - 0.5 * r;
            a *= r;
            b *= r;
        }
        na = a * c + b * s;
        nb = b * c - a * s;
        a = na;
        b = nb;
    }
    for (i = 0; i <= DM_TANH_N + 1u; i++)
        dm_tanh_tab[i] = dm_tanh_exact((float)i * (9.0f / (float)DM_TANH_N) - 4.5f);
    dm_sin_tab[0] = 0.0f;
    dm_sin_tab[DM_SIN_N / 2u] = 0.0f;
    dm_sin_tab[DM_SIN_N] = 0.0f;
    dm_sin_tab[DM_SIN_N / 4u] = 1.0f;
    dm_sin_tab[3u * DM_SIN_N / 4u] = -1.0f;
}
