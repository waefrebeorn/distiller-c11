#ifndef IMAGE_H
#define IMAGE_H

/* image.h — image operations for the e-ink pipeline (C11).
 *
 * Port of the v2 SDK's hardware/eink/composer/image_ops.py:
 * grayscale conversion, resize, crop. Opaque API, no deps.
 * Frames are row-major uint8 (grayscale) or uint8 RGB(A) triples.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert RGB (3 bytes/px) or RGBA (4 bytes/px) to grayscale
 * (1 byte/px). out must hold w*h bytes. */
void image_rgb_to_gray(const uint8_t *rgb, size_t w, size_t h,
                       int has_alpha, uint8_t *out);

/* Nearest-neighbor resize: src w0xh0 -> dst w1xh1. */
void image_resize_nearest(const uint8_t *src, size_t w0, size_t h0,
                          uint8_t *dst, size_t w1, size_t h1);

/* Bilinear resize: src w0xh0 -> dst w1xh1. */
void image_resize_bilinear(const uint8_t *src, size_t w0, size_t h0,
                           uint8_t *dst, size_t w1, size_t h1);

/* Crop a w0xh0 frame: src -> dst (cw x ch), top-left at (ox, oy). */
int image_crop(const uint8_t *src, size_t w0, size_t h0,
               uint8_t *dst, size_t ox, size_t oy,
               size_t cw, size_t ch);

/* Rotate 90/180/270 clockwise. dst must be hxw for 90/270. */
void image_rotate90(const uint8_t *src, size_t w, size_t h,
                    uint8_t *dst);
void image_rotate180(const uint8_t *src, size_t w, size_t h,
                     uint8_t *dst);
void image_rotate270(const uint8_t *src, size_t w, size_t h,
                     uint8_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* IMAGE_H */
