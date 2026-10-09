#pragma once
#include <M5Unified.h>

enum AvatarEmotion {
    EMOTION_NORMAL,
    EMOTION_HAPPY,
    EMOTION_SAD,
    EMOTION_ANGRY,
    EMOTION_SURPRISED,
    EMOTION_THINKING,
    EMOTION_TALKING,
    EMOTION_SLEEP
};

class StackChanAvatar {
public:
    StackChanAvatar();
    void init(M5GFX* display);
    void update();
    void setEmotion(AvatarEmotion emotion);
    void setLipSyncLevel(float level); // 0.0 ~ 1.0 (口の開き具合)
    void triggerBlink();
    void setGaze(float x, float y);   // -1.0 ~ 1.0 (視線)

private:
    M5GFX* _gfx = nullptr;
    M5Canvas _canvas; // PSRAM上のダブルバッファスプライト (320x240)

    AvatarEmotion _emotion = EMOTION_NORMAL;
    AvatarEmotion _targetEmotion = EMOTION_NORMAL;

    // アニメーション用補間変数
    float _eyeOpen = 1.0f;
    float _mouthOpen = 0.0f;
    float _targetMouthOpen = 0.0f;
    float _gazeX = 0.0f;
    float _gazeY = 0.0f;
    float _eyebrowAngle = 0.0f; // 度数
    float _eyebrowY = 0.0f;     // オフセット

    // 瞬きタイマー
    uint32_t _lastBlinkTime = 0;
    uint32_t _nextBlinkInterval = 3000;
    bool _isBlinking = false;
    uint32_t _blinkStartTime = 0;

    void drawEyes();
    void drawEyebrows();
    void drawMouth();
};
