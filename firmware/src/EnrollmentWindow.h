#pragma once
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <algorithm>
// Local metadata only. PCM is owned by AudioTask's existing PSRAM buffer.
class EnrollmentWindow {
 public:
    static constexpr unsigned BackgroundFrames = 50, CaptureFrames = 250;
    float backgroundRms() const { return background_==BackgroundFrames ? noise_[45] : 0; }
    float warmedBackgroundRms() const { return warmedNoise_; }
    float triggerRms() const { return threshold_; }
    void reset() { background_ = captured_ = 0; threshold_ = 100; warmedNoise_=0; }
    static float level(const int16_t* pcm) {
        double mean=0, energy=0;
        for (unsigned i=0;i<320;++i) mean += pcm[i];
        mean /= 320;
        for (unsigned i=0;i<320;++i) { double v=pcm[i]-mean; energy += v*v; }
        return sqrtf(float(energy/320));
    }
    bool background(const int16_t* pcm) {
        return backgroundLevel(level(pcm));
    }
    bool backgroundLevel(float value) {
        if (background_ < BackgroundFrames) noise_[background_++] = value;
        if (background_ != BackgroundFrames) return false;
        float warmed[30]; std::copy(noise_+20,noise_+50,warmed); std::sort(warmed,warmed+30);
        warmedNoise_=warmed[26];
        std::sort(noise_, noise_+BackgroundFrames);
        float spread=noise_[40]-noise_[10];
        threshold_=std::max(100.f, noise_[45]+std::max(60.f, spread*3));
        return true;
    }
    bool capture(const int16_t* pcm) {
        return captureLevel(level(pcm));
    }
    bool captureLevel(float value) {
        if (captured_ < CaptureFrames) levels_[captured_++] = value;
        return captured_ == CaptureFrames;
    }
    // Bridge up to 500 ms between syllables, then choose the interval with the
    // most above-background frames. A short cue tail cannot become a word.
    bool range(unsigned& first, unsigned& end, unsigned gapFrames = 25) const {
        if (captured_ != CaptureFrames) return false;
        unsigned start=0, last=0, count=0, best=0, bestFirst=0, bestEnd=0;
        for (unsigned n=0;n<=captured_;++n) {
            bool high=n<captured_ && levels_[n]>threshold_;
            if (count && (n==captured_ || n-last>gapFrames)) {
                if (count>best) { best=count; bestFirst=start; bestEnd=last+1; }
                count=0;
            }
            if (high) { if (!count) start=n; last=n; ++count; }
        }
        if (best<10) return false;
        first=bestFirst>3 ? bestFirst-3 : 0;
        end=std::min(captured_, bestEnd+3);
        return end-first>=10 && end-first<=150;
    }
 private:
    float noise_[BackgroundFrames]={}, levels_[CaptureFrames]={}, threshold_=100,warmedNoise_=0;
    unsigned background_=0, captured_=0;
};
