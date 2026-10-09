#pragma once
#include <Arduino.h>
#include "esp_camera.h"

class CameraMotion {
public:
    static bool init();
    static bool checkMotion(int threshold);
    static int getLastMotionScore();
    static bool isAvailable();

private:
    static bool s_initialized;
    static uint8_t* s_prevFrame;
    static int s_lastScore;
    static const int IMG_W = 160; // QQVGA グレースケール
    static const int IMG_H = 120;
};
