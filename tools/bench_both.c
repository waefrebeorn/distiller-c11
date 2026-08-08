/* bench_both.c — compare bayer8 / FS / full pipeline in C11. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "eink/eink.h"
#include "dither/dither.h"

static double now_sec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
}

int main(void)
{
    const size_t w = 240, h = 416;
    const int N = 100;
    uint8_t *px = malloc(w * h);
    uint8_t *raw = malloc(w * h);      /* dither output: raw frame */
    uint8_t *out = malloc(12480);      /* packed output */
    for (size_t i = 0; i < w * h; i++) px[i] = (uint8_t)(i & 255);

    dither_ctx *d = dither_create(w, h);
    double t0 = now_sec();
    for (int i = 0; i < N; i++) dither_bayer8(d, px, raw);
    double t1 = now_sec();
    printf("C11 bayer8 only:   %.3f ms/frame\n", (t1 - t0) / N * 1000);

    t0 = now_sec();
    for (int i = 0; i < N; i++) dither_floyd_steinberg(d, px, raw);
    t1 = now_sec();
    printf("C11 FS only:       %.3f ms/frame\n", (t1 - t0) / N * 1000);

    eink_frame *e = eink_create(w, h);
    t0 = now_sec();
    for (int i = 0; i < N; i++) eink_render_gray(e, px, out);
    t1 = now_sec();
    printf("C11 full pipeline: %.3f ms/frame\n", (t1 - t0) / N * 1000);

    free(out);
    free(raw);
    free(px);
    return 0;
}
