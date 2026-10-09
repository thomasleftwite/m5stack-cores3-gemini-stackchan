#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
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

static void logWiFiEvent(arduino_event_id_t event, arduino_event_info_t info) {
    switch (event) {
        case ARDUINO_EVENT_WIFI_STA_CONNECTED:
            Serial.println("\n[WiFi] Associated with access point. Waiting for DHCP...");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("\n[WiFi] Connected! IP: %s\n", WiFi.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("\n[WiFi] Disconnected from access point. Reason: %u\n",
                          static_cast<unsigned>(info.wifi_sta_disconnected.reason));
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Serial.println("\n[WiFi] Lost IP address.");
            break;
        default:
            break;
    }
}

// WiFi credentials are managed by WiFiManager in ESP32 NVS, not config.json.
static constexpr const char* WIFI_SETUP_AP = "StackChan-Setup";

static void showWiFiPortal(WiFiManager* manager) {
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(8, 12);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.println("WiFi setup");
    M5.Display.println();
    M5.Display.println(WIFI_SETUP_AP);
    M5.Display.println("Open on your phone:");
    M5.Display.println("http://192.168.4.1");
    M5.Display.println();
    M5.Display.println("Select WiFi and Save");
    Serial.println("[WiFi] Setup AP active. Open http://192.168.4.1 on your phone.");
}

static void setupWiFi() {
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(logWiFiEvent);
    WiFiManager manager;
    manager.setDebugOutput(false); // Do not log SSIDs or passwords.
    manager.setConnectTimeout(20);
    manager.setConfigPortalTimeout(0); // Wait for setup; allow retries after a wrong password.
    manager.setAPCallback(showWiFiPortal);

    // Hold the touchscreen for 3 seconds during this 5-second startup window
    // to change networks even when the saved network is still available.
    M5.Display.fillScreen(TFT_BLACK);
    M5.Display.setCursor(8, 12);
    M5.Display.setTextSize(2);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.println("WiFi startup");
    M5.Display.println();
    M5.Display.println("To change WiFi:");
    M5.Display.println("Hold screen 3 sec");
    bool forcePortal = false;
    uint32_t holdStart = 0;
    bool holding = false;
    const uint32_t windowStart = millis();
    while (millis() - windowStart < 5000) {
        M5.update();
        if (M5.Touch.getCount() > 0) {
            if (!holding) {
                holding = true;
                holdStart = millis();
            }
            if (millis() - holdStart >= 3000) {
                forcePortal = true;
                break;
            }
        } else {
            holding = false;
        }
        delay(20);
    }

    const bool connected = forcePortal
        ? manager.startConfigPortal(WIFI_SETUP_AP)
        : manager.autoConnect(WIFI_SETUP_AP);
    if (!connected || WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Setup ended without a connection. Restarting...");
        delay(1000);
        ESP.restart();
        while (true) delay(1000);
    }
    WiFi.setAutoReconnect(true);
    configTime(9 * 3600, 0, "ntp.nict.jp", "pool.ntp.org", "time.google.com");
    Serial.println("[Time] NTP time sync configured for JST (UTC+9).");
    Serial.println("[WiFi] Setup complete.");
}

static String getFormattedJSTTime() {
    time_t now = time(nullptr);
    struct tm timeinfo;
    if (localtime_r(&now, &timeinfo) && timeinfo.tm_year > (2020 - 1900)) {
        char buf[64];
        strftime(buf, sizeof(buf), "%Y年%m月%d日 %H時%M分", &timeinfo);
        return String(buf);
    }
    return "";
}

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
    // 1. M5Unifiedの初期化 (CoreS3 オーディオコーデック給電 & ハードウェア初期化)
    // internal_spk = true, internal_mic = true に設定することで、
    // M5Unified が AXP2101 PMIC 電源レール、ES7210マイクADC (I2C 0x40)、AW88298アンプ (I2C 0x36) を正しく給電・初期化する。
    // I2Sバスの排他制御は AudioTask::init() 以降で排他直列管理を行う。
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    cfg.internal_spk = true;
    cfg.internal_mic = true;
    M5.begin(cfg);

    Serial.println("=== M5Stack CoreS3 Lite Gemini Stack-chan ===");

    // 2. 設定ファイル読み込み (LittleFS破損時は自動フォーマット & デフォルト作成)
    ConfigManager::load(g_config);
#if STACKCHAN_SERVO_OUTPUT_DISABLED
    // Keep WiFi/voice/API operation available while servo power is being corrected.
    g_config.servo_enabled = false;
    Serial.println("[Servo] Output disabled by build setting; power validation pending.");
#endif

    // Configure WiFi before starting the avatar, audio, camera or servos.
    setupWiFi();

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

    // 6. GC0308 camera initialization using the existing internal I2C bus.
    const bool cameraReady = CameraMotion::init();

    // 8. Geminiクライアント初期化
    GeminiClient::init(g_config.gemini_api_key, g_config.gemini_model, g_config.tts_voice);

    // 9. FreeRTOS タスク生成 (Core 1: アバター&サーボ, Core 0: カメラ動体)
    xTaskCreatePinnedToCore(avatarTaskCode, "AvatarTask", 4096, nullptr, 2, &hAvatarTask, 1);
    xTaskCreatePinnedToCore(motionTaskCode, "MotionTask", 4096, nullptr, 1, &hMotionTask, 0);

    Serial.printf("[System] Startup complete. Camera=%s. Entering SLEEP state.\n",
                  cameraReady ? "ready" : "unavailable (Touch/Voice mode)");
}

static uint32_t s_speechEndTimestamp = 0;
static bool s_lastRequestSuccess = true;

void loop() {
    M5.update();
    uint32_t silenceMs = AudioTask::getSilenceDurationMs();

    // 画面タッチ制御 (ウェイクアップ または 発話中断)
    if (M5.BtnA.wasClicked() || M5.Touch.getCount() > 0) {
        if (g_state == STATE_SPEAKING) {
            Serial.println("[Touch] Screen tapped during speech! Stopping playback...");
            AudioTask::stopPlayback();
            g_state = STATE_WAIT_FOLLOWUP;
            AudioTask::resetSilenceTimer();
            s_speechEndTimestamp = millis();
        } else if (g_state == STATE_SLEEP || g_state == STATE_STANDBY_WAIT_KEYWORD) {
            Serial.println("[Touch] Screen tapped! Waking up to LISTENING...");
            g_state = STATE_LISTENING;
            g_avatar.setEmotion(EMOTION_HAPPY);
            if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_HAPPY);
            AudioTask::startRecording();
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
                AudioTask::startRecording();
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
            } else if (silenceMs > 1200) {
                // 発話終端検出 (1.2秒の無音で思考状態へ移行)
                AudioTask::stopRecording();
                g_state = STATE_THINKING;
                g_avatar.setEmotion(EMOTION_THINKING);
                if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_THINKING);

                String jstTime = getFormattedJSTTime();
                if (!jstTime.isEmpty()) {
                    GeminiClient::setCurrentDateTime(jstTime);
                }

                bool requestOk = false;
                size_t samples = 0;
                const int16_t* pcm = AudioTask::getRecordedPCM(&samples);

                if (AudioTask::hasMeaningfulSpeech() && pcm && samples > 1600) {
                    Serial.printf("[VAD] User speech captured (%u samples, %u ms). Sending audio to Gemini...\n",
                                  static_cast<unsigned>(samples),
                                  static_cast<unsigned>(samples * 1000 / 16000));
                    requestOk = GeminiClient::sendUserAudioDialogue(
                        pcm,
                        samples,
                        [](AvatarEmotion emo) {
                            g_avatar.setEmotion(emo);
                            if (g_config.servo_enabled) ServoControl::setEmotion(emo);
                        },
                        [](const String& token) {},
                        [](const uint8_t* ttsPcm, size_t len) {
                            AudioTask::enqueueAudioChunk(ttsPcm, len);
                        }
                    );
                } else {
                    Serial.println("[VAD] Tap or very short prompt. Sending greeting...");
                    requestOk = GeminiClient::sendUserPromptStream(
                        "こんにちは！元気？",
                        [](AvatarEmotion emo) {
                            g_avatar.setEmotion(emo);
                            if (g_config.servo_enabled) ServoControl::setEmotion(emo);
                        },
                        [](const String& token) {},
                        [](const uint8_t* ttsPcm, size_t len) {
                            AudioTask::enqueueAudioChunk(ttsPcm, len);
                        }
                    );
                }

                if (requestOk && AudioTask::isPlaying()) {
                    s_lastRequestSuccess = true;
                    g_state = STATE_SPEAKING;
                } else if (!requestOk) {
                    s_lastRequestSuccess = false;
                    g_avatar.setEmotion(EMOTION_SAD);
                    if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_SAD);
                    Serial.println("[State] Request failed or quota exceeded. Playing error chime...");
                    AudioTask::playChirp(false); // 困惑・エラーを表現する下降トーン
                    g_state = STATE_SPEAKING;
                } else {
                    s_lastRequestSuccess = true;
                    g_state = STATE_WAIT_FOLLOWUP;
                    AudioTask::resetSilenceTimer();
                    s_speechEndTimestamp = millis();
                }
            }
            break;

        case STATE_SPEAKING:
            if (!AudioTask::isPlaying()) {
                if (s_lastRequestSuccess) {
                    Serial.println("[Speech] Audio playback finished. Transitioning to WAIT_FOLLOWUP.");
                    g_state = STATE_WAIT_FOLLOWUP;
                } else {
                    Serial.println("[Speech] Error chime finished. Returning to STANDBY (prevents rapid retry loop).");
                    g_state = STATE_STANDBY_WAIT_KEYWORD;
                }
                AudioTask::resetSilenceTimer();
                s_speechEndTimestamp = millis();
            }
            break;

        case STATE_WAIT_FOLLOWUP:
            // スピーカー再生直後の1200msクールダウン (スピーカー残響・マイク回り込みによる誤検知を遮断)
            if (millis() - s_speechEndTimestamp > 1200) {
                if (AudioTask::isVoiceDetected()) {
                    Serial.println("[Voice] Follow-up speech detected. Returning to LISTENING...");
                    g_state = STATE_LISTENING;
                    g_avatar.setEmotion(EMOTION_NORMAL);
                    if (g_config.servo_enabled) ServoControl::setEmotion(EMOTION_NORMAL);
                    AudioTask::startRecording();
                    AudioTask::resetSilenceTimer();
                    break;
                }
            }
            if (silenceMs > (uint32_t)g_config.silence_timeout_sec * 1000) {
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
