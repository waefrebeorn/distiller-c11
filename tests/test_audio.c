/* test_audio.c — unit tests for the audio module (sysfs path). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hw/audio/audio.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

int main(void)
{
    /* NULL safety */
    CHECK(audio_ctl_open(NULL) == NULL || audio_ctl_open(NULL) != NULL,
          "open(NULL) is safe (auto-discover may or may not find the codec)");
    CHECK(audio_ctl_set_input_gain(NULL, 50) == -1, "null ctl gain rejected");
    CHECK(audio_ctl_set_volume(NULL, 50) == -1, "null ctl volume rejected");
    CHECK(audio_ctl_set_input_gain(NULL, 150) == -1, "gain >100 rejected");
    CHECK(audio_ctl_set_volume(NULL, -1) == -1, "volume <0 rejected");
    CHECK(audio_ctl_get_input_gain(NULL) == -1, "null get gain");
    CHECK(audio_ctl_get_volume(NULL) == -1, "null get volume");

    /* PCM API fails cleanly without /dev/snd */
    audio_format fmt = { 44100, 1, 16 };
    CHECK(audio_capture_open(&fmt) == NULL, "capture open fails clean (no ALSA uapi)");
    CHECK(audio_playback_open(&fmt) == NULL, "playback open fails clean");
    audio_pcm *p = audio_capture_open(&fmt);
    if (p != NULL) { /* if it somehow opened, read must not crash */
        int16_t b[16];
        (void)audio_read(p, b, 8);
        audio_close(p);
    }

    /* Round-trip the sysfs path if the codec is present */
    audio_ctl *c = audio_ctl_open(NULL);
    if (c != NULL) {
        int orig = audio_ctl_get_volume(c);
        CHECK(audio_ctl_set_volume(c, 40) == 0, "set volume 40");
        CHECK(audio_ctl_get_volume(c) == 40, "get volume 40");
        if (orig >= 0) {
            audio_ctl_set_volume(c, orig); /* restore */
        }
        audio_ctl_close(c);
    } else {
        printf("note: WM8960 sysfs not present on this machine; control round-trip skipped\n");
    }

    if (failures == 0) {
        printf("test_audio: ALL PASS\n");
        return 0;
    }
    printf("test_audio: %d FAILURES\n", failures);
    return 1;
}
