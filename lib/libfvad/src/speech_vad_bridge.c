/* Stackchan adapter to pinned libfvad core. Upstream sources are unchanged.
 * Distinguish real speech decisions from WebRTC's hangover extension: the
 * existing segmenter already tolerates 400 ms gaps, so hangover must not
 * turn a short click into 200 ms of enrollment evidence. */
#include "../include/speech_vad_bridge.h"
#include "vad/vad_core.h"
#include <stdlib.h>
struct SpeechVad { VadInstT core; };
int speech_vad_reset(SpeechVad* state) {
    if (!state || WebRtcVad_InitCore(&state->core)) return -1;
    return WebRtcVad_set_mode_core(&state->core, 3);
}
SpeechVad* speech_vad_create(void) {
    SpeechVad* state = malloc(sizeof(*state));
    if (state && speech_vad_reset(state)) { free(state); return NULL; }
    return state;
}
void speech_vad_free(SpeechVad* state) { free(state); }
int speech_vad_process(SpeechVad* state, const int16_t* samples) {
    if (!state || !samples) return -1;
    int result = WebRtcVad_CalcVad16khz(&state->core, samples, 320);
    return result > 1 ? 2 : result;
}
