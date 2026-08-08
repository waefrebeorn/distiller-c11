/* test_dither.c — unit tests for the dither module. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "dither/dither.h"

#define W 8
#define H 8

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    dither_ctx *ctx = dither_create(W, H);
    CHECK(ctx != NULL, "create");

    uint8_t src[W * H];
    uint8_t dst[W * H];

    /* all-black -> all 0 */
    memset(src, 0, sizeof(src));
    CHECK(dither_bayer8(ctx, src, dst) == 0, "bayer8 rc");
    for (int i = 0; i < W * H; i++) CHECK(dst[i] == 0, "black->0");

    /* all-white -> all 255 */
    memset(src, 255, sizeof(src));
    CHECK(dither_bayer8(ctx, src, dst) == 0, "bayer8 white rc");
    for (int i = 0; i < W * H; i++) CHECK(dst[i] == 255, "white->255");

    /* gradient: bayer must produce both values */
    for (int i = 0; i < W * H; i++) src[i] = (uint8_t)(i * 32);
    dither_bayer8(ctx, src, dst);
    int has0 = 0, has255 = 0;
    for (int i = 0; i < W * H; i++) {
        if (dst[i] == 0) has0 = 1;
        if (dst[i] == 255) has255 = 1;
    }
    CHECK(has0 && has255, "gradient both levels");

    /* threshold: 100 -> 0, 200 -> 255 */
    memset(src, 100, sizeof(src));
    dither_threshold(ctx, src, dst);
    for (int i = 0; i < W * H; i++) CHECK(dst[i] == 0, "thresh low");
    memset(src, 200, sizeof(src));
    dither_threshold(ctx, src, dst);
    for (int i = 0; i < W * H; i++) CHECK(dst[i] == 255, "thresh high");

    /* FS: flat gray 128 boundary produces a mix (error diffusion) */
    for (int i = 0; i < W * H; i++) src[i] = 128;
    dither_floyd_steinberg(ctx, src, dst);
    has0 = has255 = 0;
    for (int i = 0; i < W * H; i++) {
        if (dst[i] == 0) has0 = 1;
        if (dst[i] == 255) has255 = 1;
    }
    CHECK(has0 && has255, "FS 128 produces dither pattern");

    dither_free(ctx);
    CHECK(failures == 0, "no failures");
    if (failures == 0) {
        printf("test_dither: ALL PASS\n");
        return 0;
    }
    printf("test_dither: %d FAILURES\n", failures);
    return 1;
}
