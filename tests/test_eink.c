/* test_eink.c — full-pipeline test: gray -> dither -> pack. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eink/eink.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    eink_frame *e = eink_create(240, 416);
    CHECK(e != NULL, "create 240x416");
    CHECK(eink_width(e) == 240, "width");
    CHECK(eink_height(e) == 416, "height");
    CHECK(eink_packed_size(e) == 30 * 416, "packed = 12480");

    uint8_t *gray = malloc(240 * 416);
    uint8_t *out = malloc(eink_packed_size(e));
    CHECK(gray && out, "buffers");

    /* all black -> all zero bits */
    memset(gray, 0, 240 * 416);
    long n = eink_render_gray(e, gray, out);
    CHECK(n == 12480, "render rc");
    int nonzero = 0;
    for (long i = 0; i < n; i++) if (out[i]) nonzero = 1;
    CHECK(!nonzero, "black -> all zero");

    /* all white -> all 0xFF */
    memset(gray, 255, 240 * 416);
    eink_render_gray(e, gray, out);
    int notff = 0;
    for (long i = 0; i < n; i++) if (out[i] != 0xFF) notff = 1;
    CHECK(!notff, "white -> all FF");

    /* gradient produces a mix */
    for (int i = 0; i < 240 * 416; i++) gray[i] = (uint8_t)(i & 255);
    eink_render_gray(e, gray, out);
    int has0 = 0, hasFF = 0;
    for (long i = 0; i < n; i++) {
        if (out[i] == 0) has0 = 1;
        if (out[i] == 0xFF) hasFF = 1;
    }
    CHECK(has0 && hasFF, "gradient mix");

    /* RGB path: white rgb -> FF */
    uint8_t *rgb = malloc(240 * 416 * 3);
    memset(rgb, 255, 240 * 416 * 3);
    long nrgb = eink_render_rgb(e, rgb, out);
    CHECK(nrgb == n, "rgb rc");
    int notff2 = 0;
    for (long i = 0; i < n; i++) if (out[i] != 0xFF) notff2 = 1;
    CHECK(!notff2, "rgb white -> FF");

    /* NULL safety */
    CHECK(eink_render_gray(NULL, gray, out) == -1, "null eink");
    CHECK(eink_render_gray(e, NULL, out) == -1, "null gray");
    CHECK(eink_render_gray(e, gray, NULL) == -1, "null out");

    free(rgb);
    free(out);
    free(gray);
    eink_free(e);

    if (failures == 0) {
        printf("test_eink: ALL PASS\n");
        return 0;
    }
    printf("test_eink: %d FAILURES\n", failures);
    return 1;
}
