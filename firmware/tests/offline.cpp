#include "../src/ConversationRules.h"
#include "../src/LocalWakeCore.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
#include <limits>
int main() {
    using namespace conversation;
    CaptureGate gate; gate.reset(100);
    assert(gate.update(5099, false) == CaptureGate::Waiting);
    assert(gate.update(5100, false) == CaptureGate::Empty);
    gate.reset(0);
    for (int t = 0; t < 200; t += 20) assert(gate.update(t, true) == CaptureGate::Waiting);
    assert(gate.update(1179, false) == CaptureGate::Waiting);
    assert(gate.update(1180, false) == CaptureGate::Complete);
    gate.reset(0); for (int t = 0; t < 15000; t += 20) gate.update(t, true);
    assert(gate.update(15000, true) == CaptureGate::Complete);
    gate.reset(0xfffffff0u);
    assert(gate.update(uint32_t(0xfffffff0u + 5000u), false) == CaptureGate::Empty);
    assert(elapsed(40, 0xfffffff0u) == 56);
    assert(InputSamples * 2 == 480000);
    std::vector<uint8_t> wav(44 + 4800); wavHeader(wav.data(), 4800, OutputRate);
    WavView view; assert(validateWav(wav.data(), wav.size(), view)); assert(view.offset == 44 && view.bytes == 4800);
    assert(!validateWav(wav.data(), wav.size()-1, view));
    wav[24] ^= 1; assert(!validateWav(wav.data(), wav.size(), view)); wav[24] ^= 1;
    wav[34] = 8; assert(!validateWav(wav.data(), wav.size(), view)); wav[34] = 16;
    put32(wav.data()+40, 4801); assert(!validateWav(wav.data(), wav.size(), view));
    put32(wav.data()+40, 4800);
    // Legal RIFF chunks can precede data; never assume a 44-byte header.
    std::vector<uint8_t> extra(12 + wav.size());
    memcpy(extra.data(), wav.data(), 36); memcpy(extra.data()+36, "JUNK", 4); put32(extra.data()+40, 4);
    memcpy(extra.data()+48, wav.data()+36, wav.size()-36); put32(extra.data()+4, uint32_t(extra.size()-8));
    assert(validateWav(extra.data(), extra.size(), view) && view.offset == 56);
    put32(extra.data()+40, 0xffffffffu); assert(!validateWav(extra.data(), extra.size(), view));
    std::vector<uint8_t> large(44 + MaxOutputBytes + 2); wavHeader(large.data(), MaxOutputBytes+2, OutputRate);
    assert(!validateWav(large.data(), large.size(), view));
    assert(crc32((const uint8_t*)"123456789", 9) == 0xcbf43926u);
    localwake::Features a, b; a.count = b.count = 20;
    for (int i = 0; i < 20; ++i) for (size_t j = 0; j < localwake::Dimensions; ++j) a.frames[i][j] = b.frames[i][j] = int16_t(i+j);
    assert(localwake::distance(a,b) == 0);
    for (int i = 0; i < 20; ++i) for (size_t j = 0; j < localwake::Dimensions; ++j) b.frames[i][j] += 100;
    assert(localwake::distance(a,b) > 32);
    b.count = 151; assert(localwake::distance(a,b) > 1000);
    b.count = 0; assert(localwake::distance(a,b) > 1000);
    int16_t silence[320] = {}, tone[320];
    for (int i = 0; i < 320; ++i) tone[i] = int16_t(3000 * sinf(6.2831853f * 800 * i / 16000));
    localwake::Segmenter segment;
    for (int i = 0; i < 300; ++i) assert(segment.push(silence, false) == localwake::Segmenter::None);
    for (int i = 0; i < 20; ++i) assert(segment.push(tone, true) == localwake::Segmenter::None);
    for (int i = 0; i < 19; ++i) assert(segment.push(silence, false) == localwake::Segmenter::None);
    assert(segment.push(silence, false) == localwake::Segmenter::Complete); assert(segment.feature.count == 20);
    segment.reset(); segment.push(tone, true);
    for (int i = 0; i < 19; ++i) segment.push(silence, false);
    assert(segment.push(silence, false) == localwake::Segmenter::Rejected);
    segment.reset(); for (int i = 0; i < 149; ++i) segment.push(tone, true);
    assert(segment.push(tone, true) == localwake::Segmenter::Rejected);
    puts("PASS: capture deadlines, timer wrap, WAV format/length/chunks, CRC, DTW, local segmentation");
}
