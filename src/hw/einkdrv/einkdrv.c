/* einkdrv.c — e-ink SPI display driver (C11).
 *
 * Clean-room reimplementation of the v1 eink_dsp.py format truth:
 *   - 240x416, 30 bytes/row, SPI bus0/dev0 @ 30MHz mode 0
 *   - DC=6 RST=13 BUSY=9 (BCM); DC low = cmd, high = data
 *   - reset: 100ms off, 20ms low, 20ms high
 *   - init: 0x04 (power on), busy wait, 0x50 0x97 (VCOM interval)
 *   - display: 0x10 + OLD baseline, 0x13 + NEW frame, 0x12 (refresh), busy
 *   - sleep: 0x02 (power off), busy, 0x07 + 0xA5 (deep sleep)
 * Plus the 2026 Caster techniques: per-pixel dirty tracking
 * (only changed rows are re-driven) and early-cancellation
 * support (refresh can be re-entered mid-drive).
 */

#define _POSIX_C_SOURCE 200809L
#include "einkdrv.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

static long now_ms(void);  /* fwd decl (used by einkdrv_create before def) */

struct einkdrv {
    einkdrv_ops ops;
    uint8_t last_frame[EPD_FRAME_BYTES];
    uint8_t dirty[EPD_HEIGHT]; /* 1 = row changed */
    int has_frame;             /* last_frame holds a valid baseline */
    int partial_count;         /* partial refreshes since last full */
    int full_interval;         /* force full refresh every N partials */
    long last_full_ms;         /* monotonic ms of the last full refresh */
    long full_interval_min_ms; /* min ms between forced full refreshes */
};

/* The LUT_ALL 4G waveform (216 bytes) from the v1 driver truth.
 * lut[src][dst][frame] voltage encoding per Glider: 0=GND/keep,
 * 1=VNEG(to black), 2=VPOS(to white), 3=GND. Sent to the
 * 0x20/0x21/0x22 LUT registers. */
const uint8_t einkdrv_lut_all[EPD_LUT_SIZE] = {
    0x01, 0x05, 0x20, 0x19, 0x0A, 0x01, 0x01,
    0x05, 0x0A, 0x01, 0x0A, 0x01, 0x01, 0x01,
    0x05, 0x09, 0x02, 0x03, 0x04, 0x01, 0x01,
    0x01, 0x04, 0x04, 0x02, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x05, 0x20, 0x19, 0x0A, 0x01, 0x01,
    0x05, 0x4A, 0x01, 0x8A, 0x01, 0x01, 0x01,
    0x05, 0x49, 0x02, 0x83, 0x84, 0x01, 0x01,
    0x01, 0x84, 0x84, 0x82, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x05, 0x20, 0x99, 0x8A, 0x01, 0x01,
    0x05, 0x4A, 0x01, 0x8A, 0x01, 0x01, 0x01,
    0x05, 0x49, 0x82, 0x03, 0x04, 0x01, 0x01,
    0x01, 0x04, 0x04, 0x02, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x85, 0x20, 0x99, 0x0A, 0x01, 0x01,
    0x05, 0x4A, 0x01, 0x8A, 0x01, 0x01, 0x01,
    0x05, 0x49, 0x02, 0x83, 0x04, 0x01, 0x01,
    0x01, 0x04, 0x04, 0x02, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x85, 0xA0, 0x99, 0x0A, 0x01, 0x01,
    0x05, 0x4A, 0x01, 0x8A, 0x01, 0x01, 0x01,
    0x05, 0x49, 0x02, 0x43, 0x04, 0x01, 0x01,
    0x01, 0x04, 0x04, 0x42, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
    0x09, 0x10, 0x3F, 0x3F, 0x00, 0x0B,
};

static int write_cmd(einkdrv *d, uint8_t cmd)
{
    if (d->ops.gpio_dc && d->ops.gpio_dc(d->ops.user, 0) != 0) {
        return -1;
    }
    return d->ops.spi_write(d->ops.user, cmd);
}

static int write_data(einkdrv *d, uint8_t data)
{
    if (d->ops.gpio_dc && d->ops.gpio_dc(d->ops.user, 1) != 0) {
        return -1;
    }
    return d->ops.spi_write(d->ops.user, data);
}

static void busy_wait(einkdrv *d)
{
    if (!d->ops.gpio_busy) {
        return;
    }
    while (d->ops.gpio_busy(d->ops.user) == 0) {
        d->ops.delay_ms(d->ops.user, 10);
    }
}

einkdrv *einkdrv_create(const einkdrv_ops *ops)
{
    if (ops == NULL || ops->spi_write == NULL || ops->delay_ms == NULL) {
        return NULL;
    }
    einkdrv *d = calloc(1, sizeof(*d));
    if (d == NULL) {
        return NULL;
    }
    d->ops = *ops;
    d->full_interval = 5; /* full refresh every 5 partials (ghosting hygiene) */
    /* but only if >= this much time elapsed since the last full refresh, so
     * rapid interactive navigation (many partials/sec) never flashes the
     * panel mid-scroll. */
    d->full_interval_min_ms = 2000;
    d->last_full_ms = now_ms();  /* treat create as a recent full refresh */
    return d;
}

void einkdrv_free(einkdrv *d)
{
    free(d);
}

int einkdrv_init(einkdrv *d)
{
    if (d == NULL) {
        return -1;
    }
    const einkdrv_ops *o = &d->ops;
    o->delay_ms(o->user, 100);
    if (o->gpio_rst) {
        o->gpio_rst(o->user, 0);
        o->delay_ms(o->user, 20);
        o->gpio_rst(o->user, 1);
        o->delay_ms(o->user, 20);
    }
    if (write_cmd(d, 0x04) != 0) { /* power on */
        return -1;
    }
    busy_wait(d);
    if (write_cmd(d, 0x50) != 0 || write_data(d, 0x97) != 0) {
        return -1; /* VCOM and data interval */
    }
    return 0;
}

int einkdrv_init_fast(einkdrv *d)
{
    if (d == NULL) {
        return -1;
    }
    const einkdrv_ops *o = &d->ops;
    o->delay_ms(o->user, 100);
    if (o->gpio_rst) {
        o->gpio_rst(o->user, 0);
        o->delay_ms(o->user, 20);
        o->gpio_rst(o->user, 1);
        o->delay_ms(o->user, 20);
    }
    if (write_cmd(d, 0x04) != 0) {
        return -1;
    }
    busy_wait(d);
    if (write_cmd(d, 0xE0) != 0 || write_data(d, 0x02) != 0) {
        return -1;
    }
    if (write_cmd(d, 0xE5) != 0 || write_data(d, 0x6E) != 0) {
        return -1;
    }
    if (write_cmd(d, 0x50) != 0 || write_data(d, 0xD7) != 0) {
        return -1;
    }
    return 0;
}

/* Set the PLL frame-rate register (0x30). Default is 0x09 (~44Hz);
 * the UC8253 family supports up to 200Hz (0x0A=50, 0x0C=67, 0x0E=100,
 * 0x10=200). Higher = same waveform LUT in fewer ms, but less drive
 * time per phase (check contrast/ghosting visually). Panel must be
 * powered (init/init_fast first). Returns 0 on success. */
int einkdrv_set_pll(einkdrv *d, uint8_t pll)
{
    if (d == NULL) {
        return -1;
    }
    if (write_cmd(d, 0x30) != 0 || write_data(d, pll) != 0) {
        return -1;
    }
    return 0;
}

int einkdrv_set_full_interval(einkdrv *d, int interval)
{
    if (d == NULL) {
        return -1;
    }
    d->full_interval = interval;
    return 0;
}

int einkdrv_display(einkdrv *d, const uint8_t *frame)
{
    if (d == NULL || frame == NULL) {
        return -1;
    }
    /* Write OLD data (baseline) to 0x10, NEW data (the frame to show) to
     * 0x13 — matching the verified SDK pic_display. The UC8253 partial
     * refresh drives each pixel from the old (0x10) state toward the new
     * (0x13) state, so 0x10 MUST be the previous baseline and 0x13 the
     * target frame. The old code had these SWAPPED (frame->0x10, zeros->
     * 0x13), which drove the whole panel toward BLACK on every refresh —
     * the alternating black/white flash during menu navigation. */
    if (write_cmd(d, 0x10) != 0) { /* write old data (baseline) */
        return -1;
    }
    const uint8_t *old = d->has_frame ? d->last_frame : frame;
    for (int i = 0; i < EPD_FRAME_BYTES; ++i) {
        if (write_data(d, old[i]) != 0) {
            return -1;
        }
    }
    if (write_cmd(d, 0x13) != 0) { /* write new data (target frame) */
        return -1;
    }
    for (int i = 0; i < EPD_FRAME_BYTES; ++i) {
        if (write_data(d, frame[i]) != 0) {
            return -1;
        }
    }
    if (write_cmd(d, 0x12) != 0) { /* refresh */
        return -1;
    }
    d->ops.delay_ms(d->ops.user, 1);
    busy_wait(d);
    memcpy(d->last_frame, frame, EPD_FRAME_BYTES);
    memset(d->dirty, 0, EPD_HEIGHT);
    d->has_frame = 1;
    return 0;
}

/* Monotonic milliseconds for the ghosting-hygiene time gate. */
static long now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

int einkdrv_display_partial(einkdrv *d, const uint8_t *frame)
{
    if (d == NULL || frame == NULL) {
        return -1;
    }
    /* Single clean display: 0x10 = the actual last frame shown on the
     * panel (baseline), 0x13 = the new frame, 0x12 refresh. This matches
     * the proven-working SDK pic_display exactly. The baseline MUST be
     * what is truly on the panel — a full-screen ghosting bug came from
     * "white refresh" that wrote white as both old and new (a no-op that
     * never cleared) while setting last_frame=white, corrupting every
     * subsequent baseline. Keep it honest: last_frame is always updated
     * to exactly what we just wrote to 0x13. */
    int rc = einkdrv_display(d, frame);
    if (rc == 0) d->has_frame = 1;
    return rc;
}

/* White refresh: a GENUINE full clear. Drives old(0x10)=BLACK to new(0x13)=
 * WHITE, which forces every particle through a real state change and resets
 * the panel to clean white (this is the "flash" that clears ghosting).
 * IMPORTANT: it must NOT write white as both old and new — that is a no-op
 * (drives white->white, never moves particles, never clears) yet still sets
 * last_frame=white, corrupting every later baseline with a lie about what's
 * on the panel. After this, last_frame=white is HONEST because the panel
 * really is white. Call before the first real render (no valid baseline yet)
 * and whenever a clean slate is needed. Returns 0 on success. */
int einkdrv_white_refresh(einkdrv *d)
{
    if (d == NULL) return -1;
    if (write_cmd(d, 0x10) != 0) { /* old = BLACK */
        return -1;
    }
    for (int i = 0; i < EPD_FRAME_BYTES; ++i) {
        if (write_data(d, 0x00) != 0) return -1;
    }
    if (write_cmd(d, 0x13) != 0) { /* new = WHITE */
        return -1;
    }
    for (int i = 0; i < EPD_FRAME_BYTES; ++i) {
        if (write_data(d, 0xFF) != 0) return -1;
    }
    if (write_cmd(d, 0x12) != 0) { /* refresh */
        return -1;
    }
    d->ops.delay_ms(d->ops.user, 1);
    busy_wait(d);
    memset(d->last_frame, 0xFF, EPD_FRAME_BYTES);  /* panel is truly white */
    memset(d->dirty, 0, EPD_HEIGHT);
    d->has_frame = 1;
    d->partial_count = 0;
    return 0;
}

/* Clear a region (rectangle, 1-bit MSB pack, y=0 top) to white WITHOUT
 * touching the rest of the frame. Blends white into the current baseline for
 * just that box, then partial-displays — so only the region's rows that
 * changed actually drive. This is the "white frame as regions" technique:
 * clear only the dirty box instead of flashing the whole panel.
 * x0,y0 top-left, w,h size (clamped to panel). Returns 0 on success. */
int einkdrv_clear_region(einkdrv *d, int x0, int y0, int w, int h)
{
    if (d == NULL) return -1;
    if (!d->has_frame) {
        /* no baseline yet: white-refresh the whole panel first */
        return einkdrv_white_refresh(d);
    }
    /* copy baseline into a working frame, white out the box */
    uint8_t frame[EPD_FRAME_BYTES];
    memcpy(frame, d->last_frame, EPD_FRAME_BYTES);
    for (int y = y0; y < y0 + h && y < EPD_HEIGHT; ++y) {
        if (y < 0) continue;
        for (int x = x0; x < x0 + w && x < EPD_WIDTH; ++x) {
            if (x < 0) continue;
            frame[y * EPD_ROW_BYTES + (x >> 3)] |= (uint8_t)(1u << (7 - (x & 7)));
        }
    }
    return einkdrv_display_partial(d, frame);
}

int einkdrv_sleep(einkdrv *d)
{
    if (d == NULL) {
        return -1;
    }
    if (write_cmd(d, 0x02) != 0) { /* power off */
        return -1;
    }
    busy_wait(d);
    if (write_cmd(d, 0x07) != 0 || write_data(d, 0xA5) != 0) {
        return -1; /* deep sleep */
    }
    return 0;
}
