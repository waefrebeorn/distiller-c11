/* compose.c — text/canvas composition for the e-ink (C11). */

#include "compose.h"

#include <stdlib.h>
#include <string.h>

struct canvas {
    size_t width;
    size_t height;
    uint8_t *data;
};

/* 5x7 bitmap font for ASCII 32..126. Each glyph is 5 bytes (rows),
 * 7 bits per row (LSB = left). Built from the classic 5x7 font
 * table. NULL entries render as a space. */
static const uint8_t FONT5X7[95][5] = {
    /* 32 space */ {0, 0, 0, 0, 0},
    /* 33 ! */     {0, 0, 95, 0, 0},
    /* 34 " */     {0, 7, 0, 7, 0},
    /* 35 # */     {20, 127, 20, 127, 20},
    /* 36 $ */     {36, 42, 127, 42, 18},
    /* 37 % */     {35, 19, 8, 100, 97},
    /* 38 & */     {54, 73, 85, 34, 80},
    /* 39 ' */     {0, 5, 3, 0, 0},
    /* 40 ( */     {0, 28, 34, 65, 0},
    /* 41 ) */     {0, 65, 34, 28, 0},
    /* 42 * */     {84, 56, 127, 56, 84},
    /* 43 + */     {8, 8, 62, 8, 8},
    /* 44 , */     {0, 80, 48, 0, 0},
    /* 45 - */     {8, 8, 8, 8, 8},
    /* 46 . */     {0, 96, 96, 0, 0},
    /* 47 / */     {32, 16, 8, 4, 2},
    /* 48 0 */     {62, 81, 73, 69, 62},
    /* 49 1 */     {0, 66, 127, 64, 0},
    /* 50 2 */     {66, 97, 81, 73, 70},
    /* 51 3 */     {33, 65, 69, 75, 49},
    /* 52 4 */     {24, 20, 18, 127, 16},
    /* 53 5 */     {39, 69, 69, 69, 57},
    /* 54 6 */     {60, 74, 73, 73, 48},
    /* 55 7 */     {1, 113, 9, 5, 3},
    /* 56 8 */     {54, 73, 73, 73, 54},
    /* 57 9 */     {6, 73, 73, 41, 62},
    /* 58 : */     {0, 54, 54, 0, 0},
    /* 59 ; */     {0, 86, 54, 0, 0},
    /* 60 < */     {0, 8, 20, 34, 65},
    /* 61 = */     {20, 20, 20, 20, 20},
    /* 62 > */     {65, 34, 20, 8, 0},
    /* 63 ? */     {2, 1, 81, 9, 6},
    /* 64 @ */     {50, 73, 121, 65, 62},
    /* 65 A */     {126, 17, 17, 17, 126},
    /* 66 B */     {127, 73, 73, 73, 54},
    /* 67 C */     {62, 65, 65, 65, 34},
    /* 68 D */     {127, 65, 65, 34, 28},
    /* 69 E */     {127, 73, 73, 73, 65},
    /* 70 F */     {127, 9, 9, 1, 1},
    /* 71 G */     {62, 65, 65, 81, 50},
    /* 72 H */     {127, 8, 8, 8, 127},
    /* 73 I */     {0, 65, 127, 65, 0},
    /* 74 J */     {32, 64, 65, 63, 1},
    /* 75 K */     {127, 8, 20, 34, 65},
    /* 76 L */     {127, 64, 64, 64, 64},
    /* 77 M */     {127, 2, 12, 2, 127},
    /* 78 N */     {127, 4, 8, 16, 127},
    /* 79 O */     {62, 65, 65, 65, 62},
    /* 80 P */     {127, 9, 9, 9, 6},
    /* 81 Q */     {62, 65, 81, 33, 94},
    /* 82 R */     {127, 9, 25, 41, 70},
    /* 83 S */     {70, 73, 73, 73, 49},
    /* 84 T */     {1, 1, 127, 1, 1},
    /* 85 U */     {63, 64, 64, 64, 63},
    /* 86 V */     {31, 32, 64, 32, 31},
    /* 87 W */     {127, 32, 24, 32, 127},
    /* 88 X */     {99, 20, 8, 20, 99},
    /* 89 Y */     {3, 4, 120, 4, 3},
    /* 90 Z */     {97, 81, 73, 69, 67},
    /* 91 [ */     {0, 127, 65, 65, 0},
    /* 92 \ */     {2, 4, 8, 16, 32},
    /* 93 ] */     {0, 65, 65, 127, 0},
    /* 94 ^ */     {4, 2, 1, 2, 4},
    /* 95 _ */     {64, 64, 64, 64, 64},
    /* 96 ` */     {0, 1, 2, 4, 0},
    /* 97 a */     {32, 84, 84, 84, 120},
    /* 98 b */     {127, 72, 68, 68, 56},
    /* 99 c */     {56, 68, 68, 68, 32},
    /* 100 d */    {56, 68, 68, 72, 127},
    /* 101 e */    {56, 84, 84, 84, 24},
    /* 102 f */    {8, 126, 9, 1, 2},
    /* 103 g */    {12, 82, 82, 82, 62},
    /* 104 h */    {127, 8, 4, 4, 120},
    /* 105 i */    {0, 68, 125, 64, 0},
    /* 106 j */    {32, 64, 68, 62, 4},
    /* 107 k */    {127, 16, 40, 68, 0},
    /* 108 l */    {0, 65, 127, 64, 0},
    /* 109 m */    {124, 4, 24, 4, 120},
    /* 110 n */    {124, 8, 4, 4, 120},
    /* 111 o */    {56, 68, 68, 68, 56},
    /* 112 p */    {124, 20, 20, 20, 8},
    /* 113 q */    {8, 20, 20, 24, 124},
    /* 114 r */    {124, 8, 4, 4, 8},
    /* 115 s */    {72, 84, 84, 84, 32},
    /* 116 t */    {4, 63, 68, 64, 32},
    /* 117 u */    {60, 64, 64, 32, 124},
    /* 118 v */    {28, 32, 64, 32, 28},
    /* 119 w */    {60, 64, 48, 64, 60},
    /* 120 x */    {68, 40, 16, 40, 68},
    /* 121 y */    {12, 80, 80, 80, 60},
    /* 122 z */    {68, 100, 84, 76, 68},
    /* 123 { */    {0, 8, 54, 65, 0},
    /* 124 | */    {0, 0, 127, 0, 0},
    /* 125 } */    {0, 65, 54, 8, 0},
    /* 126 ~ */    {16, 8, 16, 32, 16},
};

canvas *canvas_create(size_t width, size_t height)
{
    if (width == 0 || height == 0) {
        return NULL;
    }
    canvas *c = malloc(sizeof(*c));
    if (c == NULL) {
        return NULL;
    }
    c->width = width;
    c->height = height;
    c->data = malloc(width * height);
    if (c->data == NULL) {
        free(c);
        return NULL;
    }
    memset(c->data, 255, width * height); /* white */
    return c;
}

void canvas_free(canvas *ctx)
{
    if (ctx == NULL) {
        return;
    }
    free(ctx->data);
    free(ctx);
}

uint8_t *canvas_data(canvas *ctx) { return ctx ? ctx->data : NULL; }
size_t canvas_width(const canvas *ctx) { return ctx ? ctx->width : 0; }
size_t canvas_height(const canvas *ctx) { return ctx ? ctx->height : 0; }

void canvas_fill_rect(canvas *ctx, size_t x, size_t y,
                      size_t w, size_t h)
{
    if (ctx == NULL) {
        return;
    }
    for (size_t yy = y; yy < y + h && yy < ctx->height; ++yy) {
        for (size_t xx = x; xx < x + w && xx < ctx->width; ++xx) {
            ctx->data[yy * ctx->width + xx] = 0;
        }
    }
}

void canvas_invert(canvas *ctx)
{
    if (ctx == NULL) {
        return;
    }
    const size_t n = ctx->width * ctx->height;
    for (size_t i = 0; i < n; ++i) {
        ctx->data[i] = (uint8_t)(255 - ctx->data[i]);
    }
}

int canvas_draw_text(canvas *ctx, const char *text,
                     size_t x, size_t y, unsigned scale)
{
    if (ctx == NULL || text == NULL || scale == 0) {
        return -1;
    }
    if (scale > 4) {
        scale = 4;
    }
    size_t cx = x;
    for (const char *p = text; *p; ++p) {
        unsigned char ch = (unsigned char)*p;
        if (ch == '\n') {
            cx = x;
            continue;
        }
        if (ch < 32 || ch > 126) {
            ch = ' ';
        }
        const uint8_t *glyph = FONT5X7[ch - 32];
        for (int row = 0; row < 5; ++row) {
            uint8_t bits = glyph[row];
            for (int bit = 0; bit < 7; ++bit) {
                if (bits & (1u << bit)) {
                    for (unsigned sy = 0; sy < scale; ++sy) {
                        for (unsigned sx = 0; sx < scale; ++sx) {
                            size_t px = cx + (size_t)bit * scale + sx;
                            size_t py = y + (size_t)row * scale + sy;
                            if (px < ctx->width && py < ctx->height) {
                                ctx->data[py * ctx->width + px] = 0;
                            }
                        }
                    }
                }
            }
        }
        cx += 6 * scale; /* 5 wide + 1 spacing */
    }
    return 0;
}
