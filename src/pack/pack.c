/* pack.c — 1-bit packing for e-ink frames (C11). */

#include "pack.h"

#include <stdlib.h>
#include <string.h>

struct pack_ctx {
    size_t width;
    size_t height;
    size_t row_bytes;
};

pack_ctx *pack_create(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        return NULL;
    }
    pack_ctx *ctx = malloc(sizeof(*ctx));
    if (ctx == NULL) {
        return NULL;
    }
    ctx->width = width;
    ctx->height = height;
    ctx->row_bytes = (width + 7) / 8;
    return ctx;
}

void pack_free(pack_ctx *ctx)
{
    free(ctx);
}

size_t pack_row_bytes(const pack_ctx *ctx)
{
    return ctx ? ctx->row_bytes : 0;
}

size_t pack_size(const pack_ctx *ctx)
{
    return ctx ? ctx->row_bytes * ctx->height : 0;
}

long pack_bits(pack_ctx *ctx, const uint8_t *frame, uint8_t *out)
{
    if (ctx == NULL || frame == NULL || out == NULL) {
        return -1;
    }
    const size_t w = ctx->width;
    const size_t rb = ctx->row_bytes;

    for (size_t y = 0; y < ctx->height; ++y) {
        const uint8_t *srow = frame + y * w;
        uint8_t *drow = out + y * rb;
        memset(drow, 0, rb);
        for (size_t x = 0; x < w; ++x) {
            if (srow[x] != 0) {
                drow[x / 8] |= (uint8_t)(1u << (7 - (x % 8))); /* MSB = left */
            }
        }
    }
    return (long)(ctx->row_bytes * ctx->height);
}
