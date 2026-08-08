/* dither.c — e-ink dithering kernels (C11).
 *
 * Implements the opaque dither_ctx API from dither.h.
 * Pure C11, no third-party dependencies. Both algorithms operate
 * on row-major uint8 grayscale buffers.
 */

#include "dither.h"

#include <stdlib.h>
#include <string.h>

struct dither_ctx {
    size_t width;
    size_t height;
    /* scratch for FS error diffusion (float, one row + next row) */
    float *err_cur;
    float *err_next;
};

/* 8x8 Bayer matrix (values 0..63). */
static const uint8_t BAYER8[64] = {
      0, 32,  8, 40,  2, 34, 10, 42,
     48, 16, 56, 24, 50, 18, 58, 26,
     12, 44,  4, 36, 14, 46,  6, 38,
     60, 28, 52, 20, 62, 30, 54, 22,
      3, 35, 11, 43,  1, 33,  9, 41,
     51, 19, 59, 27, 49, 17, 57, 25,
     15, 47,  7, 39, 13, 45,  5, 37,
     63, 31, 55, 23, 61, 29, 53, 21,
};

dither_ctx *dither_create(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        return NULL;
    }
    dither_ctx *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->width = width;
    ctx->height = height;
    ctx->err_cur = calloc(width, sizeof(float));
    ctx->err_next = calloc(width, sizeof(float));
    if (ctx->err_cur == NULL || ctx->err_next == NULL) {
        free(ctx->err_cur);
        free(ctx->err_next);
        free(ctx);
        return NULL;
    }
    return ctx;
}

void dither_free(dither_ctx *ctx)
{
    if (ctx == NULL) {
        return;
    }
    free(ctx->err_cur);
    free(ctx->err_next);
    free(ctx);
}

int dither_bayer8(dither_ctx *ctx,
                  const uint8_t *src, uint8_t *dst)
{
    if (ctx == NULL || src == NULL || dst == NULL) {
        return -1;
    }
    const size_t w = ctx->width;
    const size_t h = ctx->height;

    for (size_t y = 0; y < h; ++y) {
        const uint8_t *srow = src + y * w;
        uint8_t *drow = dst + y * w;
        const uint8_t *brow = BAYER8 + (y & 7) * 8;
        for (size_t x = 0; x < w; ++x) {
            /* Bayer threshold: 0..63 -> scaled 0..255-ish (x4) */
            const int thresh = (int)brow[x & 7] * 4;
            drow[x] = (srow[x] > thresh) ? 255 : 0;
        }
    }
    return 0;
}

int dither_threshold(dither_ctx *ctx,
                     const uint8_t *src, uint8_t *dst)
{
    if (ctx == NULL || src == NULL || dst == NULL) {
        return -1;
    }
    const size_t n = ctx->width * ctx->height;
    for (size_t i = 0; i < n; ++i) {
        dst[i] = (src[i] > 128) ? 255 : 0;
    }
    return 0;
}

int dither_floyd_steinberg(dither_ctx *ctx,
                           const uint8_t *src, uint8_t *dst)
{
    if (ctx == NULL || src == NULL || dst == NULL) {
        return -1;
    }
    const size_t w = ctx->width;
    const size_t h = ctx->height;

    /* error buffers: err_next holds accumulated diffusion for the
     * current row as we process; err_cur is reused as rolling. */
    float *err = ctx->err_cur;   /* current row diffusion (left->right) */
    float *nxt = ctx->err_next;  /* accumulated for the row below */

    memset(err, 0, w * sizeof(float));
    memset(nxt, 0, w * sizeof(float));

    for (size_t y = 0; y < h; ++y) {
        const uint8_t *srow = src + y * w;
        uint8_t *drow = dst + y * w;

        for (size_t x = 0; x < w; ++x) {
            float old_pixel = (float)srow[x] + err[x];
            uint8_t new_pixel = (old_pixel > 128.0f) ? 255 : 0;
            float error = old_pixel - (float)new_pixel;
            drow[x] = new_pixel;

            /* right 7/16 */
            if (x + 1 < w) {
                err[x + 1] += error * (7.0f / 16.0f);
            }
            /* down-left 3/16 */
            if (x > 0) {
                nxt[x - 1] += error * (3.0f / 16.0f);
            }
            /* down 5/16 */
            nxt[x] += error * (5.0f / 16.0f);
            /* down-right 1/16 */
            if (x + 1 < w) {
                nxt[x + 1] += error * (1.0f / 16.0f);
            }
        }

        /* advance: next row's diffusion becomes current */
        float *tmp = err;
        err = nxt;
        nxt = tmp;
        memset(nxt, 0, w * sizeof(float));
    }

    return 0;
}
