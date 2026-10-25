#pragma once
#include <Arduino.h>
#include "LocalWakeCore.h"
class LocalWake {
 public:
    bool begin();
    bool registered() const { return registered_; }
    bool detect(const int16_t* samples, bool voice);
    void resetDetection() { segment_.reset(); }
    void beginEnrollment();
    // 0=waiting, 1=accepted sample, 2=saved, -1=retry sample, -2=save error.
    int enroll(const int16_t* samples, bool voice);
    unsigned enrollmentCount() const { return enrolled_; }
    void cancelEnrollment();
 private:
    struct Store {
        uint32_t magic = 0x5231574c, version = 1, generation = 0; float threshold = 24;
        localwake::Features templates[3]; uint32_t crc = 0;
    };
    Store active_, pending_; localwake::Segmenter segment_;
    bool registered_ = false; unsigned enrolled_ = 0; int slot_ = -1;
    bool read(const char* path, Store& target);
    bool commit();
    void resetPending();
};
