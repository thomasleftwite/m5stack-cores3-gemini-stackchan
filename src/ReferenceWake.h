#pragma once
#include <Arduino.h>
#include <simplevox.h>
#include <memory>
#include "EnrollmentWindow.h"
// MFCC/cosine DTW follows the pinned working reference. Capture/UI are local.
class ReferenceWake {
 public:
    struct Diagnostics { uint32_t frames=0,voice=0,complete=0,tooShort=0,clipped=0,tooLong=0,matched=0; unsigned lastMatches=0; };
    ~ReferenceWake();
    bool begin();
    bool registered() const { return registered_; }
    struct RegistrationAudit {
        unsigned frames=0,slot=2; uint32_t generation=0;
        bool ramValid=false,fileValid=false,same=false;
    };
    RegistrationAudit auditRegistration();
    bool detectWindow(const int16_t* samples);
    void resetDetection();
    const Diagnostics& diagnostics() const { return diagnostics_; }
    // Temporary S05 metadata. No PCM or feature values leave the device.
    struct WindowProbe {
        unsigned first=0,end=0,templateFrames=0,candidateFrames=0;
        unsigned noise=0,warm=0,gate=0,rms=0,peak=0,dc=0,clips=0;
        uint32_t computeMs=0,inputGapMs=0;
        unsigned currentDistance=2001,previousDistance=2001;
        bool previousAvailable=false;
    };
    const WindowProbe& lastWindowProbe() const { return probe_; }
    void beginEnrollment();
    void cancelEnrollment();
    unsigned enrollmentCount() const { return enrolled_; }
    uint32_t enrollmentSession() const { return enrollmentSession_; }
    bool publicationUncertain() const { return publicationUncertain_; }
    // Three consistent takes prepare a candidate; only a separate rolling-window
    // verification permits publication. The previous registration stays active.
    int submitEnrollment(const int16_t* pcm,size_t count);
    bool enrollmentCandidateReady() const { return candidateReady_; }
    bool verifyEnrollmentWindow(const int16_t* samples,bool allowMatch=true);
    bool commitEnrollment();
    struct EngineProbe { uint32_t same=0,level=0,other=0,raw=0,rawOther=0,feature=0,featureOther=0; unsigned align=0; };
    bool probeEngine(EngineProbe& result,bool integerStimulus=false,bool portable=false);
 private:
    static constexpr unsigned MaxFrames=186,Dimensions=12,Threshold=250;
    struct Feature : simplevox::ISoundFeature<Feature> {
        uint16_t count=0; int16_t frames[MaxFrames][Dimensions]={};
        int size() const { return count; }
        int dimension() const { return Dimensions; }
        const int16_t* feature(int i) const { return frames[i]; }
    };
    struct Store { uint32_t magic=0x3356574c,version=3,generation=0,threshold=Threshold; Feature feature; uint32_t crc=0; };
    simplevox::MfccEngine engine_;
    std::unique_ptr<simplevox::MfccFeature> pending_[3];
    std::unique_ptr<Store> active_,scratch_,previous_;
    int16_t* ring_=nullptr; int16_t* crop_=nullptr;
    float levels_[300]={}; uint16_t clips_[250]={};
    uint64_t total_=0; unsigned enrolled_=0; int slot_=-1;
    bool initialized_=false,registered_=false;
    bool previousValid_=false;
    bool enrollmentActive_=false, candidateReady_=false, candidateVerified_=false;
    bool publicationUncertain_=false;
    uint32_t enrollmentSession_=0;
    unsigned candidateIndex_=0;
    Diagnostics diagnostics_;
    WindowProbe probe_;
    uint32_t previousInputMs_=0,maxInputGapMs_=0;
    bool inputTimed_=false;
    EnrollmentWindow window_;
    bool read(const char* path,Store& target);
    static bool valid(const Store& store);
    bool commit(const simplevox::MfccFeature& feature);
    bool processWindow(const int16_t* samples,bool enrollment);
    void traceEnrollment(unsigned state);
};
