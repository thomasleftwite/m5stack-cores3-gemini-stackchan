#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "StackChanAvatar.h"

typedef void (*EmotionCallback)(AvatarEmotion emotion);
typedef void (*TokenCallback)(const String& token);
typedef void (*AudioChunkCallback)(const uint8_t* data, size_t len);

class GeminiClient {
public:
    static void init(const String& apiKey, const String& model, const String& voice);
    static bool sendUserPromptStream(
        const String& prompt,
        EmotionCallback onEmotion,
        TokenCallback onToken,
        AudioChunkCallback onAudio
    );

private:
    static String s_apiKey;
    static String s_model;
    static String s_voice;
    static AvatarEmotion parseEmotionTag(const String& tag);
};
