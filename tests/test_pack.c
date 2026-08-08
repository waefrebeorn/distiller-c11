/* test_pack.c — unit tests for the pack module. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pack/pack.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    /* width 10 -> 2 bytes/row, height 3 -> 6 bytes total */
    pack_ctx *ctx = pack_create(10, 3);
    CHECK(ctx != NULL, "create");
    CHECK(pack_row_bytes(ctx) == 2, "row bytes = 2");
    CHECK(pack_size(ctx) == 6, "packed size = 6");

    uint8_t frame[30];
    uint8_t out[6];
    memset(frame, 0, sizeof(frame));
    memset(out, 0xAA, sizeof(out));

    /* frame with only the leftmost pixel of row 0 set */
    frame[0] = 255;
    long n = pack_bits(ctx, frame, out);
    CHECK(n == 6, "returns 6");
    CHECK(out[0] == 0x01, "LSB = leftmost pixel");
    CHECK(out[1] == 0x00, "row0 byte1 empty");

    /* second pixel (x=1) -> bit 1 */
    memset(frame, 0, sizeof(frame));
    memset(out, 0, sizeof(out));
    frame[1] = 255;
    pack_bits(ctx, frame, out);
    CHECK(out[0] == 0x02, "x=1 -> bit1");

    /* pixel x=8 -> second byte bit 0 */
    memset(frame, 0, sizeof(frame));
    memset(out, 0, sizeof(out));
    frame[8] = 255;
    pack_bits(ctx, frame, out);
    CHECK(out[1] == 0x01, "x=8 -> byte1 bit0");

    /* row 2 pixel 0 -> out[4] bit 0 */
    memset(frame, 0, sizeof(frame));
    memset(out, 0, sizeof(out));
    frame[20] = 255;
    pack_bits(ctx, frame, out);
    CHECK(out[4] == 0x01, "row2 x0 -> byte4 bit0");

    /* NULL checks */
    CHECK(pack_bits(NULL, frame, out) == -1, "null ctx");
    CHECK(pack_bits(ctx, NULL, out) == -1, "null frame");
    CHECK(pack_bits(ctx, frame, NULL) == -1, "null out");

    pack_free(ctx);
    if (failures == 0) {
        printf("test_pack: ALL PASS\n");
        return 0;
    }
    printf("test_pack: %d FAILURES\n", failures);
    return 1;
}
