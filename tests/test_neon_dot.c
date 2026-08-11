/* test_neon_dot.c — verify the A72 NEON Q4_K x Q8_K vec_dot
 * against a scalar reference, and benchmark it. Runs on ARM
 * (the NEON kernel is compiled in); on x86 it benchmarks the
 * scalar path only.
 *
 * The check: random Q4_K/Q8_K blocks -> scalar dot vs NEON dot
 * must agree within fp tolerance. This is the SLERM oracle
 * pattern applied to the kernel. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "neon_dot.h"

/* --- scalar reference (the same math, no SIMD) --- */
#define QK_K 256
typedef struct {
    uint8_t scales[12];
    uint8_t qs[QK_K];
    uint16_t d;
    uint16_t dmin;
} block_q4_K_ref;
typedef struct {
    int8_t qs[QK_K];
    uint16_t bsums[QK_K / 16];
    uint16_t d;
} block_q8_K_ref;

static float f16tof32(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exp = (h >> 10) & 0x1F;
    uint32_t man = h & 0x3FF;
    uint32_t f;
    if (exp == 0) {
        if (man == 0) f = sign;
        else {
            int e = -1;
            do { man <<= 1; e--; } while ((man & 0x400) == 0);
            man &= 0x3FF;
            f = sign | ((uint32_t)(e + 127) << 23) | (man << 13);
        }
    } else if (exp == 31) {
        f = sign | 0x7F800000u | (man << 13);
    } else {
        f = sign | ((exp - 15 + 127) << 23) | (man << 13);
    }
    float out;
    memcpy(&out, &f, 4);
    return out;
}

static float scalar_dot(int n, const void *vx, const void *vy)
{
    const block_q4_K_ref *x = vx;
    const block_q8_K_ref *y = vy;
    int nb = n / QK_K;
    float sumf = 0;
    for (int i = 0; i < nb; i++) {
        uint32_t utmp[4];
        memcpy(&utmp[0], x[i].scales + 0, 4);
        memcpy(&utmp[1], x[i].scales + 4, 4);
        memcpy(&utmp[2], x[i].scales + 8, 4);
        uint8_t *scales = (uint8_t *)&utmp[0];
        uint8_t *mins = (uint8_t *)&utmp[2];
        int sumi = 0;
        for (int j = 0; j < QK_K / 16; j++) sumi += y[i].bsums[j] * mins[j / 2];
        float sum = 0;
        for (int j = 0; j < QK_K / 32; j++) {
            int32_t scale = scales[j];
            for (int l = 0; l < 32; l++) {
                uint8_t nib = x[i].qs[j * 32 + l / 2];
                int8_t a = (l % 2 == 0) ? (int8_t)(nib & 0xF) : (int8_t)(nib >> 4);
                sum += (float)(a * y[i].qs[j * 32 + l]) * (float)scale;
            }
        }
        float d = f16tof32(x[i].d) * f16tof32(y[i].d);
        sumf += d * sum;
        float dmin = f16tof32(x[i].dmin) * f16tof32(y[i].d);
        sumf -= dmin * (float)sumi;
    }
    return sumf;
}

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void)
{
    printf("neon_dot_available: %d\n", neon_dot_available());

    /* build random test data: 64 blocks = 16384 elements */
    enum { NB = 64 };
    uint8_t q4[NB * QK_K];
    int8_t q8[NB * QK_K];
    uint8_t scales[NB * 12];
    uint16_t bsums[NB * 16];
    uint16_t dvals[NB], dmins[NB];
    for (int i = 0; i < NB * QK_K; i++) q4[i] = (uint8_t)(rand() & 0xFF);
    for (int i = 0; i < NB * QK_K; i++) q8[i] = (int8_t)(rand() & 0xFF);
    for (int i = 0; i < NB * 12; i++) scales[i] = (uint8_t)(rand() & 0xFF);
    for (int i = 0; i < NB * 16; i++) bsums[i] = (uint16_t)(rand() & 0xFF);
    for (int i = 0; i < NB; i++) { dvals[i] = 100; dmins[i] = 5; }

    /* scalar reference */
    block_q4_K_ref *rx = malloc(NB * sizeof(block_q4_K_ref));
    block_q8_K_ref *ry = malloc(NB * sizeof(block_q8_K_ref));
    for (int i = 0; i < NB; i++) {
        memcpy(rx[i].scales, scales + i * 12, 12);
        memcpy(rx[i].qs, q4 + i * QK_K, QK_K);
        rx[i].d = dvals[i];
        rx[i].dmin = dmins[i];
        memcpy(ry[i].qs, q8 + i * QK_K, QK_K);
        memcpy(ry[i].bsums, bsums + i * 16, 16);
        ry[i].d = 100;
    }
    double t0 = now_sec();
    float sref = scalar_dot(NB * QK_K, rx, ry);
    double t1 = now_sec();
    printf("scalar dot: %.4f (%.2f ms)\n", sref, (t1 - t0) * 1000);

    /* NEON (if available) — same block layout via the public API */
    float sneon = 0;
    if (neon_dot_available()) {
        /* build packed blocks: scales | qs | d | dmin for q4 */
        uint8_t *px = malloc(NB * (12 + QK_K + 4));
        uint8_t *py = malloc(NB * (QK_K + 32 + 2));
        for (int i = 0; i < NB; i++) {
            memcpy(px + i * (12 + QK_K + 4), scales + i * 12, 12);
            memcpy(px + i * (12 + QK_K + 4) + 12, q4 + i * QK_K, QK_K);
            /* d, dmin as fp16: 100 -> 0x5640, 5 -> 0x4500 */
            px[i * (12 + QK_K + 4) + 12 + QK_K + 0] = 0x40;
            px[i * (12 + QK_K + 4) + 12 + QK_K + 1] = 0x56;
            px[i * (12 + QK_K + 4) + 12 + QK_K + 2] = 0x00;
            px[i * (12 + QK_K + 4) + 12 + QK_K + 3] = 0x45;
            memcpy(py + i * (QK_K + 32 + 2), q8 + i * QK_K, QK_K);
            memcpy(py + i * (QK_K + 32 + 2) + QK_K, bsums + i * 16, 16);
            memcpy(py + i * (QK_K + 32 + 2) + QK_K + 32, &dvals[i], 2);
        }
        t0 = now_sec();
        int rc = neon_vec_dot_q4_K_q8_K(NB * QK_K, &sneon, px, py);
        t1 = now_sec();
        printf("neon dot:    %.4f (%.2f ms) rc=%d\n", sneon, (t1 - t0) * 1000, rc);
        float diff = (sref - sneon);
        if (diff < 0) diff = -diff;
        float rel = diff / (sref < 0 ? -sref : sref);
        printf("relative diff: %.2e %s\n", rel,
               rel < 1e-3 ? "PASS" : "FAIL");
        free(px);
        free(py);
    }
    free(rx);
    free(ry);
    return 0;
}
