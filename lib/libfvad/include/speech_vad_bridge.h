#ifndef STACKCHAN_SPEECH_VAD_BRIDGE_H
#define STACKCHAN_SPEECH_VAD_BRIDGE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SpeechVad SpeechVad;
SpeechVad* speech_vad_create(void);
void speech_vad_free(SpeechVad* state);
int speech_vad_reset(SpeechVad* state);
/* Fixed 320 samples / 16 kHz: -1=error, 0=no speech, 1=active, 2=hangover. */
int speech_vad_process(SpeechVad* state, const int16_t* samples);
#ifdef __cplusplus
}
#endif
#endif
