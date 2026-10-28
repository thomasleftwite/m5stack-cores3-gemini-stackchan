#pragma once
#include <stdint.h>
#include <string.h>
// M5Unified 0.1.17 record() blocks while both recording slots are occupied.
// Enqueuing a third buffer is a completion barrier for the oldest buffer.
// Reserve 80 ms per slot so a ~46 ms face transfer cannot exhaust both slots.
// Return ordered 20 ms slices; MFCC/VAD and the fixed 5-second count stay unchanged.
// isRecording() alone cannot observe a queued request before the mic task starts.
class QueuedMicFrames {
 public:
    void reset() { primed_ = false; oldest_ = 0; offset_ = BlockSamples; } // only after Mic.end() joins its worker
    template<class Microphone> bool prime(Microphone& microphone) {
        if (primed_) return true;
        if (!microphone.record(buffers_[0], BlockSamples, 16000) || !microphone.record(buffers_[1], BlockSamples, 16000)) return false;
        primed_ = true; return true;
    }
    template<class Microphone> bool read(Microphone& microphone, int16_t* out) {
        if (!prime(microphone)) return false;
        if (offset_ == BlockSamples) {
            unsigned next = (oldest_ + 2) % 3;
            if (!microphone.record(buffers_[next], BlockSamples, 16000)) return false;
            complete_ = oldest_; oldest_ = (oldest_ + 1) % 3; offset_ = 0;
        }
        memcpy(out, buffers_[complete_] + offset_, 320 * sizeof(int16_t));
        offset_ += 320;
        return true;
    }
 private:
    static constexpr unsigned BlockSamples = 1280;
    int16_t buffers_[3][BlockSamples] = {};
    unsigned oldest_ = 0, complete_ = 0, offset_ = BlockSamples;
    bool primed_ = false;
};
