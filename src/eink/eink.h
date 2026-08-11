#ifndef EINK_H
#define EINK_H

/* eink.h — the full e-ink frame pipeline (C11, opaque API).
 *
 * image -> grayscale -> dither (bayer8/fs/threshold) -> pack (1-bit)
 * One context owns all scratch; no per-call allocation.
 * This is the C11 replacement for the v1's numba JIT path
 * (distiller/peripheral/eink.py) — zero cold start.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct eink_frame eink_frame;

/* Create a frame pipeline for width x height (e.g. 240x416).
 * Returns NULL on allocation failure. */
eink_frame *eink_create(size_t width, size_t height);

void eink_free(eink_frame *ctx);

/* Choose the dither mode for subsequent renders. */
enum eink_dither {
    EINK_DITHER_BAYER8 = 0,
    EINK_DITHER_FLOYD = 1,
    EINK_DITHER_THRESHOLD = 2
};

/* Choose the dither mode for subsequent renders. */
void eink_set_dither_mode(eink_frame *ctx, enum eink_dither mode);

/* Render a grayscale frame (HxW uint8) to packed 1-bit bytes.
 * out must hold eink_packed_size(ctx) bytes.
 * Returns bytes written, or -1 on error. */
long eink_render_gray(eink_frame *ctx, const uint8_t *gray,
                      uint8_t *out);

/* Render an RGB frame (w*h*3 bytes) to packed 1-bit bytes. */
long eink_render_rgb(eink_frame *ctx, const uint8_t *rgb,
                     uint8_t *out);

/* Render an RGBA frame (w*h*4 bytes) to packed 1-bit bytes. */
long eink_render_rgba(eink_frame *ctx, const uint8_t *rgba,
                      uint8_t *out);

/* Packed frame size in bytes. */
size_t eink_packed_size(const eink_frame *ctx);

/* Width/height accessors. */
size_t eink_width(const eink_frame *ctx);
size_t eink_height(const eink_frame *ctx);

#ifdef __cplusplus
}
#endif

#endif /* EINK_H */
