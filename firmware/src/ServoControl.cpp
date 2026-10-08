#include "ServoControl.h"
#include <esp32-hal-ledc.h>

// ESP32-S3の50Hzタイマーにおける最大LEDC分解能は14bit (0〜16383)
#define SERVO_LEDC_FREQ     50
#define SERVO_LEDC_RES      14
#define SERVO_CH_PAN        2      // LEDC チャンネル 2 (0~7)
#define SERVO_CH_TILT       3      // LEDC チャンネル 3

// SG90 パルス幅: 500µs (0度) ~ 2400µs (180度)
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2400

uint8_t ServoControl::s_panPin = 2;
uint8_t ServoControl::s_tiltPin = 1;
int ServoControl::s_panCenter = 90;
int ServoControl::s_tiltCenter = 90;

float ServoControl::s_currentPan = 90.0f;
float ServoControl::s_currentTilt = 90.0f;
float ServoControl::s_targetPan = 90.0f;
float ServoControl::s_targetTilt = 90.0f;
float ServoControl::s_easeSpeed = 0.12f;

uint32_t ServoControl::s_lastActionTime = 0;
int ServoControl::s_actionStep = 0;
AvatarEmotion ServoControl::s_currentEmotion = EMOTION_NORMAL;
bool ServoControl::s_isInitialized = false;

void ServoControl::writeServoPulse(uint8_t channel, int angleDeg) {
    if (!s_isInitialized) return;
    int angle = constrain(angleDeg, 0, 180);
    // 角度からパルス幅(µs)を計算
    uint32_t us = SERVO_MIN_US + (uint32_t)(angle * (SERVO_MAX_US - SERVO_MIN_US) / 180);
    // 14bit分解能 (16384ticks for 20000µs)
    uint32_t duty = (us * 16384) / 20000;
    ledcWrite(channel, duty);
}

void ServoControl::init(uint8_t panPin, uint8_t tiltPin, int panCenter, int tiltCenter) {
    s_panPin = panPin;
    s_tiltPin = tiltPin;
    s_panCenter = panCenter;
    s_tiltCenter = tiltCenter;

    s_currentPan = s_targetPan = panCenter;
    s_currentTilt = s_targetTilt = tiltCenter;

    // ESP32-S3 LEDC PWM 設定 (14bit 分解能で設定)
    ledcSetup(SERVO_CH_PAN, SERVO_LEDC_FREQ, SERVO_LEDC_RES);
    ledcAttachPin(s_panPin, SERVO_CH_PAN);

    ledcSetup(SERVO_CH_TILT, SERVO_LEDC_FREQ, SERVO_LEDC_RES);
    ledcAttachPin(s_tiltPin, SERVO_CH_TILT);

    s_isInitialized = true;

    writeServoPulse(SERVO_CH_PAN, s_panCenter);
    writeServoPulse(SERVO_CH_TILT, s_tiltCenter);

    Serial.printf("[Servo] Initialized Port A (Pan: G%d Ch%d, Tilt: G%d Ch%d, 14bit 50Hz)\n",
                  s_panPin, SERVO_CH_PAN, s_tiltPin, SERVO_CH_TILT);
}

void ServoControl::setTarget(int panDeg, int tiltDeg, float easeSpeed) {
    s_targetPan = constrain(panDeg, 30, 150);
    s_targetTilt = constrain(tiltDeg, 50, 130);
    s_easeSpeed = easeSpeed;
}

int ServoControl::getCurrentPan() {
    return (int)s_currentPan;
}

int ServoControl::getCurrentTilt() {
    return (int)s_currentTilt;
}

void ServoControl::setEmotion(AvatarEmotion emotion) {
    s_currentEmotion = emotion;
    s_actionStep = 0;
    s_lastActionTime = millis();

    switch (emotion) {
        case EMOTION_HAPPY:
            setTarget(s_panCenter, s_tiltCenter - 14, 0.20f);
            break;
        case EMOTION_THINKING:
            setTarget(s_panCenter + 15, s_tiltCenter + 12, 0.08f);
            break;
        case EMOTION_SURPRISED:
            setTarget(s_panCenter, s_tiltCenter + 18, 0.25f);
            break;
        case EMOTION_ANGRY:
            setTarget(s_panCenter - 18, s_tiltCenter, 0.20f);
            break;
        case EMOTION_SAD:
            setTarget(s_panCenter, s_tiltCenter - 18, 0.07f);
            break;
        case EMOTION_SLEEP:
            setTarget(s_panCenter, s_tiltCenter - 20, 0.05f);
            break;
        default:
            setTarget(s_panCenter, s_tiltCenter, 0.10f);
            break;
    }
}

void ServoControl::setLipSync(float rmsLevel) {
    if (s_currentEmotion != EMOTION_SLEEP) {
        float nodOffset = constrain(rmsLevel * 6.0f, 0.0f, 6.0f);
        s_targetTilt = s_tiltCenter + nodOffset;
    }
}

void ServoControl::update() {
    if (!s_isInitialized) return;

    uint32_t now = millis();

    // 感情ごとの多段階モーションシーケンス
    if (s_currentEmotion == EMOTION_HAPPY) {
        if (s_actionStep == 0 && now - s_lastActionTime > 250) {
            setTarget(s_panCenter, s_tiltCenter + 12, 0.20f);
            s_actionStep = 1;
            s_lastActionTime = now;
        } else if (s_actionStep == 1 && now - s_lastActionTime > 250) {
            setTarget(s_panCenter, s_tiltCenter, 0.15f);
            s_actionStep = 2;
        }
    } else if (s_currentEmotion == EMOTION_ANGRY) {
        if (s_actionStep == 0 && now - s_lastActionTime > 200) {
            setTarget(s_panCenter + 18, s_tiltCenter, 0.22f);
            s_actionStep = 1;
            s_lastActionTime = now;
        } else if (s_actionStep == 1 && now - s_lastActionTime > 200) {
            setTarget(s_panCenter - 18, s_tiltCenter, 0.22f);
            s_actionStep = 2;
            s_lastActionTime = now;
        } else if (s_actionStep == 2 && now - s_lastActionTime > 200) {
            setTarget(s_panCenter, s_tiltCenter, 0.15f);
            s_actionStep = 3;
        }
    } else if (s_currentEmotion == EMOTION_NORMAL) {
        if (now - s_lastActionTime > 4000) {
            int randomPan = s_panCenter + random(-8, 9);
            int randomTilt = s_tiltCenter + random(-5, 6);
            setTarget(randomPan, randomTilt, 0.04f);
            s_lastActionTime = now;
        }
    }

    // スムーズ補間 (Easing)
    s_currentPan += (s_targetPan - s_currentPan) * s_easeSpeed;
    s_currentTilt += (s_targetTilt - s_currentTilt) * s_easeSpeed;

    writeServoPulse(SERVO_CH_PAN, (int)s_currentPan);
    writeServoPulse(SERVO_CH_TILT, (int)s_currentTilt);
}
