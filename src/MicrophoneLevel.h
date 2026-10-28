#pragma once
#include <stdint.h>
inline uint8_t microphoneMagnification(uint8_t configuredGain, uint8_t boardDefault = 2) {
    unsigned value = (unsigned(configuredGain) * boardDefault + 40) / 80;
    // Leave headroom for nearby speech; never overwrite the saved setting.
    unsigned maximum = unsigned(boardDefault) * 2;
    return uint8_t(value < 1 ? 1 : value > maximum ? maximum : value);
}
