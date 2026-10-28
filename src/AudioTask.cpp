#include "AudioTask.h"
#include <esp_heap_caps.h>
#include <math.h>
#include <algorithm>
#include "QueuedMicFrames.h"
#include "VoiceActivity.h"
#include "MicrophoneLevel.h"
#include "SpeechDetector.h"
namespace {
int16_t* recording = nullptr;
size_t used = 0;
bool capturing = false, mic = false, playing = false, voiced = false, aboveBackground = false;
uint8_t volume = 160;
float rms = 0;
VoiceActivity voiceActivity;
SpeechDetector speechDetector;
unsigned diagnosticFrames = 0, diagnosticEnergy = 0, diagnosticSpeech = 0, diagnosticJoint = 0;
const int16_t* playback = nullptr;
size_t playbackCount = 0, position = 0;
int16_t speakerBuffers[4][960];
unsigned speakerBuffer = 0;
QueuedMicFrames micFrames;
}
bool AudioTask::init(uint8_t gain, uint8_t level) {
    volume = level;
    recording = (int16_t*)heap_caps_calloc(conversation::InputSamples, 2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    M5.Speaker.end(); M5.Mic.end();
    auto config = M5.Mic.config(); config.sample_rate = 16000;
    config.magnification = microphoneMagnification(gain, config.magnification); M5.Mic.config(config);
    return recording && speechDetector.begin() && setMicEnabled(true);
}
bool AudioTask::setMicEnabled(bool enabled) {
    if (enabled == mic) return true;
    if (enabled) {
        if (playing) return false;
        M5.Speaker.end(); mic = M5.Mic.begin();
        // Reserve input destinations before a status/face draw can block.
        if (mic && !micFrames.prime(M5.Mic)) { M5.Mic.end(); micFrames.reset(); mic=false; }
    }
    else { M5.Mic.end(); micFrames.reset(); mic = false; voiced = aboveBackground = false; rms = 0; }
    return enabled == mic;
}
bool AudioTask::readFrame(int16_t* frame) {
    if (!mic) return false;
    if (!micFrames.read(M5.Mic, frame)) { setMicEnabled(false); return false; }
    int64_t mean = 0; for (int i = 0; i < 320; ++i) mean += frame[i]; mean /= 320;
    double energy = 0; for (int i = 0; i < 320; ++i) { double v = frame[i] - mean; energy += v*v; }
    float raw = sqrt(energy / 320); rms = raw / 8000;
    const bool aboveNoise = voiceActivity.update(raw);
    aboveBackground = aboveNoise;
    int speech = speechDetector.update(frame, aboveNoise);
    if (speech < 0) { setMicEnabled(false); return false; }
    voiced = speechDetector.accepted();
    ++diagnosticFrames; diagnosticEnergy += aboveNoise; diagnosticSpeech += speech == 1; diagnosticJoint += voiced;
    if (diagnosticFrames >= 100) {
        if (Serial.availableForWrite() >= 128) Serial.printf("[DEBUG-r13v1] frames=%u energy=%u speech=%u joint=%u\n", diagnosticFrames, diagnosticEnergy, diagnosticSpeech, diagnosticJoint);
        diagnosticFrames = diagnosticEnergy = diagnosticSpeech = diagnosticJoint = 0;
    }
    if (capturing && recording) {
        size_t count = std::min(size_t(320), conversation::InputSamples - used);
        memcpy(recording + used, frame, count * 2); used += count;
    }
    return true;
}
bool AudioTask::isVoiceDetected() { return voiced; }
bool AudioTask::isAboveBackground() { return aboveBackground; }
void AudioTask::beginListeningCalibration() {
    voiceActivity.beginCalibration(); speechDetector.reset(); voiced = aboveBackground = false;
}
uint32_t AudioTask::listeningEpoch() { return voiceActivity.epoch(); }
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
