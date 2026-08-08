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

/* Display one packed frame (EPD_FRAME_BYTES, MSB-first). */
int einkdrv_display(einkdrv *d, const uint8_t *frame);

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
