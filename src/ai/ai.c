/* ai.c — on-device speech AI pipeline slots (C11). */

#include "ai.h"

#include <stdlib.h>
#include <string.h>

/* --- ASR --- */

struct asr {
    asr_config cfg;
    int16_t *buf;
    size_t nframes;
    size_t cap;
};

asr *asr_create(const asr_config *cfg)
{
    if (cfg == NULL || cfg->rate == 0) {
        return NULL;
    }
    asr *a = calloc(1, sizeof(*a));
    if (a == NULL) {
        return NULL;
    }
    a->cfg = *cfg;
    return a;
}

void asr_free(asr *a)
{
    if (a == NULL) {
        return;
    }
    free(a->buf);
    free(a);
}

int asr_feed(asr *a, const int16_t *pcm, size_t nframes)
{
    if (a == NULL || pcm == NULL || nframes == 0) {
        return -1;
    }
    size_t need = a->nframes + nframes;
    if (need > a->cap) {
        size_t ncap = a->cap ? a->cap * 2 : 8192;
        while (ncap < need) {
            ncap *= 2;
        }
        int16_t *nb = realloc(a->buf, ncap * sizeof(int16_t));
        if (nb == NULL) {
            return -1;
        }
        a->buf = nb;
        a->cap = ncap;
    }
    memcpy(a->buf + a->nframes, pcm, nframes * sizeof(int16_t));
    a->nframes += nframes;
    return 0;
}

char *asr_transcribe(asr *a)
{
    if (a == NULL || a->cfg.recognize == NULL || a->nframes == 0) {
        return NULL;
    }
    return a->cfg.recognize(a->buf, a->nframes, a->cfg.rate, a->cfg.user);
}

int asr_has_recognizer(const asr *a)
{
    return (a != NULL && a->cfg.recognize != NULL) ? 1 : 0;
}

/* --- TTS --- */

struct tts {
    tts_config cfg;
};

tts *tts_create(const tts_config *cfg)
{
    if (cfg == NULL || cfg->rate == 0) {
        return NULL;
    }
    tts *t = calloc(1, sizeof(*t));
    if (t == NULL) {
        return NULL;
    }
    t->cfg = *cfg;
    return t;
}

void tts_free(tts *t)
{
    free(t);
}

int tts_speak(tts *t, const char *text, int16_t **out, uint32_t *rate)
{
    if (t == NULL || text == NULL || out == NULL) {
        return -1;
    }
    if (t->cfg.synthesize == NULL) {
        *out = NULL;
        return 0; /* no synthesizer: honest empty */
    }
    uint32_t r = t->cfg.rate;
    int n = t->cfg.synthesize(text, out, &r, t->cfg.user);
    if (rate != NULL) {
        *rate = r;
    }
    return n;
}

int tts_has_synthesizer(const tts *t)
{
    return (t != NULL && t->cfg.synthesize != NULL) ? 1 : 0;
}
