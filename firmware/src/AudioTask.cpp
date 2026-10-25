#include "AudioTask.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <algorithm>
namespace {
int16_t* recording = nullptr;
size_t used = 0;
bool capturing = false, mic = false, playing = false, voiced = false;
uint8_t volume = 160;
float rms = 0, noise = 60;
const int16_t* playback = nullptr;
size_t playbackCount = 0, position = 0;
int16_t speakerBuffers[4][960];
unsigned speakerBuffer = 0;
}
bool AudioTask::init(uint8_t gain, uint8_t level) {
    volume = level;
    recording = (int16_t*)heap_caps_calloc(conversation::InputSamples, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    M5.Speaker.end(); M5.Mic.end();
    auto config = M5.Mic.config(); config.sample_rate = 16000;
    config.magnification = uint8_t(constrain(int(gain) / 5, 1, 32)); M5.Mic.config(config);
    return recording && setMicEnabled(true);
}
bool AudioTask::setMicEnabled(bool enabled) {
    if (enabled == mic) return true;
    if (enabled) { if (playing) return false; M5.Speaker.end(); mic = M5.Mic.begin(); }
    else { M5.Mic.end(); mic = false; voiced = false; rms = 0; }
    return enabled == mic;
}
bool AudioTask::readFrame(int16_t* frame) {
    if (!mic || !M5.Mic.record(frame, 320, 16000)) return false;
    uint32_t start = millis();
    while (M5.Mic.isRecording()) {
        if (millis() - start > 250) { setMicEnabled(false); return false; }
        delay(1);
    }
    int64_t mean = 0; for (int i = 0; i < 320; ++i) mean += frame[i]; mean /= 320;
    double energy = 0; for (int i = 0; i < 320; ++i) { double v = frame[i] - mean; energy += v*v; }
    float raw = sqrt(energy / 320); rms = raw / 8000;
    voiced = raw > fmaxf(100, noise * 2.5f);
    if (!voiced) noise = fminf(500, fmaxf(25, noise * .98f + raw * .02f));
    if (capturing && recording) {
        size_t count = std::min(size_t(320), conversation::InputSamples - used);
        memcpy(recording + used, frame, count * 2); used += count;
    }
    return true;
}
bool AudioTask::isVoiceDetected() { return voiced; }
float AudioTask::getLiveRMS() { return rms; }
void AudioTask::startRecording() { clearRecording(); capturing = true; }
void AudioTask::stopRecording() { capturing = false; }
void AudioTask::clearRecording() { capturing = false; if (recording) memset(recording, 0, used * 2); used = 0; }
const int16_t* AudioTask::getRecordedPCM(size_t* count) { *count = used; return recording; }
bool AudioTask::recordingFull() { return used >= conversation::InputSamples; }
bool AudioTask::playPCM(const int16_t* data, size_t samples) {
    if (!data || !samples || playing) return false;
    setMicEnabled(false);
    if (!M5.Speaker.begin()) return false;
    M5.Speaker.setVolume(volume);
    playback = data; playbackCount = samples; position = 0; playing = true; return true;
}
bool AudioTask::playCue() {
    static int16_t cue[2400];
    for (unsigned i = 0; i < 2400; ++i) cue[i] = int16_t(sinf(6.2831853f * 880 * i / 24000) * 1500 * sinf(3.14159265f * i / 2400));
    return playPCM(cue, 2400);
}
void AudioTask::servicePlayback() {
    if (!playing) return;
    if (position < playbackCount && M5.Speaker.isPlaying(0) < 2) {
        size_t count = std::min(size_t(960), playbackCount - position);
        int16_t* buffer = speakerBuffers[speakerBuffer]; memcpy(buffer, playback + position, count * 2);
        if (!M5.Speaker.playRaw(buffer, count, 24000, false, 1, 0)) { stopPlayback(); return; }
        double energy = 0; for (size_t i = 0; i < count; ++i) energy += double(buffer[i])*buffer[i];
        rms = sqrt(energy / count) / 8000; position += count; speakerBuffer = (speakerBuffer + 1) % 4;
    }
    if (position == playbackCount && !M5.Speaker.isPlaying()) { playing = false; rms = 0; playback = nullptr; M5.Speaker.end(); }
}
bool AudioTask::isPlaying() { return playing; }
void AudioTask::stopPlayback() { M5.Speaker.stop(); M5.Speaker.end(); playing = false; playback = nullptr; rms = 0; }
