/* einkdrv_pi.c — real CM4 transport for einkdrv (spidev + gpiochip ioctls).
 *
 * The C11 driver core (einkdrv.c) is transport-injected; this is the
 * production ops vtable for the Distiller One:
 *   - SPI: /dev/spidev0.0 @ 30 MHz, mode 0, MSB-first, byte writes
 *   - DC   = BCM 6  (command low / data high)
 *   - RST  = BCM 13
 *   - BUSY = BCM 9  (0 = busy)
 *
 * GPIO uses the kernel char-device ABI directly (/dev/gpiochip0 ioctls —
 * the same path RPi.GPIO uses). No libgpiod, no sysfs export: works on
 * any modern kernel where the old sysfs GPIO export is disabled.
 *
 * Written as a small standalone .c that links against libdc11.a and
 * exposes a C11->Python ctypes ABI (see bindings/eink_c11.py).
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <linux/gpio.h>
#include <linux/spi/spidev.h>

#include "hw/einkdrv/einkdrv.h"
#include "eink/eink.h"

/* ---- GPIO via /dev/gpiochip0 line handles (kernel char-device ABI) ---- */

typedef struct {
    int dc_fd;    /* line handle fds */
    int rst_fd;
    int busy_fd;
} gpio_state;

static int gpio_open_line(int chip_fd, unsigned offset, int is_output)
{
    struct gpiohandle_request req;
    memset(&req, 0, sizeof(req));
    req.lineoffsets[0] = offset;
    req.flags = is_output ? GPIOHANDLE_REQUEST_OUTPUT : GPIOHANDLE_REQUEST_INPUT;
    req.default_values[0] = 1; /* RST idle high */
    req.lines = 1;
    strncpy(req.consumer_label, "einkdrv", sizeof(req.consumer_label) - 1);
    if (ioctl(chip_fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
        return -1;
    }
    return req.fd;
}

static int gpio_write_line(int fd, int level)
{
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    data.values[0] = level ? 1 : 0;
    return ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data);
}

static int gpio_read_line(int fd)
{
    struct gpiohandle_data data;
    memset(&data, 0, sizeof(data));
    if (ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
        return -1;
    }
    return data.values[0];
}

/* ---- the ops vtable ---- */

typedef struct {
    int spi_fd;
    gpio_state gpio;
} pi_transport;

static int t_spi_write(void *u, uint8_t b) {
    pi_transport *t = (pi_transport *)u;
    struct spi_ioc_transfer x = {0};
    x.tx_buf = (unsigned long)&b;
    x.len = 1;
    x.speed_hz = 30000000;
    x.bits_per_word = 8;
    return ioctl(t->spi_fd, SPI_IOC_MESSAGE(1), &x) < 0 ? -1 : 0;
}

static int t_gpio_dc(void *u, int level) {
    pi_transport *t = (pi_transport *)u;
    return gpio_write_line(t->gpio.dc_fd, level);
}

static int t_gpio_rst(void *u, int level) {
    pi_transport *t = (pi_transport *)u;
    return gpio_write_line(t->gpio.rst_fd, level);
}

static int t_gpio_busy(void *u) {
    pi_transport *t = (pi_transport *)u;
    return gpio_read_line(t->gpio.busy_fd);
}

static void t_delay_ms(void *u, uint32_t ms) {
    (void)u;
    struct timespec ts = { .tv_sec = ms / 1000, .tv_nsec = (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

static pi_transport g_transport;

static einkdrv_ops make_ops(pi_transport *t) {
    einkdrv_ops o;
    memset(&o, 0, sizeof(o));
    o.spi_write = t_spi_write;
    o.gpio_dc = t_gpio_dc;
    o.gpio_rst = t_gpio_rst;
    o.gpio_busy = t_gpio_busy;
    o.delay_ms = t_delay_ms;
    o.user = t;
    return o;
}

/* ---- C ABI for ctypes (symbols exported from this TU) ---- */

static einkdrv *g_drv = NULL;

/* einkdrv_pi_open(): open gpiochip0 + spidev, create driver.
 * Returns 0 on success, -1 on failure (stderr printed). */
int einkdrv_pi_open(void) {
    const unsigned DC = 6, RST = 13, BUSY = 9;

    int chip = open("/dev/gpiochip0", O_RDONLY);
    if (chip < 0) {
        fprintf(stderr, "einkdrv_pi_open: gpiochip0: %s\n", strerror(errno));
        return -1;
    }
    g_transport.gpio.dc_fd = gpio_open_line(chip, DC, 1);
    g_transport.gpio.rst_fd = gpio_open_line(chip, RST, 1);
    g_transport.gpio.busy_fd = gpio_open_line(chip, BUSY, 0);
    close(chip);
    if (g_transport.gpio.dc_fd < 0 || g_transport.gpio.rst_fd < 0 ||
        g_transport.gpio.busy_fd < 0) {
        fprintf(stderr, "einkdrv_pi_open: gpio line open failed (%s)\n",
                strerror(errno));
        return -1;
    }

    int fd = open("/dev/spidev0.0", O_RDWR);
    if (fd < 0) {
        fprintf(stderr, "einkdrv_pi_open: spidev open: %s\n", strerror(errno));
        return -1;
    }
    uint8_t mode = SPI_MODE_0;
    uint8_t bits = 8;
    uint32_t speed = 30000000;
    ioctl(fd, SPI_IOC_WR_MODE, &mode);
    ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
    ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);

    g_transport.spi_fd = fd;

    einkdrv_ops ops = make_ops(&g_transport);
    g_drv = einkdrv_create(&ops);
    if (!g_drv) {
        fprintf(stderr, "einkdrv_pi_open: create failed\n");
        close(fd);
        return -1;
    }
    return 0;
}

/* einkdrv_pi_close(): release the gpio line handles + spidev so a child
 * process can open the panel (used during app hand-off). Idempotent. */
int einkdrv_pi_close(void) {
    if (g_drv) {
        einkdrv_free(g_drv);
        g_drv = NULL;
    }
    if (g_transport.gpio.dc_fd > 0) { close(g_transport.gpio.dc_fd); g_transport.gpio.dc_fd = -1; }
    if (g_transport.gpio.rst_fd > 0) { close(g_transport.gpio.rst_fd); g_transport.gpio.rst_fd = -1; }
    if (g_transport.gpio.busy_fd > 0) { close(g_transport.gpio.busy_fd); g_transport.gpio.busy_fd = -1; }
    if (g_transport.spi_fd > 0) { close(g_transport.spi_fd); g_transport.spi_fd = -1; }
    return 0;
}

int einkdrv_pi_init(void) { return g_drv ? einkdrv_init(g_drv) : -1; }
int einkdrv_pi_init_fast(void) { return g_drv ? einkdrv_init_fast(g_drv) : -1; }

/* einkdrv_pi_set_pll(): override the PLL frame-rate register (0x30).
 * Default from the v1 LUT is 0x09 (~44Hz). The UC8253 family supports
 * up to 200Hz (e.g. 0x0A=50, 0x0C=67, 0x0E=100, 0x10=200). A higher
 * frame rate runs the same waveform LUT in fewer ms — but the
 * electrophoretic particles get less drive time per phase, so contrast
 * and ghosting must be checked visually. Returns 0 on success. */
int einkdrv_pi_set_pll(uint8_t pll) {
    return g_drv ? einkdrv_set_pll(g_drv, pll) : -1;
}

/* Display one packed frame. Persistent-session variant: assumes the
 * panel is already powered (init called once); skips the per-frame
 * reset+power-on (181ms) and goes straight to data+refresh. */
int einkdrv_pi_display(const uint8_t *frame) {
    return g_drv ? einkdrv_display(g_drv, frame) : -1;
}

/* Dirty-row partial display (no-op on identical frames). */
int einkdrv_pi_display_partial(const uint8_t *frame) {
    return g_drv ? einkdrv_display_partial(g_drv, frame) : -1;
}

int einkdrv_pi_set_full_interval(int interval) {
    return g_drv ? einkdrv_set_full_interval(g_drv, interval) : -1;
}

/* White refresh + region-clear C ABI (ctypes). */
int einkdrv_pi_white_refresh(void) {
    return g_drv ? einkdrv_white_refresh(g_drv) : -1;
}
int einkdrv_pi_clear_region(int x0, int y0, int w, int h) {
    return g_drv ? einkdrv_clear_region(g_drv, x0, y0, w, h) : -1;
}

/* C11 render pipeline exports (eink.c): gray HxW uint8 -> packed 1-bit.
 * Mode: 0=bayer8 1=floyd-steinberg 2=threshold. Returns bytes or -1. */
eink_frame *eink_pi_create(size_t w, size_t h) { return eink_create(w, h); }
void eink_pi_free(eink_frame *f) { eink_free(f); }
size_t eink_pi_packed_size(const eink_frame *f) { return eink_packed_size(f); }

long eink_pi_render_gray(const eink_frame *f, const uint8_t *gray, uint8_t *out)
{
    return eink_render_gray(f, gray, out);
}

long eink_pi_render_rgb(const eink_frame *f, const uint8_t *rgb, uint8_t *out)
{
    return eink_render_rgb(f, rgb, out);
}

/* Set dither mode on the frame context (0/1/2). */
void eink_pi_set_mode(eink_frame *f, int mode)
{
    if (f) eink_set_dither_mode(f, (enum eink_dither)mode);
}

int einkdrv_pi_sleep(void) { return g_drv ? einkdrv_sleep(g_drv) : -1; }
