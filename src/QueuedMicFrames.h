#pragma once
#include <stdint.h>
#include <string.h>
// M5Unified 0.1.17 record() blocks while both recording slots are occupied.
// Enqueuing a third buffer is a completion barrier for the oldest buffer.
// isRecording() alone cannot observe a queued request before the mic task starts.
class QueuedMicFrames {
 public:
    void reset() { primed_ = false; oldest_ = 0; } // only after Mic.end() joins its worker
    template<class Microphone> bool read(Microphone& microphone, int16_t* out) {
        if (!primed_) {
            if (!microphone.record(buffers_[0], 320, 16000) || !microphone.record(buffers_[1], 320, 16000)) return false;
            primed_ = true;
        }
        unsigned next = (oldest_ + 2) % 3;
        if (!microphone.record(buffers_[next], 320, 16000)) return false;
        memcpy(out, buffers_[oldest_], sizeof(buffers_[oldest_]));
        oldest_ = (oldest_ + 1) % 3;
        return true;
    }
 private:
    int16_t buffers_[3][320] = {};
    unsigned oldest_ = 0;
    bool primed_ = false;
};
