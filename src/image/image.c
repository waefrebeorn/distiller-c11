/* image.c — image operations for the e-ink pipeline (C11). */

#include "image.h"

#include <stdlib.h>
#include <string.h>

void image_rgb_to_gray(const uint8_t *rgb, size_t w, size_t h,
                       int has_alpha, uint8_t *out)
{
    const size_t step = has_alpha ? 4 : 3;
    const size_t n = w * h;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t *p = rgb + i * step;
        /* ITU-R BT.601 luma */
        out[i] = (uint8_t)((77u * p[0] + 150u * p[1] + 29u * p[2]) >> 8);
    }
}

void image_resize_nearest(const uint8_t *src, size_t w0, size_t h0,
                          uint8_t *dst, size_t w1, size_t h1)
{
    for (size_t y = 0; y < h1; ++y) {
        size_t sy = (y * h0) / h1;
        if (sy >= h0) sy = h0 - 1;
        const uint8_t *srow = src + sy * w0;
        uint8_t *drow = dst + y * w1;
        for (size_t x = 0; x < w1; ++x) {
            size_t sx = (x * w0) / w1;
            if (sx >= w0) sx = w0 - 1;
            drow[x] = srow[sx];
        }
    }
}

void image_resize_bilinear(const uint8_t *src, size_t w0, size_t h0,
                           uint8_t *dst, size_t w1, size_t h1)
{
    if (w0 == 0 || h0 == 0 || w1 == 0 || h1 == 0) {
        return;
    }
    for (size_t y = 0; y < h1; ++y) {
        float fy = (float)y * (float)(h0 - 1) / (float)(h1 - 1);
        size_t y0 = (size_t)fy;
        size_t y1 = y0 + 1;
        if (y1 >= h0) y1 = h0 - 1;
        float wy = fy - (float)y0;

        const uint8_t *r0 = src + y0 * w0;
        const uint8_t *r1 = src + y1 * w0;
        uint8_t *drow = dst + y * w1;

        for (size_t x = 0; x < w1; ++x) {
            float fx = (float)x * (float)(w0 - 1) / (float)(w1 - 1);
            size_t x0 = (size_t)fx;
            size_t x1 = x0 + 1;
            if (x1 >= w0) x1 = w0 - 1;
            float wx = fx - (float)x0;

            float top = (float)r0[x0] * (1.0f - wx) + (float)r0[x1] * wx;
            float bot = (float)r1[x0] * (1.0f - wx) + (float)r1[x1] * wx;
            float v = top * (1.0f - wy) + bot * wy;
            drow[x] = (uint8_t)(v + 0.5f);
        }
    }
}

int image_crop(const uint8_t *src, size_t w0, size_t h0,
               uint8_t *dst, size_t ox, size_t oy,
               size_t cw, size_t ch)
{
    if (ox + cw > w0 || oy + ch > h0) {
        return -1;
    }
    for (size_t y = 0; y < ch; ++y) {
        memcpy(dst + y * cw, src + (oy + y) * w0 + ox, cw);
    }
    return 0;
}

void image_rotate90(const uint8_t *src, size_t w, size_t h,
                    uint8_t *dst)
{
    /* dst is h x w; pixel (x,y) -> dst[y][h-1-x] */
    for (size_t y = 0; y < h; ++y) {
        for (size_t x = 0; x < w; ++x) {
            dst[x * h + (h - 1 - y)] = src[y * w + x];
        }
    }
}

void image_rotate180(const uint8_t *src, size_t w, size_t h,
                     uint8_t *dst)
{
    for (size_t y = 0; y < h; ++y) {
        for (size_t x = 0; x < w; ++x) {
            dst[(h - 1 - y) * w + (w - 1 - x)] = src[y * w + x];
        }
    }
}

void image_rotate270(const uint8_t *src, size_t w, size_t h,
                     uint8_t *dst)
{
    for (size_t y = 0; y < h; ++y) {
        for (size_t x = 0; x < w; ++x) {
            dst[(w - 1 - x) * h + y] = src[y * w + x];
        }
    }
}
