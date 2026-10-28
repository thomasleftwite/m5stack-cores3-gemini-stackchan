#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "ConversationRules.h"
// UI loop owns I2S. The network worker never touches microphone/speaker.
class AudioTask {
 public:
    static bool init(uint8_t micGain, uint8_t volume);
    static bool setMicEnabled(bool enabled);
    static bool readFrame(int16_t* frame);
    static bool isVoiceDetected();
    static bool isAboveBackground();
    static void beginListeningCalibration();
    static uint32_t listeningEpoch();
    static float getLiveRMS();
    static void startRecording();
    static void stopRecording();
    static void clearRecording();
    static const int16_t* getRecordedPCM(size_t* count);
    static bool recordingFull();
    static bool playPCM(const int16_t* data, size_t samples);
    static bool playCue();
    static void servicePlayback();
    static bool isPlaying();
    static void stopPlayback();
};
