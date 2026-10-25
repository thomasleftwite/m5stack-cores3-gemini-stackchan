#pragma once
#include <Arduino.h>
#include <atomic>
#include "StackChanAvatar.h"
class GeminiClient {
 public:
    enum Stage { Idle, STT, Reply, TTS, Complete, Failed, Cancelled, ReplyRetry };
    static void init(const String& apiKey, const String& unusedModel, const String& voice);
    static bool start(const int16_t* pcm, size_t samples, const String& dateTime);
    static bool retry();
    static void cancel();
    static bool busy();
    static Stage stage();
    static int httpStatus();
    static uint32_t retryWaitMs();
    static const char* error();
    static bool hasTranscript();
    static bool needsNewQuestion();
    static const int16_t* audio(size_t* samples);
    static AvatarEmotion emotion();
    static void clear(bool clearHistory = false);
 private:
    static void worker(void*);
};
