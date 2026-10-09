#pragma once
#include <Arduino.h>
#include <M5Unified.h>

enum AudioMode {
    AUDIO_MODE_UNINIT = 0,
    AUDIO_MODE_MIC,
    AUDIO_MODE_PLAYBACK
};

class AudioTask {
public:
    static void init(uint8_t micGain, uint8_t spkVolume);
    static void start();
    
    // I2S排他モード制御 (CoreS3 ハーフデュプレックスハードウェア対応)
    static void requestMicMode();
    static void requestPlaybackMode();
    static AudioMode getCurrentMode();

    // VAD & リップシンク値取得
    static float getLiveRMS();
    static bool isVoiceDetected();
    static uint32_t getSilenceDurationMs();
    static void resetSilenceTimer();

    // 再生キューへのPCMデータ供給
    static void enqueueAudioChunk(const uint8_t* pcmData, size_t length);
    static void finishAudioStream();
    static bool isPlaying();
    static void stopPlayback();
    static void playChirp(bool happy = true);

    // 録音機能 (STT / Gemini Multimodal用)
    static void startRecording();
    static void stopRecording();
    static bool isRecording();
    static const int16_t* getRecordedPCM(size_t* outSamples);
    static size_t getRecordedBytes();
    static bool hasMeaningfulSpeech();

private:
    static void audioWorkerTask(void* pvParameters);
};
