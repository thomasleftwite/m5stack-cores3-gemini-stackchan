#pragma once
#include <math.h>
#include <stdint.h>
#include <algorithm>
// Estimate background independently of the current speech decision. A four-second
// window outlasts the wake segment (three seconds), so speech cannot pin the floor.
class VoiceActivity {
 public:
    void beginCalibration() {
        count_ = position_ = refresh_ = 0; calibrating_ = true; voice_ = false; ++epoch_;
    }
    uint32_t epoch() const { return epoch_; }
    bool update(float raw) {
        raw = isfinite(raw) ? fmaxf(0, raw) : 0;
        if (calibrating_) {
            history_[count_++] = raw;
            if (count_ == CalibrationFrames) {
                // Seed the full window with measured ambient sound, not zeroes.
                for (unsigned i = count_; i < WindowFrames; ++i) history_[i] = history_[i % CalibrationFrames];
                estimate(); reference_ = floor_; calibrating_ = false; ++epoch_;
            }
            return false;
        }
        history_[position_] = raw; position_ = (position_ + 1) % WindowFrames;
        if (++refresh_ == 10) {
            refresh_ = 0; estimate();
            if (fabsf(floor_ - reference_) > fmaxf(40, reference_ * .5f)) {
                reference_ = floor_; voice_ = false; ++epoch_;
            }
        }
        const float margin = voice_ ? fmaxf(30, spread_ * 2.5f) : fmaxf(60, spread_ * 5);
        voice_ = raw > fmaxf(voice_ ? 80 : 100, floor_ + margin);
        return voice_;
    }
 private:
    static constexpr unsigned CalibrationFrames = 30, WindowFrames = 200;
    void estimate() {
        float sorted[WindowFrames];
        std::copy(history_, history_ + WindowFrames, sorted);
        std::sort(sorted, sorted + WindowFrames);
        floor_ = sorted[WindowFrames / 5];
        spread_ = fmaxf(5, sorted[WindowFrames * 3 / 10] - sorted[WindowFrames / 10]);
    }
    float history_[WindowFrames] = {}, floor_ = 0, spread_ = 5, reference_ = 0;
    unsigned count_ = 0, position_ = 0, refresh_ = 0;
    uint32_t epoch_ = 1;
    bool calibrating_ = true, voice_ = false;
};
