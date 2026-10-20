/* SPDX-License-Identifier: GPL-3.0-only
 * Drummatic FM-1 — the reference kit ("Dael" intro) measured: each voice alone (RMS over bars 2..4, dBFS) and the
 * whole mix (RMS, peak, crest, the band balance), against the owner's numbers of the reference on the FM-1.
 *   dm_ref [OUT.wav]  (the mix, 4 bars) */
#include <stdio.h>
#include <math.h>
#include "../firmware/src/dm/dm_machine.h"

#define NB 4
static float buf[44100 * 12 * 2];
static uint32_t render(int solo, uint32_t bars)
{
    uint32_t n = 0, i, v, total = (uint32_t)(44100.0f * 60.0f / 90.0f * 4.0f * bars);
    dm_machine_init();
    dm_kit_reference(&dm_kit);
    for (v = 0; v < DM_NV; v++)
        dm_kit.v[v].mute = solo >= 0 && (int)v != solo && !(solo == 0 && v == 6);   /* (hit + hats together) */
    dm_m.want_play = 1;
    while (n < total) {
        static float o[2 * DM_HALF];
        dm_machine_render(o);
        for (i = 0; i < DM_HALF && n < total; i++, n++) {
            buf[2 * n] = o[2 * i];
            buf[2 * n + 1] = o[2 * i + 1];
        }
    }
    return n;
}
static double rms_db(uint32_t a, uint32_t b)
{
    double s = 0;
    uint32_t i;
    for (i = a; i < b; i++)
        s += 0.5 * ((double)buf[2 * i] * buf[2 * i] + (double)buf[2 * i + 1] * buf[2 * i + 1]);
    return 10.0 * log10(s / (double)(b - a) + 1e-30);
}
/* one-pole band split: energy in bands (dB of the total) */
static void bands(uint32_t a, uint32_t b)
{
    static const double E[9] = {20, 60, 150, 400, 1000, 2500, 6000, 12000, 22050};
    double tot = 0, e[8] = {0};
    uint32_t i, k;
    double lp[9] = {0}, c[9];
    for (k = 0; k < 9; k++)
        c[k] = exp(-2 * M_PI * E[k] / 44100.0);
    for (i = a; i < b; i++) {
        double x = 0.5 * (buf[2 * i] + buf[2 * i + 1]), y[9];
        for (k = 0; k < 9; k++) {
            lp[k] = x + c[k] * (lp[k] - x);
            y[k] = lp[k];
        }
        for (k = 0; k < 8; k++) {
            double d = y[k + 1] - y[k];
            e[k] += d * d;
        }
        tot += x * x;
    }
    printf("bands (dB of total) 20-60 %.0f, 60-150 %.0f, 150-400 %.0f, 400-1k %.0f, 1-2.5k %.0f, 2.5-6k %.0f, 6-12k %.0f, 12k+ %.0f\n",
           10 * log10(e[0] / tot), 10 * log10(e[1] / tot), 10 * log10(e[2] / tot), 10 * log10(e[3] / tot),
           10 * log10(e[4] / tot), 10 * log10(e[5] / tot), 10 * log10(e[6] / tot), 10 * log10(e[7] / tot));
}
int main(int argc, char **argv)
{
    static const char *const NAME[DM_NV] = {"hit+hats", "noise", "zap", "stack", "buzz swell", "buzz fall", "", "kick"};
    static const double REF[DM_NV] = {-31.9, -33.3, -39.2, -36.9, -22.9, -25.8, 0, 0};
    uint32_t bar = (uint32_t)(44100.0f * 60.0f / 90.0f * 4.0f), n, i, v;
    double pk = 0;
    for (v = 0; v < 6; v++) {
        n = render((int)v, NB);
        printf("%-10s %6.1f dBFS  (reference %6.1f, %+.1f)\n", NAME[v], rms_db(bar, n), REF[v], rms_db(bar, n) - REF[v]);
    }
    n = render(-1, NB);
    for (i = bar; i < n; i++)
        pk = fmax(pk, fmax(fabs(buf[2 * i]), fabs(buf[2 * i + 1])));
    printf("mix: RMS %.1f dBFS (reference -20.7), peak %.1f dBFS (-2.9), crest %.1f dB (18)\n", rms_db(bar, n),
           20 * log10(pk), 20 * log10(pk) - rms_db(bar, n));
    bands(bar, n);
    printf("reference band balance (FM-1): -2 -9 -11 -13 -13 -18 -20 -18\n");
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb");
        uint32_t bytes = n * 4, x;
        uint16_t s;
        fwrite("RIFF", 1, 4, f); x = 36 + bytes; fwrite(&x, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
        x = 16; fwrite(&x, 4, 1, f); s = 1; fwrite(&s, 2, 1, f); s = 2; fwrite(&s, 2, 1, f);
        x = 44100; fwrite(&x, 4, 1, f); x = 44100 * 4; fwrite(&x, 4, 1, f); s = 4; fwrite(&s, 2, 1, f); s = 16; fwrite(&s, 2, 1, f);
        fwrite("data", 1, 4, f); fwrite(&bytes, 4, 1, f);
        for (i = 0; i < 2 * n; i++) {
            float y = buf[i] > 1 ? 1 : buf[i] < -1 ? -1 : buf[i];
            int16_t q = (int16_t)lrintf(y * 32767);
            fwrite(&q, 2, 1, f);
        }
        fclose(f);
    }
    return 0;
}
