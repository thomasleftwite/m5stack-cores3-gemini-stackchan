#include "ReferenceWake.h"
#include "LocalStorage.h"
#include "ConversationRules.h"
#include "RuntimeTrace.h"
#include <esp_heap_caps.h>
#include <new>
#include <string.h>
namespace { const char* const slots[]={"/wake-simplevox-v3-a.bin","/wake-simplevox-v3-b.bin"}; }
ReferenceWake::~ReferenceWake() {
    cancelEnrollment(); engine_.deinit();
    if(ring_) heap_caps_free(ring_); if(crop_) heap_caps_free(crop_);
}
bool ReferenceWake::read(const char* path,Store& target) {
    File f=localstorage::filesystem().open(path,"r");
    if(!f || f.size()!=sizeof(Store) || f.read((uint8_t*)&target,sizeof(Store))!=sizeof(Store)) return false;
    return valid(target);
}
bool ReferenceWake::valid(const Store& target) {
    return target.magic==0x3356574c && target.version==3 && target.threshold==Threshold &&
        target.feature.count>=2 && target.feature.count<=MaxFrames &&
        target.crc==conversation::crc32((uint8_t*)&target,offsetof(Store,crc));
}
ReferenceWake::RegistrationAudit ReferenceWake::auditRegistration() {
    RegistrationAudit audit;
    if(!registered_ || !active_ || !scratch_ || slot_<0 || slot_>1) return audit;
    audit.frames=active_->feature.count; audit.generation=active_->generation; audit.slot=unsigned(slot_);
    audit.ramValid=valid(*active_); audit.fileValid=read(slots[slot_],*scratch_);
    audit.same=audit.ramValid && audit.fileValid && memcmp(active_.get(),scratch_.get(),sizeof(Store))==0;
#ifdef ARDUINO
    Serial.printf("[DEBUG-sv6] active=%u generation=%u frames=%u ram=%u disk=%u same=%u\n",audit.slot,unsigned(audit.generation),audit.frames,unsigned(audit.ramValid),unsigned(audit.fileValid),unsigned(audit.same));
#endif
    return audit;
}
bool ReferenceWake::begin() {
    active_.reset(new(std::nothrow) Store); scratch_.reset(new(std::nothrow) Store);
    previous_.reset(new(std::nothrow) Store); previousValid_=false;
    ring_=(int16_t*)heap_caps_calloc(80000,sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    crop_=(int16_t*)heap_caps_calloc(48000,sizeof(int16_t),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!active_ || !scratch_ || !ring_ || !crop_) {
#ifdef ARDUINO
        Serial.println("[DEBUG-sv1] init=0 reason=1");
#endif
        return false;
    }
    if(!engine_.init(simplevox::MfccConfig{})) {
#ifdef ARDUINO
        Serial.println("[DEBUG-sv1] init=0 reason=2");
#endif
        return false;
    }
    initialized_=true;
#ifdef ARDUINO
    for(unsigned profile=0;profile<4;++profile) {
        EngineProbe probe;
        if(probeEngine(probe,profile!=0,profile==2))
            Serial.printf("[DEBUG-sv3] profile=%u self=%u level=%u other=%u raw=%u raw_other=%u feature=%u feature_other=%u align=%u\n",
                profile,unsigned(probe.same),unsigned(probe.level),unsigned(probe.other),unsigned(probe.raw),unsigned(probe.rawOther),
                unsigned(probe.feature),unsigned(probe.featureOther),probe.align);
        else Serial.println("[DEBUG-sv2] probe_failed=1");
        delay(1);
    }
#endif
    bool a=read(slots[0],*active_),b=read(slots[1],*scratch_);
#ifdef ARDUINO
    Serial.printf("[DEBUG-sv6] slot=0 valid=%u generation=%u frames=%u\n",unsigned(a),a ? unsigned(active_->generation):0,a ? unsigned(active_->feature.count):0);
    Serial.printf("[DEBUG-sv6] slot=1 valid=%u generation=%u frames=%u\n",unsigned(b),b ? unsigned(scratch_->generation):0,b ? unsigned(scratch_->feature.count):0);
#endif
    if(b && (!a || int32_t(scratch_->generation-active_->generation)>0)) {
        if(a && previous_) { *previous_=*active_; previousValid_=true; }
        *active_=*scratch_; slot_=1;
    }
    else if(a) {
        slot_=0;
        if(b && previous_) { *previous_=*scratch_; previousValid_=true; }
    }
    registered_=a || b; resetDetection();
    if(registered_) auditRegistration();
#ifdef ARDUINO
    Serial.printf("[DEBUG-sv1] init=1 stored=%u\n",unsigned(registered_));
#endif
    return registered_;
}
void ReferenceWake::traceEnrollment(unsigned state) {
#ifdef ARDUINO
    Serial.printf("[DEBUG-sv8] session=%u state=%u take=%u frames=%u\n",unsigned(enrollmentSession_),state,enrolled_,
        candidateReady_ && pending_[candidateIndex_] ? unsigned(pending_[candidateIndex_]->size()) : 0);
#endif
}
void ReferenceWake::beginEnrollment() {
    cancelEnrollment();
    if(++enrollmentSession_==0) ++enrollmentSession_;
    enrollmentActive_=true; publicationUncertain_=false; traceEnrollment(1);
}
void ReferenceWake::cancelEnrollment() {
    if(enrollmentActive_) traceEnrollment(5);
    for(auto& f:pending_) f.reset();
    enrolled_=0; candidateIndex_=0; enrollmentActive_=candidateReady_=candidateVerified_=false;
    resetDetection();
}
void ReferenceWake::resetDetection() {
    total_=0; window_.reset();
    candidateVerified_=false;
    inputTimed_=false; previousInputMs_=maxInputGapMs_=0;
    if(ring_) memset(ring_,0,80000*sizeof(int16_t));
    if(crop_) memset(crop_,0,48000*sizeof(int16_t));
}
int ReferenceWake::submitEnrollment(const int16_t* pcm,size_t count) {
    if(!initialized_) { cancelEnrollment(); return -2; }
    if(!enrollmentActive_ || candidateReady_) return -1;
    if(!pcm || count<6400 || count>48000 || enrolled_>=3) { cancelEnrollment(); return -1; }
    pending_[enrolled_].reset(engine_.create(pcm,int(count)));
    const auto* candidate=pending_[enrolled_].get();
    const auto config=engine_.config();
    const unsigned expectedFrames=(count-config.frame_length())/config.hop_length()+1;
    if(!candidate || candidate->size()!=int(expectedFrames) || candidate->size()<2 || candidate->size()>int(MaxFrames) || candidate->dimension()!=int(Dimensions)) {
        cancelEnrollment(); return -1;
    }
#ifdef ARDUINO
    Serial.printf("[DEBUG-sv6] take=%u samples=%u frames=%u expected=%u\n",enrolled_+1,unsigned(count),unsigned(candidate->size()),expectedFrames);
#endif
    if(++enrolled_<3) return 1;
    uint64_t totals[3]={}; bool consistent=true;
    for(unsigned i=0;i<3;++i) for(unsigned j=i+1;j<3;++j) {
        auto distance=simplevox::calcDTW(*pending_[i],*pending_[j]);
#ifdef ARDUINO
        Serial.printf("[DEBUG-sv1] pair=%u distance=%u\n",i*3+j,unsigned(distance>2000 ? 2001 : distance));
#endif
        if(distance>=Threshold) consistent=false;
        totals[i]+=distance; totals[j]+=distance;
    }
    if(!consistent) { cancelEnrollment(); return -1; }
    unsigned best=0; for(unsigned i=1;i<3;++i) if(totals[i]<totals[best]) best=i;
    candidateIndex_=best; candidateReady_=true; candidateVerified_=false;
    resetDetection(); traceEnrollment(2); return 2;
}
bool ReferenceWake::verifyEnrollmentWindow(const int16_t* samples,bool allowMatch) {
    if(!candidateReady_ || !pending_[candidateIndex_]) return false;
    const bool matched=processWindow(samples,true);
    if(!allowMatch) { candidateVerified_=false; return false; }
    if(matched && !candidateVerified_) { candidateVerified_=true; traceEnrollment(3); }
    return candidateVerified_;
}
bool ReferenceWake::commitEnrollment() {
    if(!candidateReady_ || !candidateVerified_ || !pending_[candidateIndex_]) return false;
    const bool saved=commit(*pending_[candidateIndex_]);
    traceEnrollment(saved ? 4 : publicationUncertain_ ? 7 : 6);
    cancelEnrollment(); resetDetection(); return saved;
}
bool ReferenceWake::probeEngine(EngineProbe& result,bool integerStimulus,bool portable) {
    if(!initialized_ || !crop_) return false;
    engine_.setPortableDsp(portable); result.align=engine_.bufferAlignment();
    auto stimulus=[&](bool alternate,bool weak) {
        const float a[]={600,300,800},b[]={1400,2200,1100};
        const float c[]={250,900,400},d[]={3400,3400,3200};
        for(unsigned i=0;i<15360;++i) {
            unsigned vowel=i/5120;
            if(integerStimulus) {
                const unsigned x[]={600,300,800},y[]={1400,2200,1100},z[]={250,900,400},w[]={3400,3400,3200};
                int value=((i*(alternate ? z[vowel]:x[vowel]))%16000<8000 ? 4000:-4000)+
                    ((i*(alternate ? w[vowel]:y[vowel]))%16000<8000 ? 2400:-2400);
                crop_[i]=int16_t(weak ? value*2/5 : value);
            } else {
                float t=float(i)/16000,gain=weak ? .4f:1.f;
                crop_[i]=int16_t(gain*4000*(sinf(6.283185307f*(alternate ? c[vowel]:a[vowel])*t)+
                    .6f*sinf(6.283185307f*(alternate ? d[vowel]:b[vowel])*t)));
            }
        }
    };
    stimulus(false,false); result.raw=conversation::crc32((uint8_t*)crop_,15360*sizeof(int16_t));
    std::unique_ptr<simplevox::MfccFeature> first(engine_.create(crop_,15360));
    stimulus(false,true); std::unique_ptr<simplevox::MfccFeature> weak(engine_.create(crop_,15360));
    stimulus(true,false); result.rawOther=conversation::crc32((uint8_t*)crop_,15360*sizeof(int16_t));
    std::unique_ptr<simplevox::MfccFeature> other(engine_.create(crop_,15360));
    memset(crop_,0,15360*sizeof(int16_t));
    engine_.setPortableDsp(false);
    if(!first || !weak || !other) return false;
    result.feature=conversation::crc32((const uint8_t*)first->feature(0),first->size()*Dimensions*sizeof(int16_t));
    result.featureOther=conversation::crc32((const uint8_t*)other->feature(0),other->size()*Dimensions*sizeof(int16_t));
    result.same=simplevox::calcDTW(*first,*first); result.level=simplevox::calcDTW(*first,*weak);
    result.other=simplevox::calcDTW(*first,*other); return true;
}
bool ReferenceWake::commit(const simplevox::MfccFeature& feature) {
    publicationUncertain_=false;
    std::unique_ptr<Store> verify(new(std::nothrow) Store);
    if(!verify) return false;
    memset(scratch_.get(),0,sizeof(Store));
    scratch_->magic=0x3356574c; scratch_->version=3; scratch_->threshold=Threshold;
    scratch_->generation=registered_ ? active_->generation+1 : 1;
    scratch_->feature.count=uint16_t(feature.size());
    for(int i=0;i<feature.size();++i) memcpy(scratch_->feature.frames[i],feature.feature(i),Dimensions*sizeof(int16_t));
    scratch_->crc=conversation::crc32((uint8_t*)scratch_.get(),offsetof(Store,crc));
    runtimetrace::mark(runtimetrace::StoreBegin);
    const char* temp="/wake-simplevox-v3.tmp";
    File f=localstorage::filesystem().open(temp,"w"); if(!f) return false;
    bool written=f.write((uint8_t*)scratch_.get(),sizeof(Store))==sizeof(Store); f.flush(); f.close();
    if(!written || !read(temp,*verify) || memcmp(verify.get(),scratch_.get(),sizeof(Store))) return false;
    int next=slot_==0 ? 1 : 0;
    if(localstorage::filesystem().exists(slots[next]) && !localstorage::filesystem().remove(slots[next])) return false;
    if(!localstorage::filesystem().rename(temp,slots[next])) return false;
    if(!read(slots[next],*verify) || memcmp(verify.get(),scratch_.get(),sizeof(Store))) {
        // The previously active slot is untouched. Remove only the newly
        // published candidate so a reported failure cannot become active on boot.
        // If removal also fails the UI must explicitly report uncertain storage.
        publicationUncertain_=!localstorage::filesystem().remove(slots[next]);
        return false;
    }
    if(registered_ && previous_) { *previous_=*active_; previousValid_=true; }
    *active_=*scratch_; slot_=next; registered_=true;
    auditRegistration();
    runtimetrace::mark(runtimetrace::StorePublished); return true;
}
bool ReferenceWake::detectWindow(const int16_t* samples) {
    if(enrollmentActive_) return false;
    return processWindow(samples,false);
}
bool ReferenceWake::processWindow(const int16_t* samples,bool enrollment) {
    if(!initialized_ || !samples) return false;
    if(enrollment ? !candidateReady_ || !pending_[candidateIndex_] : !registered_) return false;
#ifdef ARDUINO
    const uint32_t inputMs=millis();
    if(inputTimed_) maxInputGapMs_=std::max(maxInputGapMs_,uint32_t(inputMs-previousInputMs_));
    previousInputMs_=inputMs; inputTimed_=true;
#endif
    ++diagnostics_.frames;
    unsigned clips=0; for(unsigned i=0;i<320;++i) clips += samples[i]>32000 || samples[i]<-32000;
    clips_[total_%250]=uint16_t(clips); levels_[total_%300]=EnrollmentWindow::level(samples);
    memcpy(ring_+(total_%250)*320,samples,320*sizeof(int16_t)); ++total_;
    if(total_<300 || (total_-300)%125) return false;
#ifdef ARDUINO
    if(!enrollment) Serial.printf("[DEBUG-sv6] runtime_frames=%u ram=%u\n",unsigned(active_->feature.count),unsigned(valid(*active_)));
#endif
    if(!enrollment && !valid(*active_)) { registered_=false; return false; }
#ifdef ARDUINO
    const uint32_t computeStarted=millis();
#endif
    window_.reset();
    for(unsigned i=0;i<50;++i) window_.backgroundLevel(levels_[(total_-300+i)%300]);
    for(unsigned i=0;i<250;++i) window_.captureLevel(levels_[(total_-250+i)%300]);
    unsigned first=0,end=0;
    if(!window_.range(first,end) || (end-first)*320<6400) { ++diagnostics_.tooShort; return false; }
    clips=0; for(unsigned i=first;i<end;++i) clips+=clips_[(total_-250+i)%250];
    if(clips>=100) { ++diagnostics_.clipped; return false; }
    probe_=WindowProbe{};
    probe_.first=first; probe_.end=end;
    probe_.templateFrames=enrollment ? unsigned(pending_[candidateIndex_]->size()) : active_->feature.count;
    probe_.noise=unsigned(window_.backgroundRms()); probe_.warm=unsigned(window_.warmedBackgroundRms());
    probe_.gate=unsigned(window_.triggerRms()); probe_.clips=clips;
    float levelSum=0;
    for(unsigned i=first;i<end;++i) {
        const int16_t* part=ring_+((total_-250+i)%250)*320;
        memcpy(crop_+(i-first)*320,part,320*sizeof(int16_t));
        float level=levels_[(total_-250+i)%300]; levelSum+=level;
        probe_.peak=std::max(probe_.peak,unsigned(level));
        int32_t dc=0; for(unsigned j=0;j<320;++j) dc+=part[j]; dc/=320;
        probe_.dc=std::max(probe_.dc,unsigned(dc<0 ? -dc : dc));
    }
    probe_.rms=unsigned(levelSum/(end-first));
    const size_t count=(end-first)*320;
    std::unique_ptr<simplevox::MfccFeature> candidate(engine_.create(crop_,int(count)));
    memset(crop_,0,count*sizeof(int16_t));
    if(!candidate) return false;
    probe_.candidateFrames=unsigned(candidate->size());
    ++diagnostics_.complete;
    auto distance=enrollment ? simplevox::calcDTW(*pending_[candidateIndex_],*candidate) :
                              simplevox::calcDTW(active_->feature,*candidate);
    probe_.currentDistance=distance>2000 ? 2001:unsigned(distance);
    probe_.previousAvailable=!enrollment && previousValid_ && previous_ && valid(*previous_);
    if(probe_.previousAvailable) {
        auto previousDistance=simplevox::calcDTW(previous_->feature,*candidate);
        probe_.previousDistance=previousDistance>2000 ? 2001:unsigned(previousDistance);
    }
    bool matched=distance<Threshold;
    if(!enrollment) {
        diagnostics_.lastMatches=matched ? 1 : 0;
        if(matched) ++diagnostics_.matched;
    }
#ifdef ARDUINO
    probe_.computeMs=millis()-computeStarted; probe_.inputGapMs=maxInputGapMs_; maxInputGapMs_=0;
    Serial.printf("[DEBUG-sv5] first=%u end=%u template_frames=%u candidate_frames=%u\n",probe_.first,probe_.end,probe_.templateFrames,probe_.candidateFrames);
    Serial.printf("[DEBUG-sv5] noise=%u warm=%u gate=%u rms=%u peak=%u dc=%u clips=%u\n",probe_.noise,probe_.warm,probe_.gate,probe_.rms,probe_.peak,probe_.dc,probe_.clips);
    Serial.printf("[DEBUG-sv5] compute_ms=%u input_gap_ms=%u\n",unsigned(probe_.computeMs),unsigned(probe_.inputGapMs));
    Serial.printf("[DEBUG-sv7] current=%u previous=%u available=%u\n",probe_.currentDistance,probe_.previousDistance,unsigned(probe_.previousAvailable));
    Serial.printf("[DEBUG-sv1] frames=%u distance=%u threshold=%u matched=%u\n",unsigned(candidate->size()),
        unsigned(distance>2000 ? 2001 : distance),Threshold,unsigned(matched));
#endif
    return matched;
}
