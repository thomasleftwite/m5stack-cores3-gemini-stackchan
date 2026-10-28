#include "../src/SpeechDetector.h"
#include "../src/VoiceActivity.h"
#include "../src/LocalWakeCore.h"
#include <stdio.h>
#include "SyntheticSpeech.h"
static int16_t samples[320];
static float rms() {
    double sum = 0; for (int i = 0; i < 320; ++i) sum += double(samples[i])*samples[i];
    return sqrtf(float(sum / 320));
}
int main() {
    bool all = true;
    for (int scenario = 0; scenario < 5; ++scenario) {
        VoiceActivity level; SpeechDetector speech;
        if (!speech.begin()) return 2;
        localwake::Segmenter oldSegment, guarded;
        unsigned oldComplete = 0, guardedComplete = 0, activeFrames = 0;
        uint32_t random = 12;
        float filteredNoise = 0;
        for (int n = 0; n < 340; ++n) {
            bool burst = n >= 150 && n < 225;
            for (int i = 0; i < 320; ++i) {
                random = random * 1664525u + 1013904223u;
                float noise = (int(random >> 16) - 32768) / 32768.f;
                float value = noise * (burst ? 1800 : 20);
                if (scenario == 1) value = (burst ? 1600 : 20) * sinf(6.2831853f * 150 * (n*320+i) / 16000);
                if (scenario == 2) value = (burst ? 1800 : 20) * sinf(6.2831853f * 880 * (n*320+i) / 16000);
                if (scenario == 3) { filteredNoise = filteredNoise*.95f + noise*.05f; value = filteredNoise * (burst ? 8000 : 20); }
                if (scenario == 4) value = noise * (burst && n % 10 < 3 ? 3000 : 20);
                samples[i] = int16_t(value);
            }
            bool high = level.update(rms());
            int result = speech.update(samples, high);
            if (result < 0) return 3;
            activeFrames += speech.accepted();
            auto oldResult = oldSegment.push(samples, high), guardedResult = guarded.push(samples, speech.accepted());
            oldComplete += oldResult == localwake::Segmenter::Complete;
            guardedComplete += guardedResult == localwake::Segmenter::Complete && guarded.speechEvidence();
            if (oldResult != localwake::Segmenter::None) oldSegment.reset();
            if (guardedResult != localwake::Segmenter::None) guarded.reset();
        }
        bool pass = oldComplete > 0 && guardedComplete == 0;
        printf("%s: nonspeech scenario %d is not an enrollment take (old=%u guarded=%u voice_frames=%u)\n", pass ? "PASS" : "FAIL", scenario, oldComplete, guardedComplete, activeFrames);
        all &= pass;
    }
    VoiceActivity level; SpeechDetector speech; speech.begin();
    unsigned voices = 0;
    localwake::Segmenter spoken;
    bool acceptedWord = false;
    for (int n = 0; n < 260; ++n) {
        for (int i = 0; i < 320; ++i) {
            double value = 0;
            if (n >= 150 && n < 210) {
                int vowel = (n - 150) / 20;
                const float f1[] = {600,300,800}, f2[] = {1400,2200,1100};
                for (int h = 1; h <= 32; ++h) {
                    float hz = 120.f * h, a = (hz-f1[vowel])/120, b = (hz-f2[vowel])/180;
                    value += 5000 * (expf(-.5f*a*a)+.6f*expf(-.5f*b*b))/sqrtf(float(h))*sinf(6.2831853f*hz*(n*320+i)/16000);
                }
            }
            samples[i] = int16_t(value);
        }
        bool high = level.update(rms()); speech.update(samples, high); voices += speech.accepted();
        auto result = spoken.push(samples, speech.accepted());
        if (result == localwake::Segmenter::Complete) acceptedWord |= spoken.speechEvidence();
        if (result != localwake::Segmenter::None) spoken.reset();
    }
    printf("%s: synthetic voiced vowel sequence survives the real classifier (frames=%u)\n", voices >= 10 ? "PASS" : "FAIL", voices);
    printf("%s: voiced vowel sequence meets complete enrollment evidence\n", acceptedWord ? "PASS" : "FAIL");
    // Numeric field observation was voice=12/shape=12 for the requested word,
    // versus voice=11/shape=8 for an earlier unidentified sound. Reproduce the
    // admission boundary with synthetic PCM, without pretending it is a replay.
    for (unsigned shaped = 8; shaped <= 12; shaped += 4) {
        localwake::Segmenter boundary;
        for (unsigned n=0;n<32;++n) {
            if (n < shaped) syntheticSpeechFrame(samples, n*2, 5000);
            else for (int i=0;i<320;++i) samples[i]=0;
            boundary.push(samples, n < 12);
        }
        bool expected = shaped == 12;
        bool pass = boundary.speechEvidence() == expected;
        printf("%s: 12 voiced frames with %u shaped frames have evidence=%d (expected=%d)\n", pass ? "PASS" : "FAIL", unsigned(boundary.shapedFrames()), boundary.speechEvidence(), expected);
        all &= pass;
    }
    return all && voices >= 10 && acceptedWord ? 0 : 1;
}
