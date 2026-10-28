#pragma once
#include <speech_vad_bridge.h>
// PCM stays in the UI loop. The classifier stores filter/model state, not audio.
class SpeechDetector {
 public:
    ~SpeechDetector() { speech_vad_free(state_); }
    bool begin() { if (!state_) state_ = speech_vad_create(); return state_ != nullptr; }
    void reset() { if (state_) speech_vad_reset(state_); consecutive_ = 0; accepted_ = false; }
    int update(const int16_t* samples, bool aboveNoise) {
        int result = speech_vad_process(state_, samples);
        consecutive_ = result == 1 && aboveNoise ? consecutive_ + 1 : 0;
        accepted_ = consecutive_ >= 3;
        return result;
    }
    bool accepted() const { return accepted_; }
 private:
    SpeechVad* state_ = nullptr;
    unsigned consecutive_ = 0;
    bool accepted_ = false;
};
