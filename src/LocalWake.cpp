#include "LocalWake.h"
#include "ConversationRules.h"
#include <LittleFS.h>
#include <memory>
static const char* const slots[] = {"/wake-r13-a.bin", "/wake-r13-b.bin"};
bool LocalWake::read(const char* path, Store& target) {
    File f = LittleFS.open(path, "r");
    if (!f || f.size() != sizeof(Store) || f.read((uint8_t*)&target, sizeof(Store)) != sizeof(Store)) return false;
    if (target.magic != 0x5231574c || target.version != 1 || !isfinite(target.threshold) || target.threshold < 12 || target.threshold > 32 || target.crc != conversation::crc32((uint8_t*)&target, offsetof(Store, crc))) return false;
    for (auto& t : target.templates) if (t.count < 10 || t.count > localwake::MaxFrames) return false;
    return true;
}
bool LocalWake::begin() {
    std::unique_ptr<Store> other(new Store);
    bool a = read(slots[0], active_), b = read(slots[1], *other);
    if (b && (!a || int32_t(other->generation - active_.generation) > 0)) { active_ = *other; slot_ = 1; }
    else if (a) slot_ = 0;
    registered_ = a || b; segment_.reset(); return registered_;
}
void LocalWake::resetPending() {
    // No 11 KB temporary Store on Arduino's small loop-task stack.
    memset(&pending_, 0, sizeof(pending_)); pending_.magic = 0x5231574c;
    pending_.version = 1; pending_.threshold = 24; enrolled_ = 0; segment_.reset();
}
void LocalWake::beginEnrollment() { resetPending(); }
void LocalWake::cancelEnrollment() { resetPending(); }
bool LocalWake::detect(const int16_t* samples, bool voice) {
    if (!registered_) return false;
    auto result = segment_.push(samples, voice);
    if (result == localwake::Segmenter::None) return false;
    unsigned matches = 0;
    if (result == localwake::Segmenter::Complete) for (auto& t : active_.templates) if (localwake::distance(segment_.feature, t) <= active_.threshold) ++matches;
    segment_.reset(); return matches >= 2;
}
int LocalWake::enroll(const int16_t* samples, bool voice) {
    auto result = segment_.push(samples, voice);
    if (result == localwake::Segmenter::None) return 0;
    bool good = result == localwake::Segmenter::Complete;
    for (unsigned i = 0; good && i < enrolled_; ++i) good = localwake::distance(segment_.feature, pending_.templates[i]) <= 32;
    if (!good) { segment_.reset(); return -1; }
    pending_.templates[enrolled_++] = segment_.feature; segment_.reset();
    if (enrolled_ < 3) return 1;
    float worst = 0;
    for (int i = 0; i < 3; ++i) for (int j = i+1; j < 3; ++j) worst = fmaxf(worst, localwake::distance(pending_.templates[i], pending_.templates[j]));
    pending_.threshold = fminf(32, fmaxf(12, worst * 1.5f + 6));
    if (commit()) { cancelEnrollment(); return 2; }
    cancelEnrollment(); return -2;
}
bool LocalWake::commit() {
    pending_.generation = registered_ ? active_.generation + 1 : 1;
    pending_.crc = conversation::crc32((uint8_t*)&pending_, offsetof(Store, crc));
    const char* temp = "/wake-r13.tmp"; File f = LittleFS.open(temp, "w");
    if (!f) return false;
    bool written = f.write((uint8_t*)&pending_, sizeof(Store)) == sizeof(Store); f.flush(); f.close();
    std::unique_ptr<Store> check(new Store);
    if (!written || !read(temp, *check) || memcmp(check.get(), &pending_, sizeof(Store))) return false;
    int next = slot_ == 0 ? 1 : 0;
    if (LittleFS.exists(slots[next]) && !LittleFS.remove(slots[next])) return false;
    if (!LittleFS.rename(temp, slots[next])) return false;
    if (!read(slots[next], *check) || memcmp(check.get(), &pending_, sizeof(Store))) return false;
    active_ = pending_; slot_ = next; registered_ = true; return true;
}
