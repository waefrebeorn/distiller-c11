#ifndef COMPOSE_H
#define COMPOSE_H

/* compose.h — text/canvas composition for the e-ink (C11).
 *
 * Port of the v2 SDK's template_renderer/text concepts: draw
 * text onto a grayscale canvas with a simple 5x7 bitmap font.
 * No external font files, no deps — fully self-contained.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct canvas canvas;

/* Create a grayscale canvas w x h, initialized to white (255). */
canvas *canvas_create(size_t width, size_t height);
void canvas_free(canvas *ctx);

/* Raw buffer access (w*h bytes, row-major). */
uint8_t *canvas_data(canvas *ctx);
size_t canvas_width(const canvas *ctx);
size_t canvas_height(const canvas *ctx);

/* Draw a text string at (x, y) with scale 1..4 (font is 5x7 per
 * cell; scale multiplies). Pixels are black (0). Clipped to the
 * canvas. Returns 0 on success. */
int canvas_draw_text(canvas *ctx, const char *text,
                     size_t x, size_t y, unsigned scale);

/* Draw a filled rectangle (0 = black). */
void canvas_fill_rect(canvas *ctx, size_t x, size_t y,
                      size_t w, size_t h);

/* Invert the whole canvas (255<->0). */
void canvas_invert(canvas *ctx);

#ifdef __cplusplus
}
#endif

#endif /* COMPOSE_H */
