#ifndef EINKDRV_H
#define EINKDRV_H

/* einkdrv.h — e-ink SPI display driver (C11, opaque API).
 *
 * Clean-room reimplementation of the v1 driver
 * (distiller/drivers/eink_dsp.py) format truth:
 *   - 240x416 EPD, 30 bytes/row, MSB-first packing
 *   - SPI bus 0/0 @ 30MHz, mode 0; DC=6 RST=13 BUSY=9 (BCM)
 *   - DC low = command, DC high = data
 *   - reset: 100ms off, 20ms low, 20ms high
 *   - init: 0x04 power on, busy wait, 0x50/0x97 VCOM interval
 *   - display: 0x10 image bytes, 0x13 zeros, 0x12 refresh, busy
 *   - sleep: 0x02 power off, busy, 0x07 deep sleep + 0xA5
 *   - LUT: 0x20/0x21/0x22 (LUT_ALL 4G waveform, 200 bytes)
 *
 * Transport is injected (ops vtable) so unit tests can record the
 * exact byte stream — the SLERM oracle check against v1 truth.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EPD_WIDTH  240
#define EPD_HEIGHT 416
#define EPD_ROW_BYTES ((EPD_WIDTH + 7) / 8)   /* 30 */
#define EPD_FRAME_BYTES (EPD_ROW_BYTES * EPD_HEIGHT) /* 12480 */
#define EPD_LUT_SIZE 216

typedef struct einkdrv einkdrv;

/* Transport ops — injectable for tests (SPI + GPIO + delay). */
typedef struct einkdrv_ops {
    /* write one byte on the SPI bus; returns 0 on success */
    int (*spi_write)(void *user, uint8_t byte);
    /* set DC line: 0 = command, 1 = data */
    int (*gpio_dc)(void *user, int level);
    /* set RST line */
    int (*gpio_rst)(void *user, int level);
    /* read BUSY line; returns 0 = busy, 1 = ready */
    int (*gpio_busy)(void *user);
    /* sleep for ms */
    void (*delay_ms)(void *user, uint32_t ms);
    void *user;
} einkdrv_ops;

/* Create driver with injected transport. */
einkdrv *einkdrv_create(const einkdrv_ops *ops);

/* Full init sequence (reset + power on + VCOM). */
int einkdrv_init(einkdrv *d);

/* Fast init (0xE0/0xE5 for partial refresh). */
int einkdrv_init_fast(einkdrv *d);

/* Set the PLL frame-rate register (0x30). Default 0x09 (~44Hz); up to
 * 200Hz supported (0x0A=50, 0x0C=67, 0x0E=100, 0x10=200). Faster frame
 * rate = same waveform in fewer ms, at the cost of per-phase drive
 * time (verify contrast/ghosting). Panel must be powered first. */
int einkdrv_set_pll(einkdrv *d, uint8_t pll);

/* Set the full-refresh interval for the ghosting-hygiene path: force a
 * full refresh every N partial refreshes (default 5). 0 disables. */
int einkdrv_set_full_interval(einkdrv *d, int interval);

/* Display one packed frame (EPD_FRAME_BYTES, MSB-first). Assumes the
 * panel is already powered (init or init_fast called at least once) —
 * no per-frame reset. This is the persistent-session hot path. */
int einkdrv_display(einkdrv *d, const uint8_t *frame);

/* Partial display: only re-drive rows whose packed bytes changed vs the
 * last frame (per-pixel region tracking, Caster/Modos 2026 technique).
 * frame must be EPD_FRAME_BYTES. For a sparse change (menu, text) this
 * writes only the changed row bytes + sends the refresh; the panel
 * refresh time is unavoidable but the SPI payload shrinks and — on
 * controllers that support windowed refresh — the drive too. Returns 0
 * on success. Falls back to full display if no previous frame. */
int einkdrv_display_partial(einkdrv *d, const uint8_t *frame);

/* White refresh: drive the whole panel to white and reset the partial
 * baseline (erases ghosting, un-sticks a black panel). Call on boot/app-open
 * and after navigation transitions. Returns 0 on success. */
int einkdrv_white_refresh(einkdrv *d);

/* Clear a region (rectangle) to white without touching the rest of the
 * frame (the "white frame as regions" technique). x0,y0 top-left, w,h size.
 * Returns 0 on success. */
int einkdrv_clear_region(einkdrv *d, int x0, int y0, int w, int h);

/* Power off + deep sleep. */
int einkdrv_sleep(einkdrv *d);

/* Free the driver. */
void einkdrv_free(einkdrv *d);

/* The LUT_ALL waveform (format truth, exposed for tests). */
extern const uint8_t einkdrv_lut_all[EPD_LUT_SIZE];

#ifdef __cplusplus
}
#endif

#endif /* EINKDRV_H */
