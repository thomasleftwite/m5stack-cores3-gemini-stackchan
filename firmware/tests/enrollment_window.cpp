#include "../src/EnrollmentWindow.h"
#include "../src/LocalWake.h"
#include "SyntheticSpeech.h"
#include <LittleFS.h>
#include <assert.h>
#include <stdio.h>
static int16_t pcm[250*320], frame[320];
static localwake::Features features(bool alternate=false) {
    localwake::Features out; out.count=36;
    for (unsigned n=0;n<36;++n) {
        if (n>=3 && n<33) syntheticSpeechFrame(frame,n-3,3000,alternate);
        else std::fill(frame,frame+320,int16_t(0));
        localwake::extract(frame,out.frames[n]);
    }
    return out;
}
static void background(EnrollmentWindow& w) {
    w.reset();
    for (unsigned n=0;n<50;++n) {
        for (unsigned i=0;i<320;++i) frame[i]=int16_t(500+(i%2 ? 20 : -20));
        assert(w.background(frame)==(n==49));
    }
}
int main() {
    EnrollmentWindow w;
    for (int scenario=0;scenario<4;++scenario) {
        background(w); unsigned first=0,end=0;
        for (unsigned n=0;n<250;++n) {
            if (scenario==1 && n>=100 && n<130) syntheticSpeechFrame(frame,n-100,3000);
            else for (unsigned i=0;i<320;++i) frame[i]=int16_t(500+(i%2 ? 20 : -20));
            if (scenario==2 && n==100) for (unsigned i=0;i<320;++i) frame[i]=i%2 ? 3000 : -3000;
            if (scenario==3 && n>=20 && n<240) syntheticSpeechFrame(frame,n%30,3000);
            std::copy(frame,frame+320,pcm+n*320);
            assert(w.capture(frame)==(n==249));
            if (n<249) assert(!w.range(first,end));
        }
        bool ok=w.range(first,end);
        assert(ok==(scenario==1));
        if (ok) assert(first==97 && end==133);
    }
    puts("PASS: frozen pre-beep background, DC removal, full 5-second collection, silence/click/overlong rejection and padded word crop");
    background(w);
    for (unsigned n=0;n<250;++n) {
        bool word=(n>=20 && n<30) || (n>=100 && n<130);
        if(word) syntheticSpeechFrame(frame,n%30,3000);
        else std::fill(frame,frame+320,int16_t(0));
        w.capture(frame);
    }
    unsigned first=0,end=0; assert(w.range(first,end) && first==97 && end==133);
    puts("PASS: a short earlier sound cannot displace the longer spoken interval");
    // One utterance with a 300 ms internal pause must retain both syllables.
    background(w);
    for(unsigned n=0;n<250;++n) {
        if(n>=100 && n<110) syntheticSpeechFrame(frame,n-100,3000);
        else if(n>=125 && n<145) syntheticSpeechFrame(frame,n-115,3000);
        else std::fill(frame,frame+320,int16_t(500));
        w.capture(frame);
    }
    assert(w.range(first,end) && first==97 && end==148);
    puts("PASS: a 300 ms inter-syllable pause preserves the whole registered utterance");
    auto a=features(), b=features(true);
    LocalWake wake; wake.begin(); wake.beginEnrollment();
    auto old=storage.files;
    assert(wake.submitEnrollment(a)==1 && wake.enrollmentCount()==1 && storage.files==old);
    assert(wake.submitEnrollment(a)==1 && wake.enrollmentCount()==2 && storage.files==old);
    assert(wake.submitEnrollment(a)==2 && wake.registered() && wake.enrollmentCount()==0);
    bool detected=false;
    for (unsigned n=0;n<30;++n) {
        syntheticSpeechFrame(frame,n,3000);
        detected |= wake.detect(frame,true);
    }
    std::fill(frame,frame+320,int16_t(0));
    for (unsigned n=0;n<20;++n) detected |= wake.detect(frame,false);
    assert(detected);
    puts("PASS: padded fixed-window enrollment matches an unpadded Ready utterance");
    // Deliberately withhold classifier approval: compare identical PCM through
    // the normal call site and an independent energy-only diagnostic path.
    bool classifierMatched=false; int energyMatches=0;
    for (unsigned n=0;n<50;++n) {
        if(n<30) syntheticSpeechFrame(frame,n,3000);
        else std::fill(frame,frame+320,int16_t(0));
        classifierMatched |= wake.detect(frame,false);
        energyMatches=std::max(energyMatches,wake.probeEnergy(frame,n<30,1));
    }
    assert(!classifierMatched && energyMatches==3);
    for (unsigned n=0;n<50;++n) {
        if(n<30) syntheticSpeechFrame(frame,n,3000,true);
        else std::fill(frame,frame+320,int16_t(0));
        assert(wake.probeEnergy(frame,n<30,1)<2);
    }
    assert(storage.files.size()==1);
    puts("PASS: energy probe isolates a classifier-rejected saved word without accepting the other word or publishing storage");
    auto beforeProbe=storage.files;
    for(bool alternate : {false,true}) {
        wake.resetDetection();
        int windowsMatched=0;
        for(unsigned n=0;n<600;++n) {
            if(n>=300 && n<330) syntheticSpeechFrame(frame,n-300,3000,alternate);
            else std::fill(frame,frame+320,int16_t(500));
            windowsMatched=std::max(windowsMatched,wake.probeWindow(frame));
        }
        assert(windowsMatched==(alternate ? 0 : 3));
        assert(storage.files==beforeProbe);
    }
    RollingWakeWindow rolling; localwake::Features crop;
    for(unsigned n=0;n<425;++n) {
        if(n>=300 && n<330) syntheticSpeechFrame(frame,n-300,3000);
        else std::fill(frame,frame+320,int16_t(500));
        int result=rolling.push(frame,crop);
        if(n<299 || (n!=299 && n!=424)) assert(result==-1);
        if(n==299) assert(result==0);
        if(n==424) assert(result==1 && crop.count==a.count && memcmp(crop.frames,a.frames,a.count*sizeof(a.frames[0]))==0);
    }
    puts("PASS: overlapping Ready windows reproduce enrollment crop/MFCC exactly, detect the saved word, reject another word and never publish storage");
    for(bool alternate : {false,true}) {
        wake.resetDetection(); unsigned activations=0;
        for(unsigned n=0;n<600;++n) {
            if(n>=300 && n<330) syntheticSpeechFrame(frame,n-300,3000,alternate);
            else std::fill(frame,frame+320,int16_t(500));
            if(wake.detectWindow(frame)) { ++activations; wake.resetDetection(); }
        }
        assert(activations==(alternate ? 0u : 1u));
    }
    for(unsigned scenario=0;scenario<5;++scenario) {
        wake.resetDetection(); uint32_t random=12; float filtered=0;
        for(unsigned n=0;n<600;++n) {
            for(unsigned i=0;i<320;++i) {
                random=random*1664525u+1013904223u;
                float noise=(int(random>>16)-32768)/32768.f;
                float value=0;
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
    assert(storage.files==beforeProbe);
    puts("PASS: adopted window detector triggers once per word, rejects another word and five nonspeech scenarios without changing registration");
    auto saved=storage.files;
    wake.beginEnrollment(); assert(wake.submitEnrollment(b)==1);
    assert(wake.submitEnrollment(a)==1 && wake.enrollmentCount()==2 && storage.files==saved);
    assert(wake.submitEnrollment(a)==-1 && wake.enrollmentCount()==0 && storage.files==saved && wake.registered());
    puts("PASS: stage three before comparison; mismatch discards all three without changing active storage");
    wake.beginEnrollment(); assert(wake.submitEnrollment(b)==1); wake.cancelEnrollment();
    assert(wake.enrollmentCount()==0 && storage.files==saved);
    wake.beginEnrollment(); wake.submitEnrollment(b); wake.submitEnrollment(b);
    storage.shortWrite=true; assert(wake.submitEnrollment(b)==-2); storage.shortWrite=false;
    LocalWake reboot; assert(reboot.begin());
    puts("PASS: cancellation and failed fixed-window publication retain the saved registration");
}
