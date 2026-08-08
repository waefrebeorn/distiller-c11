#ifndef DITHER_H
#define DITHER_H

/* dither.h — e-ink dithering kernels (C11, opaque API).
 *
 * Two algorithms, both JIT-free and dependency-free:
 *   - bayer8:  ordered 8x8 dithering, fully vectorizable
 *   - floyd_steinberg: error diffusion, row-vectorized
 * Input:  grayscale uint8 HxW row-major
 * Output: binary uint8 (0 or 255) same shape
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct dither_ctx dither_ctx;

/* Create a dither context for a given frame size. Returns NULL on
 * allocation failure. */
dither_ctx *dither_create(size_t width, size_t height);

void dither_free(dither_ctx *ctx);

/* Ordered 8x8 Bayer dithering: src -> dst (0/255). Fast, no error
 * diffusion, ideal for photos. src/dst are HxW row-major grayscale. */
int dither_bayer8(dither_ctx *ctx,
                  const uint8_t *src, uint8_t *dst);

/* Floyd-Steinberg error diffusion (row-vectorized): src -> dst
 * (0/255). Best quality for text/UI. */
int dither_floyd_steinberg(dither_ctx *ctx,
                           const uint8_t *src, uint8_t *dst);

/* Simple threshold at 128. */
int dither_threshold(dither_ctx *ctx,
                     const uint8_t *src, uint8_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* DITHER_H */
