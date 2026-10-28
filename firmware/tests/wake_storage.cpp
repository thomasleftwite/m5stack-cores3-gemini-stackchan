#include "../src/LocalWake.h"
#include <LittleFS.h>
#include <assert.h>
#include <stdio.h>
#include "SyntheticSpeech.h"
static int16_t frame[320], silence[320] = {};
static int enroll(LocalWake& wake, float frequency) {
    for (int i = 0; i < 30; ++i) { syntheticSpeechFrame(frame, unsigned(i), 3000, frequency != 800); assert(wake.enroll(frame, true) == 0); }
    for (int i = 0; i < 19; ++i) assert(wake.enroll(silence, false) == 0);
    return wake.enroll(silence, false);
}
static bool detects(LocalWake& wake, float frequency) {
    wake.resetDetection();
    for (int i = 0; i < 30; ++i) { syntheticSpeechFrame(frame, unsigned(i), 3000, frequency != 800); assert(!wake.detect(frame, true)); }
    for (int i = 0; i < 19; ++i) assert(!wake.detect(silence, false));
    return wake.detect(silence, false);
}
int main() {
    storage.files["/wake-r13-a.bin"] = {1, 2, 3, 4};
    LocalWake wake; assert(!wake.begin()); wake.beginEnrollment();
    assert(enroll(wake, 800) == 1 && wake.enrollmentCount() == 1 && !wake.registered());
    assert(enroll(wake, 800) == 1 && wake.enrollmentCount() == 2 && !wake.registered());
    assert(enroll(wake, 800) == 2 && wake.registered());
    assert(detects(wake,800));
    assert(wake.diagnostics().frames == 50 && wake.diagnostics().voice == 30 && wake.diagnostics().complete == 1);
    assert(wake.diagnostics().matched == 1 && wake.diagnostics().lastMatches == 3);
    assert(!detects(wake,3300));
    assert(wake.diagnostics().complete == 2 && wake.diagnostics().matched == 1 && wake.diagnostics().lastMatches == 0);
    LocalWake reboot; assert(reboot.begin() && detects(reboot,800));
    reboot.beginEnrollment(); assert(enroll(reboot,3300)==1); assert(enroll(reboot,800)==-1);
    assert(reboot.enrollmentCount()==1); reboot.cancelEnrollment(); assert(detects(reboot,800));
    reboot.beginEnrollment(); assert(enroll(reboot,3300)==1); assert(enroll(reboot,3300)==1);
    storage.shortWrite=true; assert(enroll(reboot,3300)==-2); storage.shortWrite=false;
    LocalWake afterShortWrite; assert(afterShortWrite.begin() && detects(afterShortWrite,800));
    afterShortWrite.beginEnrollment(); enroll(afterShortWrite,3300); enroll(afterShortWrite,3300);
    storage.failRename=true; assert(enroll(afterShortWrite,3300)==-2); storage.failRename=false;
    LocalWake afterRenameFailure; assert(afterRenameFailure.begin() && detects(afterRenameFailure,800));
    afterRenameFailure.beginEnrollment(); enroll(afterRenameFailure,3300); enroll(afterRenameFailure,3300);
    assert(enroll(afterRenameFailure,3300)==2);
    LocalWake updated; assert(updated.begin() && detects(updated,3300) && !detects(updated,800));
    storage.files["/wake-mfcc-v2-b.bin"][32] ^= 1;
    LocalWake corrupted; assert(corrupted.begin() && detects(corrupted,800));
    storage.files["/wake-mfcc-v2-a.bin"].resize(20);
    LocalWake bothInvalid; assert(!bothInvalid.begin());
    assert((storage.files["/wake-r13-a.bin"] == std::vector<uint8_t>{1,2,3,4}));
    puts("PASS: three-sample publication, mismatch retry, cancel, reload, short write, rename failure, CRC fallback, corrupt slots");
}
