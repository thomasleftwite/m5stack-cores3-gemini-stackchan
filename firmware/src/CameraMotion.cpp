#include "CameraMotion.h"
#include <M5Unified.h>

// M5Stack CoreS3 / CoreS3 Lite GC0308 カメラピンアサイン
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     -1
#define SIOD_GPIO_NUM     12
#define SIOC_GPIO_NUM     11
#define Y9_GPIO_NUM       47
#define Y8_GPIO_NUM       48
#define Y7_GPIO_NUM       16
#define Y6_GPIO_NUM       15
#define Y5_GPIO_NUM       42
#define Y4_GPIO_NUM       41
#define Y3_GPIO_NUM       40
#define Y2_GPIO_NUM       39
#define VSYNC_GPIO_NUM    46
#define HREF_GPIO_NUM     38
#define PCLK_GPIO_NUM     45

bool CameraMotion::s_initialized = false;
uint8_t* CameraMotion::s_prevFrame = nullptr;
int CameraMotion::s_lastScore = 0;

bool CameraMotion::isAvailable() {
    return s_initialized;
}

bool CameraMotion::init() {
    // M5.begin() initializes the CoreS3 PMIC; ensure the camera's ALDO3 rail is on.
    M5.Power.Axp2101.setALDO3(3300);
    delay(100);

    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM; // 12
    config.pin_sccb_scl = SIOC_GPIO_NUM; // 11
    config.sccb_i2c_port = 1;            // M5内部I2Cポート0との衝突を回避するためI2Cポート1を指定
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_QQVGA; // 160x120
    config.pixel_format = PIXFORMAT_GRAYSCALE;
    config.grab_mode = CAMERA_GRAB_LATEST;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.fb_count = 1;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[Camera] Init warning: 0x%x (Fallback: Touch/Voice mode will continue safely)\n", err);
        s_initialized = false;
        return false;
    }

    s_prevFrame = (uint8_t*)ps_malloc(IMG_W * IMG_H);
    if (!s_prevFrame) {
        Serial.println("[Camera] Failed to alloc prevFrame buffer in PSRAM");
        s_initialized = false;
        return false;
    }
    memset(s_prevFrame, 0, IMG_W * IMG_H);

    s_initialized = true;
    Serial.println("[Camera] GC0308 Initialized successfully with QQVGA Motion Detector");
    return true;
}

bool CameraMotion::checkMotion(int threshold) {
    if (!s_initialized || !s_prevFrame) return false;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;

    int changedPixels = 0;
    const int step = 4;
    const int diffThreshold = 18;

    for (int y = 0; y < IMG_H; y += step) {
        int rowIdx = y * IMG_W;
        for (int x = 0; x < IMG_W; x += step) {
            int idx = rowIdx + x;
            int diff = abs((int)fb->buf[idx] - (int)s_prevFrame[idx]);
            if (diff > diffThreshold) {
                changedPixels++;
            }
            s_prevFrame[idx] = (uint8_t)((s_prevFrame[idx] * 4 + fb->buf[idx]) / 5);
        }
    }

    esp_camera_fb_return(fb);

    int totalSampled = (IMG_W / step) * (IMG_H / step);
    s_lastScore = (changedPixels * 100) / totalSampled;

    return s_lastScore >= threshold;
}

int CameraMotion::getLastMotionScore() {
    return s_lastScore;
}
