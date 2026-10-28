#include "../src/ReferenceWake.h"
#include "SyntheticSpeech.h"
#include <LittleFS.h>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <algorithm>
#include <chrono>

// All samples are mathematical synthetic signals. No device/audio input or files.
static std::vector<int16_t> utterance(unsigned frames,float amplitude,bool other=false) {
    std::vector<int16_t> out(frames*320);
    for(unsigned n=0;n<frames;++n) syntheticSpeechFrame(out.data()+n*320,n*30/frames,amplitude,other);
    return out;
}
static void background(int16_t* out,unsigned amplitude=20) {
    for(unsigned i=0;i<320;++i) out[i]=int16_t(500+(i%2 ? int(amplitude):-int(amplitude)));
}
static std::vector<int16_t> enrollmentCrop(unsigned take,bool other=false,bool report=true) {
    const unsigned durations[]={34,35,37},onsets[]={64,100,180},noise[]={20,25,18};
    const float gains[]={2800,3300,2900};
    auto word=utterance(durations[take],gains[take],other);
    EnrollmentWindow crop; int16_t frame[320];
    for(unsigned n=0;n<50;++n) { background(frame,noise[take]); assert(crop.background(frame)==(n==49)); }
    std::vector<int16_t> recording(250*320);
    for(unsigned n=0;n<250;++n) {
        background(frame,noise[take]);
        if(n>=onsets[take] && n<onsets[take]+durations[take]) {
            for(unsigned i=0;i<320;++i) frame[i]+=word[(n-onsets[take])*320+i];
        }
        std::copy(frame,frame+320,recording.begin()+n*320);
        assert(crop.capture(frame)==(n==249));
    }
    unsigned first=0,end=0; assert(crop.range(first,end));
    assert(first==onsets[take]-3 && end==onsets[take]+durations[take]+3);
    if(report) printf("ENROLL take=%u samples=%u expected_features=%u gate=%u\n",take+1,(end-first)*320,((end-first)*320-512)/256+1,unsigned(crop.triggerRms()));
    return std::vector<int16_t>(recording.begin()+first*320,recording.begin()+end*320);
}
using Takes=std::vector<std::vector<int16_t>>;
static void prepare(ReferenceWake& wake,const Takes& takes) {
    const auto saved=storage.files;
    wake.beginEnrollment();
    for(unsigned take=0;take<3;++take) {
        assert(wake.submitEnrollment(takes[take].data(),takes[take].size())==(take==2 ? 2:1));
        assert(storage.files==saved);
    }
    assert(wake.enrollmentCandidateReady() && !wake.commitEnrollment());
    assert(storage.files==saved);
}
struct Detection { unsigned matches=0,best=2001,complete=0,firstMatchFrame=0; };
static Detection detect(ReferenceWake& wake,const std::vector<int16_t>& word,unsigned onset,bool distractor=false,
                        bool verification=false,bool allowMatch=true,unsigned frameLimit=850) {
    wake.resetDetection(); Detection result; int16_t frame[320];
    auto before=wake.diagnostics().complete;
    for(unsigned n=0;n<frameLimit;++n) {
        background(frame,22);
        if(n>=onset && n<onset+word.size()/320) {
            for(unsigned i=0;i<320;++i) frame[i]+=word[(n-onset)*320+i];
        }
        if(distractor && n>=250 && n<300) {
            for(unsigned i=0;i<320;++i) frame[i]=int16_t(500+1800*sin(6.283185307*880*(n*320+i)/16000));
        }
        const bool matched=verification ? wake.verifyEnrollmentWindow(frame,allowMatch && n>=50) : wake.detectWindow(frame);
        if(matched) { ++result.matches; if(!result.firstMatchFrame) result.firstMatchFrame=n+1; }
        if(wake.diagnostics().complete>before) {
            ++result.complete; before=wake.diagnostics().complete;
            result.best=std::min(result.best,wake.lastWindowProbe().currentDistance);
        }
        // Verification is latched by design. Stop at its first authorized match.
        if(verification && matched) break;
    }
    return result;
}
int main() {
    setvbuf(stdout,nullptr,_IONBF,0);
    const auto started=std::chrono::steady_clock::now();
    storage.files.clear();
    Takes takes,otherTakes;
    for(unsigned take=0;take<3;++take) { takes.push_back(enrollmentCrop(take)); otherTakes.push_back(enrollmentCrop(take,true,false)); }
    const auto positive=utterance(36,3100),negative=utterance(38,3100,true);
    // DSP has global lifecycle state. Simulate reboot by destroying the old
    // owner first, as happens on hardware, never by running concurrent engines.
    { ReferenceWake candidateOnly; assert(!candidateOnly.begin()); prepare(candidateOnly,takes); }
    { ReferenceWake reboot; assert(!reboot.begin() && !reboot.enrollmentCandidateReady() && storage.files.empty()); }
    {
        ReferenceWake registration; assert(!registration.begin());
        assert(registration.submitEnrollment(takes[0].data(),takes[0].size())<0 && storage.files.empty());
        assert(!registration.commitEnrollment());
        prepare(registration,takes);
        assert(!registration.registered());
        assert(registration.submitEnrollment(takes[0].data(),takes[0].size())<0);
        assert(registration.enrollmentCandidateReady() && storage.files.empty());
        assert(detect(registration,negative,100,false,true,true,1550).matches==0);
        assert(!registration.commitEnrollment() && storage.files.empty());
        assert(detect(registration,positive,100,false,true,false).matches==0);
        assert(!registration.commitEnrollment() && storage.files.empty());
        const auto normalMatches=registration.diagnostics().matched;
        auto confirmation=detect(registration,positive,100,false,true);
        assert(confirmation.matches==1 && confirmation.firstMatchFrame==300);
        assert(registration.diagnostics().matched==normalMatches);
        assert(!registration.registered() && storage.files.empty());
        registration.cancelEnrollment();
        assert(!registration.commitEnrollment() && storage.files.empty());
        prepare(registration,takes);
        assert(detect(registration,positive,100,false,true).matches==1);
        assert(registration.commitEnrollment() && !registration.enrollmentCandidateReady());
        const auto saved=storage.files;
        assert(!registration.commitEnrollment() && storage.files==saved);
        auto audit=registration.auditRegistration(); assert(audit.same && audit.generation==1);
        assert(audit.frames==49 && audit.slot==0);
        printf("PERSIST features=%u generation=%u slot=%u\n",audit.frames,audit.generation,audit.slot);
    }
    const auto persisted=storage.files;
    {
    ReferenceWake detection; assert(detection.begin() && detection.auditRegistration().same);
    unsigned passed=0,failed=0,maxBest=0;
    for(unsigned phase=0;phase<125;++phase) {
        auto result=detect(detection,positive,300+phase);
        if(result.matches) ++passed; else ++failed;
        maxBest=std::max(maxBest,result.best);
    }
    printf("PHASE_SWEEP positive=%u negative=%u worst_best_distance=%u\n",passed,failed,maxBest);
    assert(passed==125 && failed==0);
    for(unsigned onset:{300u,350u,424u}) assert(detect(detection,negative,onset).matches==0);
    puts("PASS: separate candidate confirmation, explicit commit, restart and 125 Ready phases; alternate rejected");
    unsigned verifiedPhases=0,maxVerificationMs=0;
    for(unsigned start:{50u,300u}) for(unsigned phase=0;phase<125;++phase) {
        prepare(detection,takes);
        const auto normalMatches=detection.diagnostics().matched;
        auto result=detect(detection,positive,start+phase,false,true,true,1550);
        assert(result.matches==1 && result.firstMatchFrame>=300);
        assert((result.firstMatchFrame-300)%125==0);
        const unsigned afterPromptMs=(result.firstMatchFrame-50)*20;
        assert(afterPromptMs<30000);
        maxVerificationMs=std::max(maxVerificationMs,afterPromptMs);
        assert(detection.diagnostics().matched==normalMatches && storage.files==persisted);
        ++verifiedPhases; detection.cancelEnrollment();
        assert(!detection.commitEnrollment() && detection.auditRegistration().same);
    }
    printf("VERIFY_PHASE_SWEEP passed=%u max_audio_ms_after_prompt=%u hardware_wall_time_verified=0\n",verifiedPhases,maxVerificationMs);
    // A different pending word must be checked against itself, never active RAM.
    prepare(detection,otherTakes);
    assert(detect(detection,positive,100,false,true,true,1550).matches==0);
    assert(!detection.commitEnrollment() && storage.files==persisted);
    assert(detect(detection,negative,100,false,true).matches==1);
    assert(detection.commitEnrollment());
    assert(detection.auditRegistration().same && detection.auditRegistration().generation==2);
    }
    {
        ReferenceWake reboot; const bool loaded=reboot.begin(); const auto audit=reboot.auditRegistration();
        printf("REENROLL_RESTART registered=%u same=%u generation=%u slot=%u\n",unsigned(loaded),unsigned(audit.same),audit.generation,audit.slot);
        assert(loaded && audit.generation==2);
        assert(detect(reboot,negative,350).matches>0 && detect(reboot,positive,350).matches==0);
    }
    // Restore the in-memory test fixture only; no actual device/file is involved.
    storage.files=persisted;
    ReferenceWake original; assert(original.begin());
    for(unsigned scenario=0;scenario<6;++scenario) {
        prepare(original,takes);
        uint32_t random=12; float filtered=0; int16_t frame[320];
        for(unsigned n=0;n<1550;++n) {
            background(frame,22);
            for(unsigned i=0;i<320;++i) {
                random=random*1664525u+1013904223u;
                const float noise=(int(random>>16)-32768)/32768.f;
                float value=0;
                if(scenario<5 && n>=300 && n<375) {
                    value=noise*1800;
                    if(scenario==1) value=1600*sinf(6.2831853f*150*(n*320+i)/16000);
                    if(scenario==2) value=1800*sinf(6.2831853f*880*(n*320+i)/16000);
                    if(scenario==3) { filtered=filtered*.95f+noise*.05f; value=filtered*8000; }
                    if(scenario==4) value=noise*(n%10<3 ? 3000:0);
                }
                frame[i]+=int16_t(value);
            }
            assert(!original.verifyEnrollmentWindow(frame,n>=50));
        }
        assert(!original.commitEnrollment() && storage.files==persisted);
        original.cancelEnrollment();
    }
    puts("PASS: candidate-specific re-enrollment/restart and six negative fixtures across 30 seconds of audio after prompt");
    // Characterizations below describe limitations, not product acceptance tests.
    for(unsigned onset:{0u,25u,50u,100u,150u}) {
        auto r=detect(original,positive,onset);
        printf("STARTUP_CHARACTERIZATION onset=%u matched_windows=%u candidates=%u best_distance=%u\n",onset,r.matches,r.complete,r.best);
    }
    auto distracted=detect(original,positive,350,true);
    printf("DISTRACTOR_CHARACTERIZATION matched_windows=%u candidates=%u best_distance=%u\n",distracted.matches,distracted.complete,distracted.best);
    const auto quietPositive=utterance(36,1200);
    auto quietClean=detect(original,quietPositive,350,false);
    auto quietDistracted=detect(original,quietPositive,350,true);
    printf("BACKGROUND_CHARACTERIZATION clean_matches=%u distracted_matches=%u clean_distance=%u distracted_distance=%u\n",quietClean.matches,quietDistracted.matches,quietClean.best,quietDistracted.best);
    assert(quietClean.matches>0);
    assert(storage.files==persisted && original.auditRegistration().same);
    puts("PASS: detection and restart preserve all persisted bytes; characterization results are not real-voice validation");
    printf("HOST_ELAPSED_MS=%lld hardware_deadline_proof=0\n",(long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count());
}
