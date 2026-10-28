#include "../src/VoiceActivity.h"
#include "../src/LocalWake.h"
#include "../src/MicrophoneLevel.h"
#include "SyntheticSpeech.h"
#include <LittleFS.h>
#include <stdio.h>
static int16_t frame[320], silence[320] = {};
static void tone(float peak, float frequency = 800) {
    for (int i = 0; i < 320; ++i) frame[i] = int16_t(peak * sinf(6.2831853f * frequency * i / 16000));
}
static void word(int index, float peak = 226) { syntheticSpeechFrame(frame, unsigned(index), peak); }
static void harmonicWord(localwake::Features& out, float pitch, bool reverse = false, float noise = 0) {
    const float first[] = {600, 300, 800}, second[] = {1400, 2200, 1100};
    out.count = 60;
    uint32_t random = uint32_t(pitch * 100);
    for (int n = 0; n < 60; ++n) {
        int vowel = reverse ? 2 - n / 20 : n / 20;
        for (int i = 0; i < 320; ++i) {
            float value = 0;
            for (int h = 1; h <= 32; ++h) {
                float frequency = pitch * h;
                float a = (frequency - first[vowel]) / 120, b = (frequency - second[vowel]) / 180;
                float amplitude = (expf(-.5f*a*a) + .6f*expf(-.5f*b*b)) / sqrtf(float(h));
                value += amplitude * sinf(6.2831853f * frequency * (n * 320 + i) / 16000);
            }
            random = random * 1664525u + 1013904223u;
            frame[i] = int16_t(value * 3000 + noise * (float(random >> 16) / 32768 - 1));
        }
        localwake::extract(frame, out.frames[n]);
    }
}
int main() {
    localwake::Segmenter levelCheck;
    for (int n = 0; n < 30; ++n) {
        for (int i = 0; i < 320; ++i) {
            // M5Unified 0.1.17 applies magnification/(oversampling*2),
            // then saturates its int16 output. Oversampling=1 fixture.
            float value = 6000 * sinf(6.2831853f * 800 * i / 16000) * microphoneMagnification(80) / 2;
            frame[i] = int16_t(fmaxf(-32768, fminf(32767, value)));
        }
        levelCheck.push(frame, true);
    }
    localwake::Segmenter::Result levelResult = localwake::Segmenter::None;
    for (int i = 0; i < 20; ++i) levelResult = levelCheck.push(silence, false);
    bool levelOk = levelResult == localwake::Segmenter::Complete;
    printf("%s: microphone amplification avoids saturation rejection on the same synthetic speech level\n", levelOk ? "PASS" : "FAIL");
    if (!levelOk) return 1;
    static localwake::Features harmonicA, harmonicB, centeredA, centeredB, meanB, reversedWord;
    harmonicWord(harmonicA, 120, false, 100); harmonicWord(harmonicB, 135, false, 300);
    localwake::prepareForMatch(harmonicA, centeredA); localwake::prepareForMatch(harmonicB, centeredB);
    localwake::stationary(centeredB, meanB);
    bool pitchOk = localwake::compareWord(centeredB, centeredA, meanB).matches(32);
    printf("%s: same synthetic vowel sequence tolerates a 12.5 percent voice-pitch change\n", pitchOk ? "PASS" : "FAIL");
    if (!pitchOk) return 1;
    auto texture = [](localwake::Features& out, uint32_t seed) {
        out.count = 60;
        for (int n = 0; n < 60; ++n) for (size_t k = 0; k < localwake::Dimensions; ++k) {
            seed = seed * 1664525u + 1013904223u;
            out.frames[n][k] = int16_t((k % 3 == size_t(n / 20) ? 20 : -10) + int(seed >> 24) % 81 - 40);
        }
    };
    texture(harmonicA, 1); texture(harmonicB, 42);
    localwake::prepareForMatch(harmonicA, centeredA); localwake::prepareForMatch(harmonicB, centeredB); localwake::stationary(centeredB, meanB);
    auto textured = localwake::compareWord(centeredB, centeredA, meanB);
    bool textureOk = textured.matches(32);
    printf("%s: repeated phonetic trajectory tolerates independent fast frame variation (synthetic cost=%.1f baseline=%.1f)\n", textureOk ? "PASS" : "FAIL", textured.actual, textured.stationary);
    if (!textureOk) return 1;
    reversedWord = harmonicB;
    for (int i = 0; i < 60; ++i) memcpy(reversedWord.frames[i], harmonicB.frames[59 - i], sizeof(reversedWord.frames[i]));
    localwake::prepareForMatch(reversedWord, centeredB); localwake::stationary(centeredB, meanB);
    bool noisyReverseRejected = !localwake::compareWord(centeredB, centeredA, meanB).matches(32);
    printf("%s: smoothing still rejects the reversed noisy phonetic sequence\n", noisyReverseRejected ? "PASS" : "FAIL");
    if (!noisyReverseRejected) return 1;
    harmonicA.count = harmonicB.count = 60;
    for (int i = 0; i < 60; ++i) {
        int a = i < 5 ? -60 : i < 40 ? 0 : i < 55 ? 60 : 0;
        int b = i < 5 ? -60 : i < 10 ? 0 : i < 25 ? 60 : 0;
        for (size_t k = 0; k < localwake::Dimensions; ++k) {
            harmonicA.frames[i][k] = k % 2 ? -a : a;
            harmonicB.frames[i][k] = k % 2 ? -b : b;
        }
    }
    localwake::prepareForMatch(harmonicA, centeredA); localwake::prepareForMatch(harmonicB, centeredB); localwake::stationary(centeredB, meanB);
    bool timingRejected = !localwake::compareWord(centeredB, centeredA, meanB).matches(32);
    printf("%s: unrelated relative phoneme timing cannot be warped into the stored word\n", timingRejected ? "PASS" : "FAIL");
    if (!timingRejected) return 1;
    localwake::Features changing, flat;
    changing.count = 30;
    for (int i = 0; i < 30; ++i) {
        int value = i < 10 ? -60 : i < 20 ? 60 : 0;
        changing.frames[i][0] = value; changing.frames[i][1] = -value;
    }
    localwake::stationary(changing, flat);
    bool sequence = localwake::compareWord(changing, changing, flat).matches(12)
        && !localwake::compareWord(flat, changing, flat).matches(12);
    printf("%s: constant average spectrum cannot impersonate the word's temporal pattern\n", sequence ? "PASS" : "FAIL");
    if (!sequence) return 1;
    localwake::Features varied = changing, variedMean, reverse = changing, reversedMean, stretched;
    for (int i = 0; i < 30; ++i) {
        for (size_t k = 0; k < localwake::Dimensions; ++k) {
            varied.frames[i][k] += int16_t(i % 3 - 1);
            reverse.frames[i][k] = changing.frames[29 - i][k];
        }
    }
    localwake::stationary(varied, variedMean); localwake::stationary(reverse, reversedMean);
    stretched.count = 60;
    for (int i = 0; i < 60; ++i) memcpy(stretched.frames[i], changing.frames[i / 2], sizeof(stretched.frames[i]));
    localwake::Features stretchedMean; localwake::stationary(stretched, stretchedMean);
    bool ordered = localwake::compareWord(varied, changing, variedMean).matches(12)
        && localwake::compareWord(stretched, changing, stretchedMean).matches(12)
        && !localwake::compareWord(reverse, changing, reversedMean).matches(12);
    printf("%s: changing word tolerates timing/feature variation but rejects its reversed sequence\n", ordered ? "PASS" : "FAIL");
    if (!ordered) return 1;
    localwake::Features colored = varied, normalized, normalizedMean;
    for (int i = 0; i < colored.count; ++i)
        for (size_t k = 0; k < localwake::Dimensions; ++k) colored.frames[i][k] += int16_t((int(k) % 3 - 1) * 90);
    localwake::stationary(colored, variedMean);
    bool rawMatch = localwake::compareWord(colored, changing, variedMean).matches(12);
    localwake::prepareForMatch(colored, normalized); localwake::stationary(normalized, normalizedMean);
    bool normalizedMatch = localwake::compareWord(normalized, changing, normalizedMean).matches(12);
    bool coloration = !rawMatch && !normalizedMatch;
    printf("%s: persistent word timbre is retained instead of erased (raw=%d prepared=%d)\n", coloration ? "PASS" : "FAIL", int(rawMatch), int(normalizedMatch));
    if (!coloration) return 1;
    LocalWake wake; wake.begin(); wake.beginEnrollment(); tone(226);
    for (int take = 0; take < 3; ++take) {
        for (int i = 0; i < 30; ++i) { word(i); wake.enroll(frame, true); }
        int result = 0;
        for (int i = 0; i < 20; ++i) result = wake.enroll(silence, false);
        if (result != (take == 2 ? 2 : 1)) return 2;
    }
    VoiceActivity activity;
    unsigned quietTriggers = 0;
    for (int i = 0; i < 200; ++i) quietTriggers += activity.update(80);
    unsigned loudFrames = 0, weakFrames = 0; bool matched = false;
    tone(566);
    for (int i = 0; i < 5; ++i) { word(i, 566); bool v = activity.update(400); loudFrames += v; matched |= wake.detect(frame, v, activity.epoch()); }
    tone(226);
    for (int i = 0; i < 25; ++i) { word(i + 5); bool v = activity.update(160); weakFrames += v; matched |= wake.detect(frame, v, activity.epoch()); }
    for (int i = 0; i < 40; ++i) { bool v = activity.update(80); matched |= wake.detect(silence, v, activity.epoch()); }
    VoiceActivity startup;
    unsigned startupTriggers = 0;
    for (int i = 0; i < 200; ++i) startupTriggers += startup.update(130);
    printf("%s: weak syllables reach the real stored-template matcher (loud=%u weak=%u match=%d)\n", matched ? "PASS" : "FAIL", loudFrames, weakFrames, int(matched));
    printf("%s: stationary background stays unvoiced, including startup (quiet=%u startup=%u)\n", !quietTriggers && !startupTriggers ? "PASS" : "FAIL", quietTriggers, startupTriggers);
    LocalWake reload;
    bool retained = reload.begin();
    printf("%s: stored registration remains readable\n", retained ? "PASS" : "FAIL");
    VoiceActivity background;
    LocalWake backgroundDetector; backgroundDetector.begin();
    bool falseWake = false;
    tone(226);
    for (int i = 0; i < 180; ++i) falseWake |= backgroundDetector.detect(frame, background.update(160));
    for (int i = 0; i < 20; ++i) falseWake |= backgroundDetector.detect(silence, background.update(0));
    printf("%s: steady background followed by silence does not impersonate the stored word\n", !falseWake ? "PASS" : "FAIL");
    bool environmental = true, reacquired = true;
    // Background uses one constituent frequency of the changing stored word.
    // The gate's RMS fixture is independent of the fixed PCM feature fixture.
    for (float ambient : {160.f, 300.f, 900.f}) {
        VoiceActivity adaptive;
        LocalWake detector; detector.begin();
        auto push = [&](float raw, const int16_t* samples) {
            bool v = adaptive.update(raw);
            return detector.detect(samples, v, adaptive.epoch());
        };
        for (int i = 0; i < 200; ++i) environmental &= !push(80, silence);
        tone(226);
        for (int i = 0; i < 350; ++i) environmental &= !push(ambient + (i % 7 - 3) * 3, frame);
        // Detect a real foreground word after adapting to the louder room.
        bool foreground = false;
        for (int i = 0; i < 30; ++i) { word(i); foreground |= push(ambient + 400, frame); }
        for (int i = 0; i < 20; ++i) foreground |= push(ambient, silence);
        reacquired &= foreground;
        for (int i = 0; i < 250; ++i) environmental &= !push(0, silence);
        // Every return from playback/error recalibrates, without replacing Store.
        adaptive.beginCalibration(); detector.resetDetection();
        tone(226);
        for (int i = 0; i < 200; ++i) environmental &= !push(ambient, frame);
        for (int i = 0; i < 20; ++i) environmental &= !push(0, silence);
        for (int i = 0; i < 200; ++i) environmental &= !push(80, silence);
        bool again = false;
        for (int i = 0; i < 30; ++i) { word(i); again |= push(160, frame); }
        for (int i = 0; i < 20; ++i) again |= push(80, silence);
        reacquired &= again;
    }
    printf("%s: background onset/variation/stop and Ready recalibration do not trigger the matcher\n", environmental ? "PASS" : "FAIL");
    printf("%s: saved word is detected in louder ambient sound and after returning to Ready\n", reacquired ? "PASS" : "FAIL");
    reload.beginEnrollment(); tone(226);
    for (int i = 0; i < 25; ++i) reload.enroll(frame, true);
    int badRegistration = 0;
    for (int i = 0; i < 20; ++i) badRegistration = reload.enroll(silence, false);
    bool guarded = badRegistration == 0 && reload.registered() && reload.enrollmentCount() == 0;
    printf("%s: non-changing background cannot replace the saved registration\n", guarded ? "PASS" : "FAIL");
    reload.beginEnrollment(); tone(226);
    reload.enroll(frame, true);
    int shortResult = 0;
    for (int i = 0; i < 20; ++i) shortResult = reload.enroll(silence, false);
    bool waits = shortResult == 0 && reload.enrollmentCount() == 0;
    printf("%s: short cue residue/background is ignored while enrollment keeps waiting\n", waits ? "PASS" : "FAIL");
    return sequence && matched && !quietTriggers && !startupTriggers && retained && !falseWake && environmental && reacquired && guarded && waits ? 0 : 1;
}
