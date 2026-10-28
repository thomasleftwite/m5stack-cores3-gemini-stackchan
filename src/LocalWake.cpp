#include "LocalWake.h"
#include "ConversationRules.h"
#include "LocalStorage.h"
#include <memory>
#include <new>
#include "RuntimeTrace.h"
static const char* const slots[] = {"/wake-mfcc-v2-a.bin", "/wake-mfcc-v2-b.bin"};
bool LocalWake::read(const char* path, Store& target) {
    File f = localstorage::filesystem().open(path, "r");
    if (!f || f.size() != sizeof(Store) || f.read((uint8_t*)&target, sizeof(Store)) != sizeof(Store)) return false;
    if (target.magic != 0x3243574c || target.version != 2 || !isfinite(target.threshold) || target.threshold < 12 || target.threshold > 32 || target.crc != conversation::crc32((uint8_t*)&target, offsetof(Store, crc))) return false;
    for (auto& t : target.templates) if (t.count < 10 || t.count > localwake::MaxFrames) return false;
    return true;
}
bool LocalWake::begin() {
    std::unique_ptr<Store> other(new Store);
    bool a = read(slots[0], active_), b = read(slots[1], *other);
    if (b && (!a || int32_t(other->generation - active_.generation) > 0)) { active_ = *other; slot_ = 1; }
    else if (a) slot_ = 0;
    registered_ = a || b; segment_.reset();
#ifdef ARDUINO
    if (registered_) {
        unsigned flat = 0, reversed = 0, pairs = 0, sequencePairs = 0;
        auto& probe = other->templates[0];
        for (unsigned i = 0; i < 3; ++i) {
            const auto& t = active_.templates[i];
            localwake::stationary(t, probe);
            if (localwake::distance(t, probe) <= active_.threshold) ++flat;
            probe.count = t.count;
            for (unsigned n = 0; n < t.count; ++n) memcpy(probe.frames[n], t.frames[t.count - n - 1], sizeof(probe.frames[n]));
            if (localwake::distance(t, probe) <= active_.threshold) ++reversed;
            for (unsigned j = i + 1; j < 3; ++j) if (localwake::distance(t, active_.templates[j]) <= active_.threshold) ++pairs;
        }
        // Reuse the heap scratch Store; never put 3 Features on loopTask's stack.
        for (unsigned i = 0; i < 3; ++i) {
            localwake::prepareForMatch(active_.templates[i], other->templates[0]);
            localwake::stationary(other->templates[0], other->templates[1]);
            for (unsigned j = i + 1; j < 3; ++j) {
                localwake::prepareForMatch(active_.templates[j], other->templates[2]);
                if (localwake::compareWord(other->templates[0], other->templates[2], other->templates[1]).matches(active_.threshold)) ++sequencePairs;
            }
        }
        Serial.printf("[DEBUG-r13w2] flat_templates=%u reversed_templates=%u pair_matches=%u\n", flat, reversed, pairs);
        Serial.printf("[DEBUG-r13w2] sequence_pairs=%u\n", sequencePairs);
    }
#endif
    return registered_;
}
void LocalWake::resetPending() {
    // No 11 KB temporary Store on Arduino's small loop-task stack.
    memset(&pending_, 0, sizeof(pending_)); pending_.magic = 0x3243574c;
    pending_.version = 2; pending_.threshold = 24; enrolled_ = 0; segment_.reset();
}
void LocalWake::beginEnrollment() { resetPending(); }
void LocalWake::cancelEnrollment() { resetPending(); }
bool LocalWake::detect(const int16_t* samples, bool voice, uint32_t backgroundEpoch) {
    if (!registered_) return false;
    if (backgroundEpoch_ != backgroundEpoch) {
#ifdef ARDUINO
        Serial.printf("[DEBUG-e02] primary_epoch_reset=1 frames=%u\n", unsigned(segment_.feature.count));
#endif
        segment_.reset(); backgroundEpoch_ = backgroundEpoch;
    }
    ++diagnostics_.frames; if (voice) ++diagnostics_.voice;
    auto result = segment_.push(samples, voice);
    if (result == localwake::Segmenter::None) return false;
    unsigned matches = 0;
    unsigned decision = 1;
    if (result == localwake::Segmenter::Complete && segment_.speechEvidence()) {
        ++diagnostics_.complete;
        localwake::prepareForMatch(segment_.feature, pending_.templates[0]);
        localwake::stationary(pending_.templates[0], pending_.templates[1]);
        unsigned base = 0, means = 0, strict25 = 0, strict35 = 0, strict50 = 0;
        for (auto& t : active_.templates) {
            if (localwake::distance(segment_.feature, t) <= active_.threshold) ++base;
            localwake::prepareForMatch(t, pending_.templates[2]);
            auto comparison = localwake::compareWord(pending_.templates[0], pending_.templates[2], pending_.templates[1]);
            if (comparison.actual <= active_.threshold) ++means;
            if (comparison.matches(active_.threshold)) ++matches;
            if (comparison.matches(active_.threshold, .25f)) ++strict25;
            if (comparison.matches(active_.threshold, .35f)) ++strict35;
            if (comparison.matches(active_.threshold, .50f)) ++strict50;
        }
#ifdef ARDUINO
        if (Serial.availableForWrite() >= 112) Serial.printf("[DEBUG-r13w2] raw_matches=%u centered_matches=%u sequence_matches=%u\n", base, means, matches);
        if (Serial.availableForWrite() >= 96) Serial.printf("[DEBUG-r13w2] strict25=%u strict35=%u strict50=%u\n", strict25, strict35, strict50);
#else
        (void)base; (void)means; (void)strict25; (void)strict35; (void)strict50;
#endif
    } else if (segment_.rejection() == localwake::Segmenter::TooShort) { ++diagnostics_.tooShort; decision = 2; }
    else if (segment_.rejection() == localwake::Segmenter::Clipped) { ++diagnostics_.clipped; decision = 3; }
    else { ++diagnostics_.tooLong; decision = 4; }
    diagnostics_.lastMatches = matches;
    if (matches >= 2) { ++diagnostics_.matched; decision = 5; }
#ifdef ARDUINO
    if (Serial.availableForWrite() >= 96) Serial.printf("[DEBUG-r13w1] decision=%u matches=%u\n", decision, matches);
#else
    (void)decision;
#endif
    segment_.reset(); return matches >= 2;
}
int LocalWake::probeEnergy(const int16_t* samples, bool aboveBackground, uint32_t backgroundEpoch) {
    if (!registered_) return -1;
    if (energyEpoch_ != backgroundEpoch) {
#ifdef ARDUINO
        Serial.printf("[DEBUG-e02] energy_epoch_reset=1 frames=%u\n", unsigned(energySegment_.feature.count));
#endif
        energySegment_.reset(); energyEpoch_=backgroundEpoch;
    }
    auto result=energySegment_.push(samples,aboveBackground);
    if (result==localwake::Segmenter::None) return -1;
    unsigned matches=0, absolute=0;
    float best=1000000;
    // Do not change the real wake gate, pair threshold or sequence margin.
    if (result==localwake::Segmenter::Complete && energySegment_.speechEvidence()) {
        localwake::prepareForMatch(energySegment_.feature,pending_.templates[0]);
        localwake::stationary(pending_.templates[0],pending_.templates[1]);
        for (const auto& t : active_.templates) {
            localwake::prepareForMatch(t,pending_.templates[2]);
            auto c=localwake::compareWord(pending_.templates[0],pending_.templates[2],pending_.templates[1]);
            best=fminf(best,c.actual);
            absolute += c.actual<=active_.threshold;
            matches += c.matches(active_.threshold);
        }
    }
#ifdef ARDUINO
    Serial.printf("[DEBUG-e02] energy_frames=%u voice=%u shape=%u rejection=%u matches=%u absolute=%u best_bin=%u threshold_bin=%u\n",
        unsigned(energySegment_.feature.count),unsigned(energySegment_.voicedFrames()),unsigned(energySegment_.shapedFrames()),
        unsigned(energySegment_.rejection()),matches,absolute,unsigned(fminf(999,best)),unsigned(active_.threshold));
#else
    (void)best; (void)absolute;
#endif
    energySegment_.reset(); return int(matches);
}
bool LocalWake::detectWindow(const int16_t* samples) {
    if (!registered_) return false;
    ++diagnostics_.frames;
    int matches=probeWindow(samples);
    if (matches<0) return false;
    if (pending_.templates[0].count) ++diagnostics_.complete;
    diagnostics_.lastMatches=unsigned(matches);
    if (matches>=2) { ++diagnostics_.matched; return true; }
    return false;
}
int LocalWake::probeWindow(const int16_t* samples) {
    if (!registered_) return -1;
    int result=rolling_.push(samples,pending_.templates[0]);
    if (result<0) return -1;
    unsigned matches=0,absolute=0;
    float best=1000000;
    if (result==1) {
        localwake::prepareForMatch(pending_.templates[0],pending_.templates[1]);
        localwake::stationary(pending_.templates[1],pending_.templates[2]);
        for (unsigned i=0;i<3;++i) {
            localwake::prepareForMatch(active_.templates[i],rollingStored_);
            auto c=localwake::compareWord(pending_.templates[1],rollingStored_,pending_.templates[2]);
            best=fminf(best,c.actual); absolute += c.actual<=active_.threshold;
            matches += c.matches(active_.threshold);
#ifdef ARDUINO
            Serial.printf("[DEBUG-e04] template=%u frames=%u actual_bin=%u stationary_bin=%u\n",i,unsigned(active_.templates[i].count),
                unsigned(fminf(999,c.actual)),unsigned(fminf(999,c.stationary)));
#endif
        }
    }
#ifdef ARDUINO
    Serial.printf("[DEBUG-e04] window_crop=%u matches=%u absolute=%u best_bin=%u threshold_bin=%u\n",
        unsigned(pending_.templates[0].count),matches,absolute,unsigned(fminf(999,best)),unsigned(active_.threshold));
#else
    (void)absolute; (void)best;
#endif
    return int(matches);
}
int LocalWake::enroll(const int16_t* samples, bool voice) {
    auto result = segment_.push(samples, voice);
    if (result == localwake::Segmenter::None) return 0;
#ifdef ARDUINO
    if (Serial.availableForWrite() >= 96) Serial.printf("[DEBUG-r13v1] segment_voice=%u segment_shape=%u\n", unsigned(segment_.voicedFrames()), unsigned(segment_.shapedFrames()));
#endif
    if (result == localwake::Segmenter::Complete && !segment_.speechEvidence()) {
        segment_.reset(); return 0;
    }
    unsigned rejection = result == localwake::Segmenter::Rejected ?
        (segment_.rejection() == localwake::Segmenter::TooShort ? 1 : segment_.rejection() == localwake::Segmenter::Clipped ? 2 : 3) : 0;
    if (rejection == 1) {
#ifdef ARDUINO
        if (Serial.availableForWrite() >= 64) Serial.println("[DEBUG-r13w3] rejection=1");
#endif
        segment_.reset(); return 0; // Tiny background/cue tails do not restart a take.
    }
    bool good = result == localwake::Segmenter::Complete;
    std::unique_ptr<localwake::Features[]> analysis;
    if (good) {
        analysis.reset(new (std::nothrow) localwake::Features[3]);
        good = bool(analysis);
        if (!good) rejection = 6;
    }
    if (good) {
        localwake::prepareForMatch(segment_.feature, analysis[0]);
        localwake::stationary(analysis[0], analysis[1]);
        good = localwake::compareWord(analysis[0], analysis[0], analysis[1]).matches(32);
        if (!good) rejection = 4;
        for (unsigned i = 0; good && i < enrolled_; ++i) {
            localwake::prepareForMatch(pending_.templates[i], analysis[2]);
            analysis[1].count = analysis[0].count;
            auto forward = localwake::compareWord(analysis[0], analysis[2], analysis[1]);
            good = forward.matches(32);
            analysis[1].count = analysis[2].count;
            auto backward = localwake::compareWord(analysis[2], analysis[0], analysis[1]);
            good = good && backward.matches(32);
            if (!good) rejection = 5;
#ifdef ARDUINO
            if (!good && Serial.availableForWrite() >= 112) {
                unsigned flags = (analysis[0].count > analysis[2].count * 2 || analysis[2].count > analysis[0].count * 2 ? 1 : 0)
                    | (forward.actual > 32 ? 2 : 0)
                    | (forward.stationary - forward.actual < fmaxf(1, forward.stationary * .15f) ? 4 : 0)
                    | (backward.stationary - backward.actual < fmaxf(1, backward.stationary * .15f) ? 8 : 0);
                Serial.printf("[DEBUG-r13w3] pair_flags=%u candidate_bin=%u reference_bin=%u\n", flags,
                    unsigned((analysis[0].count + 9) / 10), unsigned((analysis[2].count + 9) / 10));
            }
#endif
        }
    }
    if (!good) {
#ifdef ARDUINO
        if (Serial.availableForWrite() >= 64) Serial.printf("[DEBUG-r13w3] rejection=%u\n", rejection);
#else
        (void)rejection;
#endif
        segment_.reset(); return -1;
    }
    pending_.templates[enrolled_++] = segment_.feature; segment_.reset();
    if (enrolled_ < 3) return 1;
    float worst = 0;
    for (int i = 0; i < 3; ++i) for (int j = i+1; j < 3; ++j) {
        localwake::prepareForMatch(pending_.templates[i], analysis[0]);
        localwake::prepareForMatch(pending_.templates[j], analysis[2]);
        worst = fmaxf(worst, localwake::distance(analysis[0], analysis[2], true));
    }
    pending_.threshold = fminf(32, fmaxf(12, worst * 1.5f + 6));
    if (commit()) {
#ifdef ARDUINO
        Serial.println("[DEBUG-r13w2] sequence_pairs=3");
#endif
        cancelEnrollment(); return 2;
    }
    cancelEnrollment(); return -2;
}
int LocalWake::submitEnrollment(const localwake::Features& candidate) {
    if (candidate.count < 10 || candidate.count > localwake::MaxFrames || enrolled_ >= 3) {
        cancelEnrollment(); return -1;
    }
    pending_.templates[enrolled_++] = candidate;
    if (enrolled_ < 3) return 1;
    std::unique_ptr<localwake::Features[]> analysis(new (std::nothrow) localwake::Features[4]);
    if (!analysis) { cancelEnrollment(); return -2; }
    float worst=0;
    for (unsigned i=0;i<3;++i) {
        localwake::prepareForMatch(pending_.templates[i], analysis[0]);
        localwake::stationary(analysis[0], analysis[1]);
        if (!localwake::compareWord(analysis[0], analysis[0], analysis[1]).matches(32)) {
            cancelEnrollment(); return -1;
        }
        for (unsigned j=i+1;j<3;++j) {
            localwake::prepareForMatch(pending_.templates[j], analysis[2]);
            localwake::stationary(analysis[2], analysis[3]);
            auto a=localwake::compareWord(analysis[0], analysis[2], analysis[1]);
            auto b=localwake::compareWord(analysis[2], analysis[0], analysis[3]);
            if (!a.matches(32) || !b.matches(32)) { cancelEnrollment(); return -1; }
            worst=fmaxf(worst, fmaxf(a.actual,b.actual));
        }
    }
    pending_.threshold=fminf(32, fmaxf(12, worst*1.5f+6));
    bool saved=commit(); cancelEnrollment(); return saved ? 2 : -2;
}
bool LocalWake::commit() {
    runtimetrace::mark(runtimetrace::StoreBegin);
    pending_.generation = registered_ ? active_.generation + 1 : 1;
    pending_.crc = conversation::crc32((uint8_t*)&pending_, offsetof(Store, crc));
    const char* temp = "/wake-mfcc-v2.tmp"; File f = localstorage::filesystem().open(temp, "w");
    if (!f) return false;
    runtimetrace::mark(runtimetrace::StoreWrite);
    bool written = f.write((uint8_t*)&pending_, sizeof(Store)) == sizeof(Store); f.flush(); f.close();
    runtimetrace::mark(runtimetrace::StoreWritten, written);
    std::unique_ptr<Store> check(new Store);
    runtimetrace::mark(runtimetrace::StoreAllocated);
    if (!written || !read(temp, *check) || memcmp(check.get(), &pending_, sizeof(Store))) return false;
    runtimetrace::mark(runtimetrace::StoreChecked);
    int next = slot_ == 0 ? 1 : 0;
    if (localstorage::filesystem().exists(slots[next]) && !localstorage::filesystem().remove(slots[next])) return false;
    if (!localstorage::filesystem().rename(temp, slots[next])) return false;
    runtimetrace::mark(runtimetrace::StoreRenamed);
    if (!read(slots[next], *check) || memcmp(check.get(), &pending_, sizeof(Store))) return false;
    active_ = pending_; slot_ = next; registered_ = true;
    runtimetrace::mark(runtimetrace::StorePublished); return true;
}
