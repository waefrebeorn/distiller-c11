/* sam.c — SAM (RP2040) UART protocol module (C11). */

#define _POSIX_C_SOURCE 200809L
#define _DEFAULT_SOURCE 1 /* CRTSCTS */

#include "sam.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

struct sam {
    int fd;
    int running;
    pthread_t thread;
    sam_callbacks cb;
    char line[256];
    size_t line_len;
};

uint8_t sam_crc8(const uint8_t *data, size_t len)
{
    /* CRC-8/MAXIM (Dallas 1-Wire): poly 0x31, init 0x00, xorout
     * 0x00, reflected (LSB-first). Reflected poly = 0x8C.
     * Check: crc8("123456789") == 0xA1. */
    uint8_t crc = 0;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc & 0x01) ? (uint8_t)((crc >> 1) ^ 0x8C) : (uint8_t)(crc >> 1);
        }
    }
    return crc;
}

static int set_baud(struct termios *tio, uint32_t baud)
{
    speed_t sp;
    switch (baud) {
    case 9600:   sp = B9600;   break;
    case 19200:  sp = B19200;  break;
    case 38400:  sp = B38400;  break;
    case 57600:  sp = B57600;  break;
    case 115200: sp = B115200; break;
    default:     return -1;
    }
    cfsetispeed(tio, sp);
    cfsetospeed(tio, sp);
    return 0;
}

sam *sam_open(const char *device, uint32_t baud,
              const sam_callbacks *cb)
{
    if (device == NULL || cb == NULL) {
        return NULL;
    }
    int fd = open(device, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (fd < 0) {
        return NULL;
    }
    struct termios tio;
    if (tcgetattr(fd, &tio) != 0) {
        close(fd);
        return NULL;
    }
    if (set_baud(&tio, baud) != 0) {
        close(fd);
        return NULL;
    }
    tio.c_cflag |= (CLOCAL | CREAD);
    tio.c_cflag &= ~(PARENB | CSTOPB | CSIZE | CRTSCTS);
    tio.c_cflag |= CS8;
    tio.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    tio.c_oflag &= ~OPOST;
    tio.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
    tio.c_cc[VMIN] = 0;
    tio.c_cc[VTIME] = 1;
    if (tcsetattr(fd, TCSANOW, &tio) != 0) {
        close(fd);
        return NULL;
    }

    sam *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        close(fd);
        return NULL;
    }
    s->fd = fd;
    s->cb = *cb;
    s->running = 0;
    return s;
}

static void handle_line(sam *s)
{
    if (s->line_len == 0) {
        return;
    }
    s->line[s->line_len] = '\0';
    /* digit line = button state bitmask */
    char *end = NULL;
    long v = strtol(s->line, &end, 10);
    if (end != s->line && *end == '\0' && v >= 0 && v <= 255) {
        if (s->cb.on_button) {
            s->cb.on_button(s, (uint8_t)v, s->cb.user);
        }
    } else {
        if (s->cb.on_info) {
            s->cb.on_info(s, s->line, s->cb.user);
        }
    }
    s->line_len = 0;
}

static void *monitor_loop(void *arg)
{
    sam *s = arg;
    char buf[64];
    while (s->running) {
        struct pollfd pfd = { .fd = s->fd, .events = POLLIN };
        int pr = poll(&pfd, 1, 200);
        if (pr > 0 && (pfd.revents & POLLIN)) {
            ssize_t n = read(s->fd, buf, sizeof(buf));
            if (n > 0) {
                for (ssize_t i = 0; i < n; ++i) {
                    if (buf[i] == '\n' || buf[i] == '\r') {
                        handle_line(s);
                    } else if (s->line_len < sizeof(s->line) - 1) {
                        s->line[s->line_len++] = (char)buf[i];
                    }
                }
            }
        }
    }
    return NULL;
}

int sam_start(sam *s)
{
    if (s == NULL || s->running) {
        return -1;
    }
    s->running = 1;
    return pthread_create(&s->thread, NULL, monitor_loop, s);
}

void sam_close(sam *s)
{
    if (s == NULL) {
        return;
    }
    if (s->running) {
        s->running = 0;
        pthread_join(s->thread, NULL);
    }
    close(s->fd);
    free(s);
}

int sam_send(sam *s, const char *fmt, ...)
{
    if (s == NULL || fmt == NULL) {
        return -1;
    }
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    if (n < 0 || (size_t)n >= sizeof(buf)) {
        return -1;
    }
    strncat(buf, "\n", sizeof(buf) - strlen(buf) - 1);
    ssize_t w = write(s->fd, buf, strlen(buf));
    return w > 0 ? 0 : -1;
}
