#include <M5Unified.h>
#include <WiFi.h>
#include "ConfigManager.h"
#include "StackChanAvatar.h"
#include "AudioTask.h"
#include "CameraMotion.h"
#include "GeminiClient.h"
#include "ServoControl.h"

// アプリケーション状態マシン
enum AppState {
    STATE_SLEEP,                  // 静止待機 (カメラまたはタッチ/キーワードで復帰)
    STATE_STANDBY_WAIT_KEYWORD,   // 動体検知後、ウェイクワード発話を待機
    STATE_LISTENING,              // ユーザー発話録音中
    STATE_THINKING,               // Gemini API 直接ストリーミング問い合わせ中
    STATE_SPEAKING,               // スタックチャン発話 & リップシンク & 首振り
    STATE_WAIT_FOLLOWUP           // 会話継続待ち (一定時間無音でSLEEPへ)
};

AppConfig g_config;
StackChanAvatar g_avatar;
volatile AppState g_state = STATE_SLEEP;

TaskHandle_t hAvatarTask = nullptr;
TaskHandle_t hMotionTask = nullptr;

// 1. アバター描画 & サーボ補間タスク (Core 1 / 60FPS)
void avatarTaskCode(void* pv) {
    while (true) {
        float liveRms = AudioTask::getLiveRMS();

        if (g_state == STATE_SPEAKING) {
            g_avatar.setLipSyncLevel(liveRms * 2.5f);
            if (g_config.servo_enabled) {
                ServoControl::setLipSync(liveRms);
            }
        } else {
            g_avatar.setLipSyncLevel(0.0f);
        }

        g_avatar.update();

        if (g_config.servo_enabled) {
            ServoControl::update();
        }

        vTaskDelay(pdMS_TO_TICKS(16)); // ~60fps
    }
}

// 2. カメラ動体検知タスク (Core 0 / 低優先度)
void motionTaskCode(void* pv) {
    while (true) {
        if (g_state == STATE_SLEEP && CameraMotion::isAvailable()) {
            bool motion = CameraMotion::checkMotion(g_config.camera_motion_threshold);
            if (motion) {
                Serial.println("[System] Motion Detected! Waking up to STANDBY...");
                g_avatar.setEmotion(EMOTION_NORMAL);
                g_avatar.triggerBlink();
                if (g_config.servo_enabled) {
                    ServoControl::setEmotion(EMOTION_NORMAL);
                }
                g_state = STATE_STANDBY_WAIT_KEYWORD;
                AudioTask::resetSilenceTimer();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 10FPSでチェック
    }
}

void setup() {
    // 1. M5Unifiedの初期化 (SpeakerとMicの競合を防ぐクリーン初期化)
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_spk = true;
    cfg.internal_mic = true;
    M5.begin(cfg);

    Serial.println("=== M5Stack CoreS3 Lite Gemini Stack-chan ===");

    // 2. 設定ファイル読み込み (LittleFS破損時は自動フォーマット & デフォルト作成)
    ConfigManager::load(g_config);

    // 3. Port A SG90 サーボ初期化 (14bit LEDC PWM 50Hz)
    if (g_config.servo_enabled) {
        ServoControl::init(
            g_config.servo_pan_pin,
            g_config.servo_tilt_pin,
            g_config.servo_pan_center,
            g_config.servo_tilt_center
        );
        ServoControl::setEmotion(EMOTION_SLEEP);
    }

    // 4. アバター初期化 (320x240 LCDスプライト)
    g_avatar.init(&M5.Display);
    g_avatar.setEmotion(EMOTION_SLEEP);

    // 5. オーディオ初期化 (M5.begin()と共存する安全な初期化)
    AudioTask::init(g_config.mic_gain, g_config.spk_volume);
    AudioTask::start();

    // 6. GC0308 カメラ初期化 (PMIC ALDO3電源投入 & I2Cポート1指定)
    CameraMotion::init();

    // 7. WiFi接続 (直接Google API接続用)
    if (g_config.wifi_ssid.length() > 0) {
        Serial.printf("[WiFi] Connecting to %s...\n", g_config.wifi_ssid.c_str());
        WiFi.begin(g_config.wifi_ssid.c_str(), g_config.wifi_password.c_str());
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 25) {
            delay(400);
            Serial.print(".");
            retry++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("\n[WiFi] Connection timeout. Retrying in background...");
        }
    }

    // 8. Geminiクライアント初期化
    GeminiClient::init(g_config.gemini_api_key, g_config.gemini_model, g_config.tts_voice);

    // 9. FreeRTOS タスク生成 (Core 1: アバター&サーボ, Core 0: カメラ動体)
    xTaskCreatePinnedToCore(avatarTaskCode, "AvatarTask", 4096, nullptr, 2, &hAvatarTask, 1);
    xTaskCreatePinnedToCore(motionTaskCode, "MotionTask", 4096, nullptr, 1, &hMotionTask, 0);

    Serial.println("[System] All peripherals initialized cleanly. Entering SLEEP state.");
}

void loop() {
    M5.update();
    uint32_t silenceMs = AudioTask::getSilenceDurationMs();

    // 画面タッチでも強制ウェイクアップ可能
    if (M5.BtnA.wasClicked() || M5.Touch.getCount() > 0) {
        if (g_state == STATE_SLEEP) {
            Serial.println("[Touch] Screen tapped! Waking up...");
            g_state = STATE_LISTENING;
            g_avatar.setEmotion(EMOTION_HAPPY);
            if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_HAPPY);
            AudioTask::resetSilenceTimer();
        }
    }

    switch (g_state) {
        case STATE_SLEEP:
            break;

        case STATE_STANDBY_WAIT_KEYWORD:
            if (AudioTask::isVoiceDetected()) {
                Serial.println("[Voice] Wake sound heard! Transition to LISTENING...");
                g_avatar.setEmotion(EMOTION_HAPPY);
                if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_HAPPY);
                g_state = STATE_LISTENING;
                AudioTask::resetSilenceTimer();
            } else if (silenceMs > 8000) {
                Serial.println("[Timeout] No wake sound. Returning to SLEEP.");
                g_avatar.setEmotion(EMOTION_SLEEP);
                if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_SLEEP);
                g_state = STATE_SLEEP;
            }
            break;

        case STATE_LISTENING:
            if (AudioTask::isVoiceDetected()) {
                AudioTask::resetSilenceTimer();
            } else if (silenceMs > 1800) {
                Serial.println("[VAD] End of user speech detected. Sending to Gemini directly...");
                g_state = STATE_THINKING;
                g_avatar.setEmotion(EMOTION_THINKING);
                if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_THINKING);

                GeminiClient::sendUserPromptStream(
                    "こんにちは！元気？", // 実機録音テキスト
                    [](AvatarEmotion emo) {
                        g_avatar.setEmotion(emo);
                        if (g_config.servo_enabled) ServoControl::setEmotion(emo);
                        g_state = STATE_SPEAKING;
                    },
                    [](const String& token) {},
                    [](const uint8_t* pcm, size_t len) {
                        AudioTask::enqueueAudioChunk(pcm, len);
                    }
                );

                g_state = STATE_WAIT_FOLLOWUP;
                AudioTask::resetSilenceTimer();
            }
            break;

        case STATE_SPEAKING:
            if (!AudioTask::isPlaying()) {
                g_state = STATE_WAIT_FOLLOWUP;
                AudioTask::resetSilenceTimer();
            }
            break;

        case STATE_WAIT_FOLLOWUP:
            if (AudioTask::isVoiceDetected()) {
                g_state = STATE_LISTENING;
                AudioTask::resetSilenceTimer();
            } else if (silenceMs > (uint32_t)g_config.silence_timeout_sec * 1000) {
                Serial.println("[Timeout] Silence limit reached. Ending session.");
                g_avatar.setEmotion(EMOTION_SLEEP);
                if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_SLEEP);
                g_state = STATE_SLEEP;
            }
            break;

        default:
            break;
    }

    delay(20);
}
