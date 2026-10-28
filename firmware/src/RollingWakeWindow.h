#pragma once
#include "EnrollmentWindow.h"
#include "LocalWakeCore.h"
// Overlapping 5 s windows, each with its own preceding 1 s noise
// reference. Store only local MFCC and level metadata, never a PCM recording.
class RollingWakeWindow {
 public:
    void reset() { total_=0; }
    // -1=collecting, 0=window without a valid crop, 1=candidate in target.
    int push(const int16_t* pcm, localwake::Features& target) {
        levels_[total_%300]=EnrollmentWindow::level(pcm);
        unsigned clipped=0;
        for(unsigned i=0;i<320;++i) clipped += pcm[i]>32000 || pcm[i]<-32000;
        clips_[total_%250]=uint16_t(clipped);
        localwake::extract(pcm,features_[total_%250]);
        ++total_;
        if (total_<300 || (total_-300)%125) return -1;
        window_.reset();
        for(unsigned i=0;i<50;++i) window_.backgroundLevel(levels_[(total_-300+i)%300]);
        for(unsigned i=0;i<250;++i) window_.captureLevel(levels_[(total_-250+i)%300]);
        unsigned first=0,end=0;
        if (!window_.range(first,end)) { target.count=0; return 0; }
        clipped=0;
        for(unsigned i=first;i<end;++i) clipped += clips_[(total_-250+i)%250];
        if (clipped>=100) { target.count=0; return 0; }
        target.count=uint16_t(end-first);
        for(unsigned i=0;i<target.count;++i)
            memcpy(target.frames[i],features_[(total_-250+first+i)%250],sizeof(target.frames[i]));
        return 1;
    }
 private:
    uint64_t total_=0;
    float levels_[300]={};
    int16_t features_[250][localwake::Dimensions]={};
    uint16_t clips_[250]={};
    EnrollmentWindow window_;
};
