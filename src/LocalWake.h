#pragma once
#include <Arduino.h>
#include "LocalWakeCore.h"
#include "RollingWakeWindow.h"
class LocalWake {
 public:
    struct Diagnostics {
        uint32_t frames = 0, voice = 0, complete = 0, tooShort = 0, clipped = 0, tooLong = 0, matched = 0;
        unsigned lastMatches = 0;
    };
    const Diagnostics& diagnostics() const { return diagnostics_; }
    bool begin();
    bool registered() const { return registered_; }
    bool detect(const int16_t* samples, bool voice, uint32_t backgroundEpoch = 0);
    void resetDetection() { segment_.reset(); energySegment_.reset(); rolling_.reset(); }
    // E02 shadow only: -1=waiting, 0..3=matching templates; never activates wake.
    int probeEnergy(const int16_t* samples, bool aboveBackground, uint32_t backgroundEpoch);
    int probeWindow(const int16_t* samples);
    bool detectWindow(const int16_t* samples);
    void beginEnrollment();
    // 0=waiting, 1=accepted sample, 2=saved, -1=retry sample, -2=save error.
    int enroll(const int16_t* samples, bool voice);
    // Fixed-window enrollment: stage all three before comparing/publishing.
    // -1 discards the whole pending set; active registration is retained.
    int submitEnrollment(const localwake::Features& candidate);
    unsigned enrollmentCount() const { return enrolled_; }
    void cancelEnrollment();
 private:
    struct Store {
        uint32_t magic = 0x3243574c, version = 2, generation = 0; float threshold = 24;
        localwake::Features templates[3]; uint32_t crc = 0;
    };
    Store active_, pending_; localwake::Segmenter segment_, energySegment_;
    bool registered_ = false; unsigned enrolled_ = 0; int slot_ = -1;
    Diagnostics diagnostics_;
    uint32_t backgroundEpoch_ = 0;
    uint32_t energyEpoch_ = 0;
    RollingWakeWindow rolling_;
    localwake::Features rollingStored_;
    bool read(const char* path, Store& target);
    bool commit();
    void resetPending();
};
