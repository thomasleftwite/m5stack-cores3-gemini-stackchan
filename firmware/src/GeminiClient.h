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
    static void setCurrentDateTime(const String& dateTimeStr);
    
    // テキスト入力によるストリーミング対話 + 自動TTS音声合成
    static bool sendUserPromptStream(
        const String& prompt,
        EmotionCallback onEmotion,
        TokenCallback onToken,
        AudioChunkCallback onAudio
    );

    // 実機マイク録音PCM (16kHz mono) によるマルチモーダル音声対話 (STT + LLM + TTS)
    static bool sendUserAudioDialogue(
        const int16_t* pcmSamples,
        size_t sampleCount,
        EmotionCallback onEmotion,
        TokenCallback onToken,
        AudioChunkCallback onAudio
    );

    // テキストから24kHz PCM音声を合成して再生ストリームへ供給
    static bool generateTTS(
        const String& text,
        AudioChunkCallback onAudio
    );

private:
    static String s_apiKey;
    static String s_model;
    static String s_voice;
    static String s_currentDateTime;
    static AvatarEmotion parseEmotionTag(const String& tag);
    static String extractCleanText(const String& rawText);
    static String extractHeardText(const String& rawText);
};
