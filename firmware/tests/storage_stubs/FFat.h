#pragma once
// Reuse the existing in-memory FS fake; this is not a real FAT power-loss test.
#include "LittleFS.h"
namespace fs { using FS = LittleFSMock; }
struct FatMock : LittleFSMock {
    bool mountSucceeds = true, formatRequested = false;
    unsigned mountCalls = 0;
    bool begin(bool format, const char*, uint8_t, const char*) {
        ++mountCalls; formatRequested = format; return mountSucceeds;
    }
};
inline FatMock FFat;
