/* test_ai.c — unit tests for the ASR/TTS pipeline slots. */
#define _POSIX_C_SOURCE 200809L /* strdup */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ai/ai.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL: %s (line %d)\n", msg, __LINE__); \
        failures++; \
    } \
} while (0)

/* --- fake recognizer: echoes a fixed transcript --- */
static char *fake_recognize(const int16_t *pcm, size_t nframes,
                            uint32_t rate, void *user)
{
    (void)pcm; (void)nframes; (void)rate;
    return strdup((const char *)user);
}

/* --- fake synthesizer: emits a sine-ish ramp --- */
static int fake_synthesize(const char *text, int16_t **out,
                           uint32_t *rate, void *user)
{
    (void)text; (void)user;
    *rate = 22050;
    int n = 128;
    *out = malloc((size_t)n * sizeof(int16_t));
    for (int i = 0; i < n; i++) {
        (*out)[i] = (int16_t)i;
    }
    return n;
}

int main(void)
{
    /* ASR without recognizer: honest empty */
    asr_config cfg0 = { .rate = 16000, .recognize = NULL, .user = NULL };
    asr *a = asr_create(&cfg0);
    CHECK(a != NULL, "asr create");
    CHECK(asr_has_recognizer(a) == 0, "no recognizer");
    int16_t pcm[64];
    memset(pcm, 0, sizeof(pcm));
    CHECK(asr_feed(a, pcm, 64) == 0, "feed ok");
    CHECK(asr_transcribe(a) == NULL, "no fabrication without recognizer");
    asr_free(a);

    /* ASR with recognizer */
    asr_config cfg1 = { .rate = 16000, .recognize = fake_recognize,
                        .user = (void *)"hello device" };
    a = asr_create(&cfg1);
    CHECK(asr_has_recognizer(a) == 1, "has recognizer");
    CHECK(asr_feed(a, pcm, 64) == 0, "feed ok");
    char *trans = asr_transcribe(a);
    CHECK(trans != NULL && strcmp(trans, "hello device") == 0, "transcribe echoes");
    free(trans);
    asr_free(a);

    /* NULL safety */
    CHECK(asr_create(NULL) == NULL, "null cfg");
    CHECK(asr_feed(NULL, pcm, 8) == -1, "null feed");
    CHECK(asr_feed(a = asr_create(&cfg1), NULL, 8) == -1, "null pcm");
    asr_free(a);

    /* TTS without synthesizer: honest zero */
    tts_config tcfg0 = { .rate = 22050, .synthesize = NULL, .user = NULL };
    tts *t = tts_create(&tcfg0);
    CHECK(t != NULL, "tts create");
    CHECK(tts_has_synthesizer(t) == 0, "no synthesizer");
    int16_t *out = NULL;
    uint32_t rate = 0;
    CHECK(tts_speak(t, "hello", &out, &rate) == 0, "honest 0 frames");
    CHECK(out == NULL, "no fabricated audio");
    tts_free(t);

    /* TTS with synthesizer */
    tts_config tcfg1 = { .rate = 22050, .synthesize = fake_synthesize,
                         .user = NULL };
    t = tts_create(&tcfg1);
    CHECK(tts_has_synthesizer(t) == 1, "has synthesizer");
    out = NULL;
    int n = tts_speak(t, "hello", &out, &rate);
    CHECK(n == 128, "128 frames");
    CHECK(rate == 22050, "rate 22050");
    CHECK(out != NULL && out[0] == 0 && out[127] == 127, "pcm ramp");
    free(out);
    tts_free(t);

    /* NULL safety */
    CHECK(tts_create(NULL) == NULL, "null tts cfg");
    CHECK(tts_speak(NULL, "x", &out, &rate) == -1, "null tts");
    CHECK(tts_speak(t = tts_create(&tcfg1), NULL, &out, &rate) == -1,
          "null text");
    tts_free(t);
    CHECK(tts_speak(t = tts_create(&tcfg1), "x", NULL, &rate) == -1,
          "null out");
    tts_free(t);

    if (failures == 0) {
        printf("test_ai: ALL PASS\n");
        return 0;
    }
    printf("test_ai: %d FAILURES\n", failures);
    return 1;
}
