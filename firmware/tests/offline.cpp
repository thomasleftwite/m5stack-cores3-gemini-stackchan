#include "../src/ConversationRules.h"
#include "../src/LocalWakeCore.h"
#include "../src/QueuedMicFrames.h"
#include "../src/RequestFailure.h"
#include <assert.h>
#include <vector>
#include <stdio.h>
#include <limits>
#include <deque>
struct DelayedMic {
    struct Request { int16_t* data; size_t count; };
    std::deque<Request> pending;
    int16_t completed = 0;
    bool reject = false;
    // Simulates notification before recording starts: no buffer is filled until
    // the third enqueue blocks for a free slot. The old isRecording loop misses it.
    bool record(int16_t* destination, size_t count, uint32_t rate) {
        assert(count % 320 == 0 && rate == 16000);
        if (reject) return false;
        if (pending.size() == 2) {
            auto out = pending.front(); pending.pop_front();
            for (size_t base=0;base<out.count;base+=320) {
                ++completed;
                for (int i = 0; i < 320; ++i) out.data[base+i] = completed;
            }
        }
        pending.push_back({destination,count}); return true;
    }
};
// Conservative queue model: samples arriving with no destination are lost.
// Not an emulation of the hardware DMA FIFO; isolates destination starvation.
struct TimedMic {
    struct Request { int16_t* data; size_t count, used; };
    std::deque<Request> pending;
    unsigned time=0, lost=0;
    void advance(unsigned samples) {
        for(unsigned n=0;n<samples;++n,++time) {
            if(pending.empty()) { ++lost; continue; }
            auto& p=pending.front(); p.data[p.used++]=int16_t(time%30000);
            if(p.used==p.count) pending.pop_front();
        }
    }
    bool record(int16_t* p,size_t count,uint32_t rate) {
        assert(rate==16000);
        while(pending.size()==2) advance(1);
        pending.push_back({p,count,0}); return true;
    }
};
int main() {
    assert(requestfailure::classify(nullptr) == requestfailure::None);
    assert(requestfailure::classify("") == requestfailure::None);
    assert(requestfailure::classify("Clock not synchronized") == requestfailure::Clock);
    assert(requestfailure::classify("WiFi disconnected") == requestfailure::WiFi);
    assert(requestfailure::classify("Request memory unavailable") == requestfailure::RequestMemory);
    assert(requestfailure::classify("unknown private content") == requestfailure::Unknown);
    QueuedMicFrames micFrames; DelayedMic microphone; int16_t completeFrame[320];
    for (int n = 1; n <= 12; ++n) {
        assert(micFrames.read(microphone, completeFrame));
        for (auto sample : completeFrame) assert(sample == n);
    }
    microphone.pending.clear(); micFrames.reset();
    assert(micFrames.read(microphone, completeFrame) && completeFrame[0] == 13);
    microphone.reject = true;
    bool rejected=false;
    for(unsigned n=0;n<4;++n) if(!micFrames.read(microphone,completeFrame)) { rejected=true; break; }
    assert(rejected);
    QueuedMicFrames buffered; TimedMic timed;
    assert(buffered.prime(timed));
    assert(timed.time==0 && timed.pending.size()==2);
    timed.advance(46*16); // First status redraw after enabling the microphone.
    unsigned expected=0, nextDraw=0;
    for(unsigned n=0;n<250;++n) {
        assert(buffered.read(timed,completeFrame));
        for(auto sample:completeFrame) assert(sample==int16_t(expected++%30000));
        if(timed.time>=nextDraw) { timed.advance(46*16); nextDraw=timed.time+54*16; }
        timed.advance(16); // UI loop's 1 ms yield.
    }
    assert(timed.lost==0);
    puts("PASS: 5 seconds of ordered PCM survives 46 ms drawing with 100 ms frame scheduling (queue model)");
    using namespace conversation;
    auto retryAt = retryDeadline(100, 60000);
    assert(!deadlineReached(60100, retryAt)); // 429 retry window still retains STT/reply.
    assert(!deadlineReached(90099, retryAt));
    assert(deadlineReached(90100, retryAt));
    retryAt = retryDeadline(0xfffffff0u, 60000);
    assert(!deadlineReached(uint32_t(0xfffffff0u + 60000), retryAt));
    assert(deadlineReached(uint32_t(0xfffffff0u + 90000), retryAt));
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
    assert(segment.rejection() == localwake::Segmenter::TooShort);
    segment.reset(); for (int i = 0; i < 149; ++i) segment.push(tone, true);
    assert(segment.push(tone, true) == localwake::Segmenter::Rejected);
    assert(segment.rejection() == localwake::Segmenter::TooLong);
    puts("PASS: capture deadlines, timer wrap, WAV format/length/chunks, CRC, DTW, local segmentation");
}
