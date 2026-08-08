/* eink.c — the full e-ink frame pipeline (C11). */

#include "eink.h"

#include <stdlib.h>
#include <string.h>

#include "../dither/dither.h"
#include "../image/image.h"
#include "../pack/pack.h"

struct eink_frame {
    size_t width;
    size_t height;
    enum eink_dither mode;

    dither_ctx *dither;
    pack_ctx *pack;

    /* scratch buffers */
    uint8_t *gray;    /* w*h */
    uint8_t *binary;  /* w*h */
    uint8_t *packed;  /* row_bytes*h */
};

eink_frame *eink_create(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        return NULL;
    }
    eink_frame *ctx = calloc(1, sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->width = width;
    ctx->height = height;
    ctx->mode = EINK_DITHER_FLOYD;

    ctx->dither = dither_create(width, height);
    ctx->pack = pack_create(width, height);
    if (ctx->dither == NULL || ctx->pack == NULL) {
        eink_free(ctx);
        return NULL;
    }
    ctx->gray = malloc(width * height);
    ctx->binary = malloc(width * height);
    ctx->packed = malloc(pack_size(ctx->pack));
    if (ctx->gray == NULL || ctx->binary == NULL || ctx->packed == NULL) {
        eink_free(ctx);
        return NULL;
    }
    return ctx;
}

void eink_free(eink_frame *ctx)
{
    if (ctx == NULL) {
        return;
    }
    free(ctx->gray);
    free(ctx->binary);
    free(ctx->packed);
    dither_free(ctx->dither);
    pack_free(ctx->pack);
    free(ctx);
}

size_t eink_packed_size(const eink_frame *ctx)
{
    return ctx ? pack_size(ctx->pack) : 0;
}

size_t eink_width(const eink_frame *ctx)
{
    return ctx ? ctx->width : 0;
}

size_t eink_height(const eink_frame *ctx)
{
    return ctx ? ctx->height : 0;
}

/* Internal: gray -> binary (per mode) -> packed bytes. */
static long render_gray_internal(eink_frame *ctx, const uint8_t *gray,
                                 uint8_t *out)
{
    int rc;
    switch (ctx->mode) {
    case EINK_DITHER_BAYER8:
        rc = dither_bayer8(ctx->dither, gray, ctx->binary);
        break;
    case EINK_DITHER_THRESHOLD:
        rc = dither_threshold(ctx->dither, gray, ctx->binary);
        break;
    case EINK_DITHER_FLOYD:
    default:
        rc = dither_floyd_steinberg(ctx->dither, gray, ctx->binary);
        break;
    }
    if (rc != 0) {
        return -1;
    }
    return pack_bits(ctx->pack, ctx->binary, out);
}

long eink_render_gray(eink_frame *ctx, const uint8_t *gray, uint8_t *out)
{
    if (ctx == NULL || gray == NULL || out == NULL) {
        return -1;
    }
    return render_gray_internal(ctx, gray, out);
}

long eink_render_rgb(eink_frame *ctx, const uint8_t *rgb, uint8_t *out)
{
    if (ctx == NULL || rgb == NULL || out == NULL) {
        return -1;
    }
    image_rgb_to_gray(rgb, ctx->width, ctx->height, 0, ctx->gray);
    return render_gray_internal(ctx, ctx->gray, out);
}

long eink_render_rgba(eink_frame *ctx, const uint8_t *rgba, uint8_t *out)
{
    if (ctx == NULL || rgba == NULL || out == NULL) {
        return -1;
    }
    image_rgb_to_gray(rgba, ctx->width, ctx->height, 1, ctx->gray);
    return render_gray_internal(ctx, ctx->gray, out);
}
