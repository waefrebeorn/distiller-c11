/* test_image.c — unit tests for the image module. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image/image.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    /* rgb -> gray: pure red is ~77 (BT.601), black 0, white 255 */
    uint8_t rgb[3] = {255, 0, 0};
    uint8_t g;
    image_rgb_to_gray(rgb, 1, 1, 0, &g);
    CHECK(g > 40 && g < 120, "red luma ~77");

    uint8_t white[3] = {255, 255, 255};
    image_rgb_to_gray(white, 1, 1, 0, &g);
    CHECK(g == 255, "white -> 255");

    uint8_t black[3] = {0, 0, 0};
    image_rgb_to_gray(black, 1, 1, 0, &g);
    CHECK(g == 0, "black -> 0");

    /* resize nearest: 2x2 all-200 -> 1x1 = 200 */
    uint8_t src2[4] = {200, 200, 200, 200};
    uint8_t dst1;
    image_resize_nearest(src2, 2, 2, &dst1, 1, 1);
    CHECK(dst1 == 200, "nearest 2x2->1x1");

    /* resize nearest: 1x1 100 -> 3x3 all 100 */
    uint8_t src1 = 100;
    uint8_t dst9[9];
    image_resize_nearest(&src1, 1, 1, dst9, 3, 3);
    for (int i = 0; i < 9; i++) CHECK(dst9[i] == 100, "nearest upscale");

    /* crop */
    uint8_t big[16]; /* 4x4 = 0..15 */
    for (int i = 0; i < 16; i++) big[i] = (uint8_t)i;
    uint8_t crop[4]; /* 2x2 at (1,1) = 5,6,9,10 */
    int rc = image_crop(big, 4, 4, crop, 1, 1, 2, 2);
    CHECK(rc == 0, "crop rc");
    CHECK(crop[0] == 5 && crop[1] == 6 && crop[2] == 9 && crop[3] == 10,
          "crop values");

    /* out-of-bounds crop fails */
    rc = image_crop(big, 4, 4, crop, 3, 3, 2, 2);
    CHECK(rc == -1, "crop oob");

    /* rotate90: 2x3 -> 3x2 */
    uint8_t r2x3[6] = {1, 2, 3, 4, 5, 6}; /* rows: [1 2 3] [4 5 6] */
    uint8_t r3x2[6];
    image_rotate90(r2x3, 3, 2, r3x2);
    /* expected 3x2: [4 1] [5 2] [6 3] */
    CHECK(r3x2[0] == 4 && r3x2[1] == 1, "rot90 r0");
    CHECK(r3x2[2] == 5 && r3x2[3] == 2, "rot90 r1");
    CHECK(r3x2[4] == 6 && r3x2[5] == 3, "rot90 r2");

    /* rotate180: identity order reversed */
    uint8_t r180[6];
    image_rotate180(r2x3, 3, 2, r180);
    CHECK(r180[0] == 6 && r180[5] == 1, "rot180 ends");

    if (failures == 0) {
        printf("test_image: ALL PASS\n");
        return 0;
    }
    printf("test_image: %d FAILURES\n", failures);
    return 1;
}
