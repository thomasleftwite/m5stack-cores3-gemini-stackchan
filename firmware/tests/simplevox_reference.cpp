#include <simplevox.h>
#include "SyntheticSpeech.h"
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <cmath>
#include "../src/ReferenceWake.h"
#include <LittleFS.h>
static int16_t frame[320];
static std::vector<int16_t> word(bool alternate=false,float gain=1,unsigned frames=48) {
    std::vector<int16_t> pcm(frames*320);
    for(unsigned n=0;n<frames;++n) {
        syntheticSpeechFrame(frame,n*30/frames,3000,alternate);
        for(unsigned i=0;i<320;++i) pcm[n*320+i]=int16_t(frame[i]*gain);
    }
    return pcm;
}
static void verifyCandidate(ReferenceWake& wake,const std::vector<int16_t>& pcm) {
    assert(wake.enrollmentCandidateReady());
    assert(!wake.commitEnrollment()); // Three mutually consistent takes are insufficient.
    bool verified=false;
    for(unsigned n=0;n<600 && !verified;++n) {
        if(n>=300 && n<300+pcm.size()/320)
            std::copy(pcm.begin()+(n-300)*320,pcm.begin()+(n-299)*320,frame);
        else std::fill(frame,frame+320,int16_t(500));
        verified=wake.verifyEnrollmentWindow(frame,n>=300);
    }
    assert(verified);
}
int main() {
    EnrollmentWindow quality;
    for(unsigned i=0;i<50;++i) quality.backgroundLevel(i<20 ? 2000.f : 20.f);
    assert(quality.backgroundRms()==2000 && quality.warmedBackgroundRms()==20 && quality.triggerRms()>2000);
    puts("PASS: diagnostics distinguish microphone warmup from later measured background without changing the gate");
    simplevox::MfccEngine engine;
    assert(engine.init(simplevox::MfccConfig{}));
    assert(engine.bufferAlignment()==0);
    auto a=word(),quiet=word(false,.4f),b=word(true);
    std::unique_ptr<simplevox::MfccFeature> first(engine.create(a.data(),int(a.size()))),
        same(engine.create(a.data(),int(a.size()))),weak(engine.create(quiet.data(),int(quiet.size()))),
        other(engine.create(b.data(),int(b.size())));
    assert(first && same && weak && other);
    auto self=simplevox::calcDTW(*first,*same),level=simplevox::calcDTW(*first,*weak),different=simplevox::calcDTW(*first,*other);
    printf("REFERENCE: self=%u lower_level=%u alternate=%u frames=%d\n",self,level,different,first->size());
    assert(self<5 && level<250);
    auto longPcm=word(false,1,120);
    std::unique_ptr<simplevox::MfccFeature> longer(engine.create(longPcm.data(),int(longPcm.size())));
    auto slow=simplevox::calcDTW(*first,*longer);
    printf("REFERENCE: 2.5x duration=%u\n",slow); assert(slow<250);
    float silentMfcc[12]; int16_t silence[512]={};
    engine.calculate(silence,silentMfcc);
    for(float value:silentMfcc) assert(std::isfinite(value));
    engine.deinit();
    std::vector<int16_t> pa(54*320,500),pb(54*320,500);
    std::copy(a.begin(),a.end(),pa.begin()+960); std::copy(b.begin(),b.end(),pb.begin()+960);
    storage.files["/wake-mfcc-v2-a.bin"]={1,2,3};
    auto legacy=storage.files["/wake-mfcc-v2-a.bin"];
    {
        ReferenceWake wake; assert(!wake.begin()); wake.beginEnrollment();
        ReferenceWake::EngineProbe probe;
        assert(wake.probeEngine(probe));
        printf("ENGINE PROBE: self=%u level=%u other=%u\n",probe.same,probe.level,probe.other);
        assert(probe.same<5 && probe.level<250 && probe.other>=250);
        ReferenceWake::EngineProbe integerProbe,portableProbe,repeatProbe;
        assert(wake.probeEngine(integerProbe,true,false) && wake.probeEngine(portableProbe,true,true) && wake.probeEngine(repeatProbe,true,false));
        printf("INTEGER PROBE: opt=%u portable=%u raw=%u raw_other=%u feature=%u feature_other=%u\n",integerProbe.other,portableProbe.other,
            integerProbe.raw,integerProbe.rawOther,integerProbe.feature,integerProbe.featureOther);
        assert(integerProbe.raw==portableProbe.raw && integerProbe.rawOther==portableProbe.rawOther);
        assert(integerProbe.feature==portableProbe.feature && integerProbe.featureOther==portableProbe.featureOther);
        assert(integerProbe.feature==repeatProbe.feature && integerProbe.featureOther==repeatProbe.featureOther);
        assert(integerProbe.same<5 && integerProbe.level<250 && integerProbe.other>=250);
        auto before=storage.files;
        assert(wake.submitEnrollment(pa.data(),pa.size())==1 && storage.files==before);
        assert(wake.submitEnrollment(pa.data(),pa.size())==1 && storage.files==before);
        assert(wake.submitEnrollment(pa.data(),pa.size())==2 && !wake.registered() && storage.files==before);
        verifyCandidate(wake,a);
        assert(storage.files==before && wake.commitEnrollment() && wake.registered());
        auto saved=storage.files;
        auto audit=wake.auditRegistration();
        assert(audit.same && audit.ramValid && audit.fileValid && audit.frames==66 && audit.generation==1 && audit.slot==0);
        storage.files["/wake-simplevox-v3-a.bin"].back()^=1;
        audit=wake.auditRegistration();
        assert(audit.ramValid && !audit.fileValid && !audit.same && wake.registered());
        storage.files=saved;
        assert(wake.auditRegistration().same);
        wake.beginEnrollment(); wake.submitEnrollment(pa.data(),pa.size()); wake.submitEnrollment(pb.data(),pb.size());
        assert(wake.submitEnrollment(pa.data(),pa.size())==-1 && storage.files==saved && wake.registered());
        for(bool alternate:{false,true}) {
            wake.resetDetection(); unsigned matches=0;
            for(unsigned n=0;n<600;++n) {
                if(n>=300 && n<348) std::copy((alternate ? b:a).begin()+(n-300)*320,(alternate ? b:a).begin()+(n-299)*320,frame);
                else std::fill(frame,frame+320,int16_t(500));
                if(wake.detectWindow(frame)) {
                    const auto& probe=wake.lastWindowProbe();
                    assert(probe.first==122 && probe.end==176);
                    assert(probe.templateFrames==66 && probe.candidateFrames==66);
                    assert(probe.noise==0 && probe.warm==0 && probe.gate==100);
                    assert(probe.rms>0 && probe.peak>=probe.rms && probe.dc>=500 && probe.clips==0);
                    ++matches; wake.resetDetection();
                }
            }
            assert(matches==(alternate ? 0u:1u));
        }
        puts("PASS: rolling-window diagnostics describe the actual wrapped PCM crop and saved template without changing its decision");
        for(unsigned scenario=0;scenario<5;++scenario) {
            wake.resetDetection(); uint32_t random=12; float filtered=0;
            for(unsigned n=0;n<600;++n) {
                for(unsigned i=0;i<320;++i) {
                    random=random*1664525u+1013904223u;
                    float noise=(int(random>>16)-32768)/32768.f,value=0;
                    if(n>=300 && n<375) {
                        value=noise*1800;
                        if(scenario==1) value=1600*sinf(6.2831853f*150*(n*320+i)/16000);
                        if(scenario==2) value=1800*sinf(6.2831853f*880*(n*320+i)/16000);
                        if(scenario==3) { filtered=filtered*.95f+noise*.05f; value=filtered*8000; }
                        if(scenario==4) value=noise*(n%10<3 ? 3000 : 0);
                    }
                    frame[i]=int16_t(value);
                }
                assert(!wake.detectWindow(frame));
            }
        }
        puts("PASS: reference detector rejects five synthetic nonspeech conditions");
        wake.beginEnrollment(); wake.submitEnrollment(pb.data(),pb.size()); wake.submitEnrollment(pb.data(),pb.size());
        assert(wake.submitEnrollment(pb.data(),pb.size())==2);
        verifyCandidate(wake,b);
        storage.shortWrite=true;
        assert(!wake.commitEnrollment() && wake.registered()); storage.shortWrite=false;
        wake.beginEnrollment(); wake.submitEnrollment(pb.data(),pb.size()); wake.submitEnrollment(pb.data(),pb.size());
        assert(wake.submitEnrollment(pb.data(),pb.size())==2);
        verifyCandidate(wake,b);
        storage.failRename=true; assert(!wake.commitEnrollment()); storage.failRename=false;
        wake.beginEnrollment(); wake.submitEnrollment(pb.data(),pb.size()); wake.submitEnrollment(pb.data(),pb.size());
        assert(wake.submitEnrollment(pb.data(),pb.size())==2);
        verifyCandidate(wake,b); assert(wake.commitEnrollment());
        auto twoGenerations=storage.files;
        wake.resetDetection(); bool previousMatched=false;
        for(unsigned n=0;n<600;++n) {
            if(n>=300 && n<348) std::copy(a.begin()+(n-300)*320,a.begin()+(n-299)*320,frame);
            else std::fill(frame,frame+320,int16_t(500));
            assert(!wake.detectWindow(frame));
            const auto& comparison=wake.lastWindowProbe();
            if(comparison.previousAvailable && comparison.previousDistance<250) {
                assert(comparison.currentDistance>=250); previousMatched=true;
            }
        }
        assert(previousMatched && storage.files==twoGenerations);
        puts("PASS: same-input comparison finds the previous word without activating it or changing either saved generation");
        storage.files["/wake-simplevox-v3-b.bin"].back()^=1; // Invalidate latest; old slot must survive.
        assert(storage.files["/wake-mfcc-v2-a.bin"]==legacy);
        puts("PASS: reference 3-take validation/medoid, real window detector, other-word rejection, failed publication and legacy-file preservation");
    }
    {
        ReferenceWake reboot; assert(reboot.begin() && reboot.registered());
        auto audit=reboot.auditRegistration();
        assert(audit.same && audit.frames==66 && audit.slot==0 && audit.generation==1);
        unsigned matches=0;
        for(unsigned n=0;n<600;++n) {
            if(n>=300 && n<348) std::copy(a.begin()+(n-300)*320,a.begin()+(n-299)*320,frame);
            else std::fill(frame,frame+320,int16_t(500));
            if(reboot.detectWindow(frame)) { ++matches; reboot.resetDetection(); }
        }
        assert(matches==1 && reboot.auditRegistration().same);
    }
    puts("PASS: persisted registration detects its enrolled word after restart and CRC fallback; RAM/file audit detects corruption without writing storage");
    storage.files.clear();
    decltype(storage.files) publicationBaseline;
    {
        ReferenceWake wake; assert(!wake.begin());
        auto empty=storage.files;
        assert(wake.submitEnrollment(pa.data(),pa.size())==-1 && storage.files==empty);
        assert(!wake.commitEnrollment());
        auto prepare=[&](const std::vector<int16_t>& capture) {
            const uint32_t previousSession=wake.enrollmentSession();
            wake.beginEnrollment(); assert(wake.enrollmentSession()!=previousSession);
            assert(wake.submitEnrollment(capture.data(),capture.size())==1);
            assert(wake.submitEnrollment(capture.data(),capture.size())==1);
            assert(wake.submitEnrollment(capture.data(),capture.size())==2);
            assert(wake.enrollmentCandidateReady());
            assert(wake.submitEnrollment(capture.data(),capture.size())==-1);
            assert(wake.enrollmentCandidateReady()); // Late submit cannot replace a verified candidate.
        };
        prepare(pa);
        verifyCandidate(wake,a);
        assert(wake.diagnostics().matched==0); // Enrollment confirmation is not a normal wake.
        wake.resetDetection(); assert(!wake.commitEnrollment());
        verifyCandidate(wake,a); assert(wake.commitEnrollment());
        assert(!wake.commitEnrollment());
        auto saved=storage.files;
        publicationBaseline=saved;
        prepare(pb); verifyCandidate(wake,b); wake.cancelEnrollment();
        assert(!wake.commitEnrollment() && storage.files==saved && wake.auditRegistration().same);
    }
    for(unsigned fault=0;fault<3;++fault) {
        storage.files=publicationBaseline;
        {
            ReferenceWake wake; assert(wake.begin()); wake.beginEnrollment();
            assert(wake.submitEnrollment(pb.data(),pb.size())==1);
            assert(wake.submitEnrollment(pb.data(),pb.size())==1);
            assert(wake.submitEnrollment(pb.data(),pb.size())==2);
            verifyCandidate(wake,b);
            storage.failReadPath=fault==0 ? "/wake-simplevox-v3.tmp" : "/wake-simplevox-v3-b.bin";
            if(fault==2) storage.failRemovePath="/wake-simplevox-v3-b.bin";
            assert(!wake.commitEnrollment() && wake.registered());
            assert(wake.publicationUncertain()==(fault==2));
            storage.failReadPath.clear(); storage.failRemovePath.clear();
        }
        ReferenceWake reboot; assert(reboot.begin());
        auto audit=reboot.auditRegistration();
        assert(audit.same && audit.generation==(fault==2 ? 2u : 1u));
        assert(audit.slot==(fault==2 ? 1u : 0u));
    }
    puts("PASS: explicit sessions, candidate-only verification, duplicate/cancel guards, read-failure rollback and uncertain publication match reboot selection");
}
