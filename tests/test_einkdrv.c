/* test_einkdrv.c — oracle test: verify the C11 driver emits the
 * exact SPI sequence the v1 eink_dsp.py protocol requires. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw/einkdrv/einkdrv.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

/* --- recording transport --- */
static uint8_t seq[65536];
static int seq_len = 0;
static int dc_level = -1;
static int busy_state = 1; /* ready */
static int delay_ms_total = 0;

static int spi_rec(void *u, uint8_t b) { (void)u; seq[seq_len++] = b; return 0; }
static int dc_rec(void *u, int l) { (void)u; dc_level = l; return 0; }
static int rst_rec(void *u, int l) { (void)u; (void)l; return 0; }
static int busy_rec(void *u) { (void)u; return busy_state; }
static void delay_rec(void *u, uint32_t ms) { (void)u; delay_ms_total += (int)ms; }

int main(void)
{
    einkdrv_ops ops = {
        .spi_write = spi_rec,
        .gpio_dc = dc_rec,
        .gpio_rst = rst_rec,
        .gpio_busy = busy_rec,
        .delay_ms = delay_rec,
        .user = NULL,
    };

    /* NULL safety */
    CHECK(einkdrv_create(NULL) == NULL, "null ops rejected");
    einkdrv_ops bad = ops;
    bad.spi_write = NULL;
    CHECK(einkdrv_create(&bad) == NULL, "null spi rejected");

    einkdrv *d = einkdrv_create(&ops);
    CHECK(d != NULL, "create");

    /* init: reset delays (100+20+20) then 0x04 power on,
     * busy wait, 0x50 0x97 VCOM */
    seq_len = 0;
    delay_ms_total = 0;
    CHECK(einkdrv_init(d) == 0, "init ok");
    CHECK(delay_ms_total == 140, "init reset delays 100+20+20");
    /* sequence should start with 0x04 (power on) */
    CHECK(seq_len >= 3 && seq[0] == 0x04, "init: 0x04 power on first");
    /* find 0x50 then 0x97 after it */
    int found50 = -1, found97 = -1;
    for (int i = 0; i < seq_len; i++) {
        if (seq[i] == 0x50 && found50 < 0) found50 = i;
        if (found50 >= 0 && i > found50 && seq[i] == 0x97 && found97 < 0) found97 = i;
    }
    CHECK(found50 >= 0, "init sends 0x50 (VCOM interval)");
    CHECK(found97 > found50, "init sends 0x97 data after 0x50");

    /* display: 0x10 + 12480 image bytes + 0x13 + 12480 zeros +
     * 0x12 refresh + 1ms delay, then busy wait.
     * Layout: seq[0]=0x10 cmd, seq[1..12480]=image (12480 bytes),
     * seq[12481]=0x13, seq[12482..24961]=zeros, seq[24962]=0x12 */
    static uint8_t frame[EPD_FRAME_BYTES];
    for (int i = 0; i < EPD_FRAME_BYTES; i++) frame[i] = (uint8_t)(i & 0xFF);
    seq_len = 0;
    delay_ms_total = 0;
    CHECK(einkdrv_display(d, frame) == 0, "display ok");
    CHECK(seq[0] == 0x10, "display: 0x10 write old first");
    /* image bytes follow */
    CHECK(seq[1] == (uint8_t)(0 & 0xFF), "display: image byte 0");
    CHECK(seq[EPD_FRAME_BYTES + 1] == 0x13, "display: 0x13 write new after image");
    CHECK(seq[EPD_FRAME_BYTES + 2] == 0x00, "display: zeros for new");
    CHECK(seq[EPD_FRAME_BYTES * 2 + 2] == 0x12, "display: 0x12 refresh");
    CHECK(seq_len == EPD_FRAME_BYTES * 2 + 3, "display: exact byte count");
    CHECK(delay_ms_total >= 1, "display: 1ms refresh delay");

    /* sleep: 0x02 power off, busy, 0x07 + 0xA5 deep sleep */
    seq_len = 0;
    CHECK(einkdrv_sleep(d) == 0, "sleep ok");
    CHECK(seq[0] == 0x02, "sleep: 0x02 power off");
    int has07 = 0, hasA5 = 0;
    for (int i = 0; i < seq_len; i++) {
        if (seq[i] == 0x07) has07 = 1;
        if (seq[i] == 0xA5) hasA5 = 1;
    }
    CHECK(has07 && hasA5, "sleep: 0x07 + 0xA5 deep sleep");

    /* fast init: 0x04, 0xE0 0x02, 0xE5 0x5A */
    seq_len = 0;
    CHECK(einkdrv_init_fast(d) == 0, "fast init ok");
    int hasE0 = 0, hasE5 = 0, has5A = 0;
    for (int i = 0; i < seq_len; i++) {
        if (seq[i] == 0xE0) hasE0 = 1;
        if (seq[i] == 0xE5) hasE5 = 1;
        if (seq[i] == 0x5A) has5A = 1;
    }
    CHECK(hasE0 && hasE5 && has5A, "fast init: 0xE0/0xE5/0x5A");

    /* LUT table sanity: 216 bytes, activation phase pattern
     * (alternating VPOS/VNEG visible as 0x01/0x05/0x02/0x03...) */
    CHECK(sizeof(einkdrv_lut_all) == EPD_LUT_SIZE, "LUT size 216");
    CHECK(einkdrv_lut_all[0] == 0x01, "LUT starts 0x01");

    einkdrv_free(d);
    if (failures == 0) {
        printf("test_einkdrv: ALL PASS\n");
        return 0;
    }
    printf("test_einkdrv: %d FAILURES\n", failures);
    return 1;
}
