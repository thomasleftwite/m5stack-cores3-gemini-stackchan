#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace conversation {
constexpr uint32_t InputRate = 16000, OutputRate = 24000;
constexpr size_t InputSamples = InputRate * 15, MaxOutputBytes = OutputRate * 2 * 20;
constexpr uint32_t IdleMs = 180000, StartWaitMs = 5000, EndSilenceMs = 1000;
inline uint32_t elapsed(uint32_t now, uint32_t then) { return now - then; }
struct CaptureGate {
    uint32_t start = 0, lastVoice = 0, voicedMs = 0; bool heard = false;
    void reset(uint32_t now) { start = lastVoice = now; voicedMs = 0; heard = false; }
    enum Result { Waiting, Complete, Empty };
    Result update(uint32_t now, bool voice, uint32_t limitMs = 15000) {
        if (voice) { heard = true; lastVoice = now; voicedMs += 20; }
        if (!heard && elapsed(now, start) >= StartWaitMs) return Empty;
        if (elapsed(now, start) >= limitMs || (heard && elapsed(now, lastVoice) >= EndSilenceMs))
            return voicedMs >= 200 ? Complete : Empty;
        return Waiting;
    }
};
inline uint16_t le16(const uint8_t* p) { return p[0] | (uint16_t(p[1]) << 8); }
inline uint32_t le32(const uint8_t* p) { return le16(p) | (uint32_t(le16(p + 2)) << 16); }
inline void put16(uint8_t* p, uint16_t v) { p[0] = uint8_t(v); p[1] = uint8_t(v >> 8); }
inline void put32(uint8_t* p, uint32_t v) { put16(p, uint16_t(v)); put16(p + 2, uint16_t(v >> 16)); }
inline void wavHeader(uint8_t* h, uint32_t bytes, uint32_t rate = InputRate) {
    memset(h, 0, 44); memcpy(h, "RIFF", 4); put32(h + 4, bytes + 36);
    memcpy(h + 8, "WAVEfmt ", 8); put32(h + 16, 16); put16(h + 20, 1);
    put16(h + 22, 1); put32(h + 24, rate); put32(h + 28, rate * 2);
    put16(h + 32, 2); put16(h + 34, 16); memcpy(h + 36, "data", 4); put32(h + 40, bytes);
}
struct WavView { size_t offset = 0, bytes = 0; };
inline bool validateWav(const uint8_t* data, size_t size, WavView& out) {
    out = {};
    if (!data || size < 44 || memcmp(data, "RIFF", 4) || memcmp(data + 8, "WAVE", 4) || uint64_t(le32(data + 4)) + 8 != size) return false;
    bool format = false, audio = false;
    for (size_t at = 12; at + 8 <= size;) {
        uint32_t n = le32(data + at + 4);
        if (n > size - at - 8) return false;
        if (!memcmp(data + at, "fmt ", 4)) {
            if (format || n < 16 || le16(data + at + 8) != 1 || le16(data + at + 10) != 1 || le32(data + at + 12) != OutputRate || le32(data + at + 16) != OutputRate * 2 || le16(data + at + 20) != 2 || le16(data + at + 22) != 16) return false;
            format = true;
        } else if (!memcmp(data + at, "data", 4)) {
            if (audio || !n || (n & 1) || n > MaxOutputBytes) return false;
            out.offset = at + 8; out.bytes = n; audio = true;
        }
        const size_t next = at + 8 + n + (n & 1);
        if (next > size) return false;
        at = next;
        if (at == size) return format && audio;
    }
    return false;
}
inline uint32_t crc32(const uint8_t* p, size_t size) {
    uint32_t crc = ~0u;
    while (size--) { crc ^= *p++; for (int i = 0; i < 8; ++i) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1))); }
    return ~crc;
}
}
