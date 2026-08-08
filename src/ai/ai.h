#ifndef AI_H
#define AI_H

/* ai.h — on-device speech AI pipeline slots (C11, opaque API).
 *
 * The Distiller's speech path: mic -> ASR (speech-to-text) and
 * TTS (text-to-speech) -> speaker. Per the SLERM neural-model
 * rule, the deterministic pipeline is C11 and the learned step
 * (the recognizer / synthesizer) is a pluggable callback:
 *   - the ASR slot: audio frames in, transcribed text out via a
 *     recognizer callback (parakeet/whisper drop in later)
 *   - the TTS slot: text in, PCM frames out via a synthesizer
 *     callback (piper drops in later)
 * With no recognizer/synthesizer installed the API still works
 * and reports honestly (no fabricated transcripts).
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- ASR (speech to text) --- */

typedef struct asr asr;

/* Recognizer callback: given 16-bit mono PCM at the configured
 * rate, produce a NUL-terminated transcript (malloc'd, caller
 * frees) or NULL if no recognizer is installed. */
typedef char *(*asr_recognize_fn)(const int16_t *pcm, size_t nframes,
                                  uint32_t rate, void *user);

typedef struct asr_config {
    uint32_t rate;          /* expected sample rate, e.g. 16000 */
    asr_recognize_fn recognize;
    void *user;
} asr_config;

asr *asr_create(const asr_config *cfg);
void asr_free(asr *a);

/* Feed PCM frames into the ASR slot. Returns 0 on success. */
int asr_feed(asr *a, const int16_t *pcm, size_t nframes);

/* Transcribe what has been fed; returns malloc'd text or NULL
 * (no recognizer / nothing fed / recognition failed). */
char *asr_transcribe(asr *a);

/* 1 if a recognizer is installed, 0 otherwise. */
int asr_has_recognizer(const asr *a);

/* --- TTS (text to speech) --- */

typedef struct tts tts;

/* Synthesizer callback: given text, produce 16-bit mono PCM
 * frames; returns frame count or -1; fills *out (malloc'd).
 * NULL = no synthesizer installed. */
typedef int (*tts_synthesize_fn)(const char *text, int16_t **out,
                                 uint32_t *rate, void *user);

typedef struct tts_config {
    uint32_t rate;          /* output sample rate, e.g. 22050 */
    tts_synthesize_fn synthesize;
    void *user;
} tts_config;

tts *tts_create(const tts_config *cfg);
void tts_free(tts *t);

/* Synthesize text to PCM. Returns frame count (>0), 0 if no
 * synthesizer, -1 on error. *out is malloc'd (caller frees). */
int tts_speak(tts *t, const char *text, int16_t **out, uint32_t *rate);

/* 1 if a synthesizer is installed, 0 otherwise. */
int tts_has_synthesizer(const tts *t);

#ifdef __cplusplus
}
#endif

#endif /* AI_H */
