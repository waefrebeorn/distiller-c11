#ifndef PACK_H
#define PACK_H

/* pack.h — 1-bit packing for e-ink frames (C11, opaque API).
 *
 * Converts a binary (0/255) HxW grayscale frame into the
 * 1-bit-per-pixel byte stream the e-ink driver expects:
 * LSB = leftmost pixel, each row padded to a byte boundary.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct pack_ctx pack_ctx;

/* Create a packer for width x height frames. */
pack_ctx *pack_create(size_t width, size_t height);

void pack_free(pack_ctx *ctx);

/* Pack a binary frame (0/255, HxW row-major) into out.
 * out must hold pack_size(ctx) bytes. Returns bytes written,
 * or -1 on error. */
long pack_bits(pack_ctx *ctx, const uint8_t *frame, uint8_t *out);

/* Bytes needed for one packed frame. */
size_t pack_size(const pack_ctx *ctx);

/* Row byte width (padded to multiple of 8). */
size_t pack_row_bytes(const pack_ctx *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PACK_H */
