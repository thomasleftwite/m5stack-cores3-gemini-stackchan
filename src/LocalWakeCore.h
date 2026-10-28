#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include "MfccFeatures.h"
// MFCC templates. All frames and features stay local; v1 spectra are incompatible.
namespace localwake {
constexpr size_t Dimensions = 12, MaxFrames = 150, FrameSamples = 320;
// Count qualified voiced portions, not the full spoken word's wall-clock time.
// Require 200 ms of both classifier and periodic spectral evidence.
constexpr size_t MinimumSpeechFrames = 10, MinimumShapedFrames = 10;
struct Features { uint16_t count = 0; int16_t frames[MaxFrames][Dimensions] = {}; };
// A word's constant-spectrum counterpart: useful for checking whether matching
// actually distinguishes its phonetic sequence from stationary sound.
inline void stationary(const Features& source, Features& out) {
    out.count = source.count;
    if (!source.count) return;
    for (size_t k = 0; k < Dimensions; ++k) {
        int32_t sum = 0;
        for (size_t i = 0; i < source.count; ++i) sum += source.frames[i][k];
        for (size_t i = 0; i < out.count; ++i) out.frames[i][k] = int16_t(sum / source.count);
    }
}
inline int averagedBand(const Features& source, size_t frame, size_t band) {
    size_t first = frame > 2 ? frame - 2 : 0, last = std::min(size_t(source.count - 1), frame + 2);
    int32_t sum = 0;
    for (size_t i = first; i <= last; ++i) sum += source.frames[i][band];
    return sum / int(last - first + 1);
}
// Smooth fast fluctuations while retaining the word's persistent spectral shape.
// MFCC excludes C0, so level changes do not require subtracting the word's timbre.
inline void prepareForMatch(const Features& source, Features& out) {
    out.count = source.count;
    if (!source.count) return;
    for (size_t k = 0; k < Dimensions; ++k) {
        for (size_t i = 0; i < out.count; ++i) out.frames[i][k] = int16_t(averagedBand(source, i, k));
    }
}
inline bool extract(const int16_t* samples, int16_t* out) {
    return detail::mfcc(samples, out);
}
inline float distance(const Features& a, const Features& b, bool limitWarp = false) {
    if (a.count < 10 || b.count < 10 || a.count > MaxFrames || b.count > MaxFrames || a.count > b.count * 2 || b.count > a.count * 2) return 1000000;
    float previous[MaxFrames + 1], current[MaxFrames + 1];
    for (size_t j = 0; j <= b.count; ++j) previous[j] = j ? 1e9f : 0;
    for (size_t i = 1; i <= a.count; ++i) {
        current[0] = 1e9f;
        for (size_t j = 1; j <= b.count; ++j) {
            if (limitWarp) {
                // Scaled diagonal: allow global speaking-speed differences but
                // not arbitrary stretching of individual phonemes (20% band).
                int offset = int((i - 1) * (b.count - 1)) - int((j - 1) * (a.count - 1));
                int radius = std::max(int((a.count - 1) * (b.count - 1) / 5), int(std::max(a.count, b.count) - 1));
                if (abs(offset) > radius) { current[j] = 1e9f; continue; }
            }
            float d = 0;
            for (size_t k = 0; k < Dimensions; ++k) d += abs(int(a.frames[i-1][k]) - int(b.frames[j-1][k]));
            current[j] = d / Dimensions + std::min(previous[j-1], std::min(previous[j], current[j-1]));
        }
        memcpy(previous, current, (b.count + 1) * sizeof(float));
    }
    return previous[b.count] / std::max(a.count, b.count);
}
struct WordComparison {
    float actual, stationary;
    bool matches(float threshold, float margin = .15f) const {
        // Require evidence for the changing sequence, beyond its average timbre.
        return actual <= threshold && stationary - actual >= fmaxf(1, stationary * margin);
    }
};
inline WordComparison compareWord(const Features& candidate, const Features& stored, const Features& stationaryCandidate) {
    return {distance(candidate, stored, true), distance(stationaryCandidate, stored, true)};
}
class Segmenter {
 public:
    enum Rejection { NoRejection, TooShort, Clipped, TooLong };
    enum Result { None, Complete, Rejected }; Features feature;
    Rejection rejection() const { return rejection_; }
    size_t voicedFrames() const { return voiced_; }
    size_t shapedFrames() const { return shaped_; }
    bool speechEvidence() const { return voiced_ >= MinimumSpeechFrames && shaped_ >= MinimumShapedFrames; }
    void reset() { feature.count = 0; silence_ = voiced_ = clipped_ = shaped_ = 0; active_ = false; rejection_ = NoRejection; }
    Result push(const int16_t* samples, bool voice) {
        if (!active_ && !voice) return None;
        active_ = true;
        for (size_t i = 0; i < FrameSamples; ++i) if (samples[i] > 32000 || samples[i] < -32000) ++clipped_;
        if (voice) { ++voiced_; silence_ = 0; } else ++silence_;
        if (feature.count < MaxFrames) {
            bool shaped = extract(samples, feature.frames[feature.count++]);
            if (voice && shaped) ++shaped_;
        }
        if (feature.count >= MaxFrames) { active_ = false; rejection_ = TooLong; return Rejected; }
        if (silence_ >= 20) {
            feature.count = uint16_t(feature.count - silence_); active_ = false;
            rejection_ = voiced_ < 10 ? TooShort : clipped_ >= 100 ? Clipped : NoRejection;
            return rejection_ == NoRejection ? Complete : Rejected;
        }
        return None;
    }
 private:
    size_t silence_ = 0, voiced_ = 0, clipped_ = 0, shaped_ = 0; bool active_ = false;
    Rejection rejection_ = NoRejection;
};
}
