#ifndef SAM_H
#define SAM_H

/* sam.h — SAM (RP2040) UART protocol module (C11, opaque API).
 *
 * Clean-room reimplementation of the v1 driver (distiller/drivers/
 * sam.py) format truth:
 *   - UART 9600 baud, line-based protocol
 *   - digit lines = button state bitmask (1=UP 2=DOWN 4=SELECT 8=SHUTDOWN)
 *   - non-digit lines = SAM info messages
 * The v2 moved to sysfs LEDs; this module keeps the v1 button
 * protocol AND adds a typed command framing for v2-style control.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Button bitmask values (v1 protocol truth). */
enum sam_button {
    SAM_BTN_UP     = 1,
    SAM_BTN_DOWN   = 2,
    SAM_BTN_SELECT = 4,
    SAM_BTN_SHUTDOWN = 8,
};

/* CRC8 result for a message (poly 0x07, init 0x00 — Dallas/CRC-8). */
uint8_t sam_crc8(const uint8_t *data, size_t len);

typedef struct sam sam;

/* Button state callback (bitmask of sam_button values). */
typedef void (*sam_button_fn)(sam *s, uint8_t state, void *user);

/* Info message callback (non-digit SAM lines). */
typedef void (*sam_info_fn)(sam *s, const char *line, void *user);

typedef struct sam_callbacks {
    sam_button_fn on_button;
    sam_info_fn   on_info;
    void         *user;
} sam_callbacks;

/* Open the SAM UART device (e.g. /dev/ttyAMA3). Returns NULL on
 * failure. Does NOT start the monitor thread. */
sam *sam_open(const char *device, uint32_t baud,
              const sam_callbacks *cb);

/* Start the background monitor thread (poll loop). 0 on success. */
int sam_start(sam *s);

/* Stop the monitor thread and close the UART. */
void sam_close(sam *s);

/* Send a typed command line to SAM (v2-style control). */
int sam_send(sam *s, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif /* SAM_H */
