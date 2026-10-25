#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
// Speaker-dependent log-spectrum templates. All frames and features stay local.
namespace localwake {
constexpr size_t Dimensions = 12, MaxFrames = 150, FrameSamples = 320;
struct Features { uint16_t count = 0; int16_t frames[MaxFrames][Dimensions] = {}; };
inline void extract(const int16_t* samples, int16_t* out) {
    static const float frequencies[Dimensions] = {180,260,380,550,800,1100,1500,2000,2600,3300,4200,5500};
    float values[Dimensions], mean = 0, dc = 0;
    for (size_t i = 0; i < FrameSamples; ++i) dc += samples[i]; dc /= FrameSamples;
    for (size_t b = 0; b < Dimensions; ++b) {
        float c = 2 * cosf(6.283185307f * frequencies[b] / 16000), q1 = 0, q2 = 0;
        for (size_t i = 0; i < FrameSamples; ++i) { float q = (samples[i] - dc) / 32768.0f + c * q1 - q2; q2 = q1; q1 = q; }
        values[b] = logf(1e-8f + fmaxf(0, q1*q1 + q2*q2 - c*q1*q2)); mean += values[b];
    }
    mean /= Dimensions;
    for (size_t b = 0; b < Dimensions; ++b) out[b] = int16_t((values[b] - mean) * 20);
}
inline float distance(const Features& a, const Features& b) {
    if (a.count < 10 || b.count < 10 || a.count > MaxFrames || b.count > MaxFrames || a.count > b.count * 2 || b.count > a.count * 2) return 1000000;
    float previous[MaxFrames + 1], current[MaxFrames + 1];
    for (size_t j = 0; j <= b.count; ++j) previous[j] = j ? 1e9f : 0;
    for (size_t i = 1; i <= a.count; ++i) {
        current[0] = 1e9f;
        for (size_t j = 1; j <= b.count; ++j) {
            float d = 0;
            for (size_t k = 0; k < Dimensions; ++k) d += abs(int(a.frames[i-1][k]) - int(b.frames[j-1][k]));
            current[j] = d / Dimensions + std::min(previous[j-1], std::min(previous[j], current[j-1]));
        }
        memcpy(previous, current, (b.count + 1) * sizeof(float));
    }
    return previous[b.count] / std::max(a.count, b.count);
}
class Segmenter {
 public:
    enum Result { None, Complete, Rejected }; Features feature;
    void reset() { feature.count = 0; silence_ = voiced_ = clipped_ = 0; active_ = false; }
    Result push(const int16_t* samples, bool voice) {
        if (!active_ && !voice) return None;
        active_ = true;
        for (size_t i = 0; i < FrameSamples; ++i) if (samples[i] > 32000 || samples[i] < -32000) ++clipped_;
        if (voice) { ++voiced_; silence_ = 0; } else ++silence_;
        if (feature.count < MaxFrames) extract(samples, feature.frames[feature.count++]);
        if (feature.count >= MaxFrames) { active_ = false; return Rejected; }
        if (silence_ >= 20) { feature.count = uint16_t(feature.count - silence_); active_ = false; return voiced_ >= 10 && clipped_ < 100 ? Complete : Rejected; }
        return None;
    }
 private:
    size_t silence_ = 0, voiced_ = 0, clipped_ = 0; bool active_ = false;
};
}
