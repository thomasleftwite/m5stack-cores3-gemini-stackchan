#pragma once
#include <Arduino.h>
#include "StackChanAvatar.h"

class ServoControl {
public:
    static void init(uint8_t panPin = 2, uint8_t tiltPin = 1, int panCenter = 90, int tiltCenter = 90);
    static void update();
    static void setEmotion(AvatarEmotion emotion);
    static void setLipSync(float rmsLevel);
    static void setTarget(int panDeg, int tiltDeg, float easeSpeed = 0.12f);
    static int getCurrentPan();
    static int getCurrentTilt();

private:
    static uint8_t s_panPin;
    static uint8_t s_tiltPin;
    static int s_panCenter;
    static int s_tiltCenter;

    static float s_currentPan;
    static float s_currentTilt;
    static float s_targetPan;
    static float s_targetTilt;
    static float s_easeSpeed;

    static uint32_t s_lastActionTime;
    static int s_actionStep;
    static AvatarEmotion s_currentEmotion;
    static bool s_isInitialized;

    static void writeServoPulse(uint8_t channel, int angleDeg);
};
