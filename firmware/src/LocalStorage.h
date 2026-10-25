#pragma once
#include <FFat.h>

// Fixed FAT layout selected for this CoreS3. Never format on a mount failure.
namespace localstorage {
inline bool begin() { return FFat.begin(false, "/ffat", 2, "ffat"); }
inline fs::FS& filesystem() { return FFat; }
}
