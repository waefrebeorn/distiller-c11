#ifndef AUDIO_H
#define AUDIO_H

/* audio.h — audio control for the Distiller (C11, opaque API).
 *
 * Clean-room reimplementation of the v1 mic/speaker (PyAudio +
 * seeed-2mic-voicecard ALSA/WM8960) and the v2 sysfs control
 * paths. Format truth:
 *   - v1: 16-bit mono 44.1kHz capture/playback via ALSA
 *   - v2: sysfs controls under
 *     /sys/devices/platform/axi/.../1-0018/{input_gain,volume_level}
 * The C11 module abstracts both: an ALSA record/play API and a
 * sysfs gain/volume API. Pure POSIX, no third-party libs.
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- PCM record/play (ALSA via /dev/snd, raw ioctl) --- */

typedef struct audio_pcm audio_pcm;

typedef struct audio_format {
    uint32_t rate;      /* samples/sec, e.g. 44100 */
    uint16_t channels;  /* 1 = mono, 2 = stereo */
    uint16_t bits;      /* 16 */
} audio_format;

/* Open the default capture device. Returns NULL on failure. */
audio_pcm *audio_capture_open(const audio_format *fmt);

/* Open the default playback device. Returns NULL on failure. */
audio_pcm *audio_playback_open(const audio_format *fmt);

/* Read up to nframes frames; returns frames read, or -1. */
int audio_read(audio_pcm *p, int16_t *buf, size_t nframes);

/* Write nframes frames; returns frames written, or -1. */
int audio_write(audio_pcm *p, const int16_t *buf, size_t nframes);

void audio_close(audio_pcm *p);

/* --- WM8960 sysfs controls (v2 path) --- */

typedef struct audio_ctl audio_ctl;

/* Open the WM8960 sysfs control dir. base_path may be NULL to
 * auto-discover the 1-0018 device. */
audio_ctl *audio_ctl_open(const char *base_path);

/* Set input gain (0-100). Returns 0 on success. */
int audio_ctl_set_input_gain(audio_ctl *c, int percent);

/* Set volume (0-100). Returns 0 on success. */
int audio_ctl_set_volume(audio_ctl *c, int percent);

/* Get input gain / volume (0-100), or -1. */
int audio_ctl_get_input_gain(audio_ctl *c);
int audio_ctl_get_volume(audio_ctl *c);

void audio_ctl_close(audio_ctl *c);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_H */
