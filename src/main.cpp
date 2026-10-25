#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "ConfigManager.h"
#include "StackChanAvatar.h"
#include "AudioTask.h"
#include "GeminiClient.h"
#include "ServoControl.h"
#include "LocalWake.h"
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


enum class Mode { Ready, Sleep, Setting, Enroll, Cue, Recording, Busy, Speaking, Error, MicError };
AppConfig g_config;
StackChanAvatar g_avatar;
LocalWake wake;
Mode mode = Mode::Ready;
conversation::CaptureGate gate;
uint32_t lastActivity = 0, entered = 0, lastDraw = 0;
uint32_t errorDeadline = 0;
Mode micResume = Mode::Ready;
bool touching = false, cancelRequested = false;
String notice;
int16_t frame[320];

static void setEmotion(AvatarEmotion e) {
    g_avatar.setEmotion(e); if (g_config.servo_enabled) ServoControl::setEmotion(e);
}
static void ready(const String& message = "") {
    AudioTask::stopPlayback(); AudioTask::stopRecording(); AudioTask::clearRecording();
    GeminiClient::clear(); wake.cancelEnrollment(); wake.resetDetection();
    M5.Display.setBrightness(100); mode = Mode::Ready; entered = lastActivity = millis();
    bool micOk = AudioTask::setMicEnabled(wake.registered());
    notice = !micOk ? "マイクの準備に失敗しました" : message;
    setEmotion(EMOTION_NORMAL);
}
static void sleepNow() {
    AudioTask::setMicEnabled(false); AudioTask::clearRecording();
    GeminiClient::clear(true); wake.resetDetection(); mode = Mode::Sleep;
    setEmotion(EMOTION_SLEEP); M5.Display.setBrightness(0);
}
static void enterSetting() {
    wake.resetDetection(); AudioTask::setMicEnabled(false); mode = Mode::Setting;
}
static void microphoneError(Mode resume) {
    AudioTask::stopRecording(); AudioTask::setMicEnabled(false);
    AudioTask::clearRecording();
    micResume = resume; mode = Mode::MicError; notice = "マイクを再準備してください";
}
static void requestError() {
    mode = Mode::Error; entered = millis(); notice = "";
    errorDeadline = conversation::retryDeadline(entered, GeminiClient::retryWaitMs());
    setEmotion(EMOTION_SAD);
}
static void drawMenu(const String& title, const String& action, const String& detail) {
    M5.Display.fillScreen(0x18E3); M5.Display.setFont(&fonts::efontJA_16);
    M5.Display.setTextColor(TFT_WHITE, 0x18E3); M5.Display.setTextSize(1);
    M5.Display.setCursor(12, 15); M5.Display.println(title);
    M5.Display.setCursor(12, 45); M5.Display.println(detail);
    M5.Display.fillRoundRect(10, 85, 300, 60, 8, 0x249f);
    M5.Display.setCursor(20, 105); M5.Display.println(action);
    M5.Display.setCursor(20, 195); M5.Display.println("戻る / キャンセル");
}
static String stageText() {
    if (cancelRequested) return "キャンセル処理中";
    switch (GeminiClient::stage()) {
        case GeminiClient::STT: return "音声を認識しています";
        case GeminiClient::Reply: return "返事を考えています";
        case GeminiClient::ReplyRetry: return "混雑のため、もう一度問い合わせています";
        case GeminiClient::TTS: return "声を作っています";
        default: return "処理しています";
    }
}
static String requestErrorText() {
    const int code = GeminiClient::httpStatus();
    if (code == 503) return "Geminiが混雑中です。少し待って再試行";
    if (code == 429) return "無料枠の上限です。しばらくお待ちください";
    if (code == 401 || code == 403) return "APIキーと利用権限を確認してください";
    if (code == 404) return "指定モデルが利用できません";
    if (code <= 0) return "通信が完了しませんでした。接続を確認";
    return "応答を確認できません。再試行できます";
}
void setup() {
    auto cfg = M5.config(); cfg.serial_baudrate = 115200; cfg.internal_spk = true; cfg.internal_mic = true;
    M5.begin(cfg);
    Serial.println("[System] CoreS3 file conversation R13.1");
    if (!ConfigManager::load(g_config)) {
        M5.Display.println("Settings storage unavailable; no automatic format.");
        while (true) delay(100);
    }
#if STACKCHAN_SERVO_OUTPUT_DISABLED
    g_config.servo_enabled = false;
#endif
    setupWiFi();
    if (g_config.servo_enabled) ServoControl::init(g_config.servo_pan_pin, g_config.servo_tilt_pin, g_config.servo_pan_center, g_config.servo_tilt_center);
    g_avatar.init(&M5.Display);
    if (!AudioTask::init(g_config.mic_gain, g_config.spk_volume)) {
        M5.Display.println("Audio memory or microphone unavailable.");
        while (true) delay(100);
    }
    wake.begin();
    GeminiClient::init(g_config.gemini_api_key, g_config.gemini_model, g_config.tts_voice);
    ready();
}
void loop() {
    M5.update();
    uint32_t now = millis();
    bool down = M5.Touch.getCount() > 0;
    bool tap = (down && !touching) || M5.BtnA.wasClicked();
    touching = down;
    int x = down ? M5.Touch.getDetail().x : 0, y = down ? M5.Touch.getDetail().y : 200;
    if (tap) {
        lastActivity = now;
        switch (mode) {
            case Mode::Sleep: ready(); break;
            case Mode::Ready: if (x >= 215 && y < 45) enterSetting(); break;
            case Mode::Setting:
                if (y >= 85 && y <= 145) {
                    wake.beginEnrollment(); mode = Mode::Enroll; notice = "";
                    if (!AudioTask::setMicEnabled(true)) { mode = Mode::Setting; notice = "マイクの準備に失敗しました"; }
                } else if (y >= 180) ready();
                break;
            case Mode::Enroll: if (y >= 180) ready("登録を中止しました"); break;
            case Mode::MicError:
                if (y >= 85 && y <= 145) {
                    if (AudioTask::setMicEnabled(true)) {
                        wake.resetDetection(); mode = micResume; entered = lastActivity = millis(); notice = "";
                    }
                } else if (y >= 180) ready();
                break;
            case Mode::Busy: cancelRequested = true; GeminiClient::cancel(); break;
            case Mode::Error:
                if (y >= 85 && y <= 145) {
                    if (GeminiClient::retry()) { cancelRequested = false; mode = Mode::Busy; }
                    else notice = "少し待って再試行してください";
                } else if (y >= 180) ready();
                break;
            default: ready("中止しました"); break;
        }
        lastDraw = 0;
    }
    now = millis(); // A transition may have advanced entered/lastActivity during I2S setup.
    AudioTask::servicePlayback();
    if (mode == Mode::Ready) {
        if (now - lastActivity >= conversation::IdleMs) sleepNow();
        else if (wake.registered() && now - entered >= 1200) {
            if (!AudioTask::readFrame(frame)) microphoneError(Mode::Ready);
            else if (wake.detect(frame, AudioTask::isVoiceDetected())) {
                lastActivity = now; wake.resetDetection();
                if (AudioTask::playCue()) mode = Mode::Cue;
                else ready("開始音を再生できません");
            }
        }
    } else if (mode == Mode::Enroll) {
        if (!AudioTask::readFrame(frame)) microphoneError(Mode::Enroll);
        else {
            int result = wake.enroll(frame, AudioTask::isVoiceDetected());
            if (result == 2) ready("呼びかけを保存しました");
            else if (result == -2) ready("保存できません。旧登録を保持");
            else if (result == -1) { notice = "同じ言葉でもう一度"; lastDraw = 0; }
            else if (result == 1) { notice = "次も同じ言葉を話してください"; lastDraw = 0; }
        }
    } else if (mode == Mode::Cue && !AudioTask::isPlaying()) {
        if (!AudioTask::setMicEnabled(true)) ready("マイクの準備に失敗しました");
        else { AudioTask::startRecording(); gate.reset(millis()); mode = Mode::Recording; }
    } else if (mode == Mode::Recording) {
        if (!AudioTask::readFrame(frame)) microphoneError(Mode::Ready);
        else {
            auto result = gate.update(millis(), AudioTask::isVoiceDetected());
            if (result != conversation::CaptureGate::Waiting || AudioTask::recordingFull()) {
                AudioTask::stopRecording(); AudioTask::setMicEnabled(false);
                if (result == conversation::CaptureGate::Empty || gate.voicedMs < 200) ready("声を確認できませんでした");
                else {
                    size_t samples = 0; const int16_t* pcm = AudioTask::getRecordedPCM(&samples);
                    cancelRequested = false;
                    if (GeminiClient::start(pcm, samples, getFormattedJSTTime())) { mode = Mode::Busy; entered = millis(); setEmotion(EMOTION_THINKING); }
                    else requestError();
                }
            }
        }
    } else if (mode == Mode::Busy) {
        if (GeminiClient::hasTranscript()) AudioTask::clearRecording();
        if (!GeminiClient::busy()) {
            if (cancelRequested || GeminiClient::stage() == GeminiClient::Cancelled) ready("中止しました");
            else if (GeminiClient::stage() == GeminiClient::Complete) {
                size_t samples = 0; const int16_t* audio = GeminiClient::audio(&samples);
                setEmotion(GeminiClient::emotion());
                if (AudioTask::playPCM(audio, samples)) mode = Mode::Speaking;
                else ready("音声を再生できません");
            } else if (GeminiClient::needsNewQuestion()) ready("聞き取れません。もう一度呼びかけてください");
            else requestError();
        }
    } else if (mode == Mode::Speaking && !AudioTask::isPlaying()) ready();
    else if (mode == Mode::Error && conversation::deadlineReached(now, errorDeadline)) ready("処理を終了しました");

    if (mode != Mode::Sleep && millis() - lastDraw >= 50) {
        lastDraw = millis();
        if (mode == Mode::Setting) drawMenu("設定", "ウェイクワード登録 / 更新", notice.isEmpty() ? "同じ人が同じ呼びかけを3回録音" : notice);
        else if (mode == Mode::Enroll) drawMenu("呼びかけの登録", String(wake.enrollmentCount() + 1) + " / 3  同じ言葉を話してください", notice);
        else if (mode == Mode::MicError) drawMenu("録音できませんでした", "マイクを再準備", notice);
        else if (mode == Mode::Error) drawMenu("応答できませんでした", "失敗した処理から再試行", notice.isEmpty() ? requestErrorText() : notice);
        else {
            String status = mode == Mode::Ready ? (notice.isEmpty() ? (wake.registered() ? "呼びかけを待っています" : "設定から呼びかけを登録してください") : notice) :
                mode == Mode::Cue ? "合図の後に話してください" : mode == Mode::Recording ? "録音中 / タッチで中止" :
                mode == Mode::Speaking ? "お話し中 / タッチで中止" : stageText() + " / タッチで中止";
            g_avatar.setStatus(status); g_avatar.setLipSyncLevel(mode == Mode::Speaking ? AudioTask::getLiveRMS() * 2.5f : 0);
            g_avatar.update();
            if (mode == Mode::Ready) {
                M5.Display.fillRoundRect(220, 5, 95, 32, 5, 0x249f);
                M5.Display.setFont(&fonts::efontJA_16); M5.Display.setCursor(250, 12); M5.Display.print("設定");
            }
        }
    }
    if (g_config.servo_enabled) { ServoControl::setLipSync(mode == Mode::Speaking ? AudioTask::getLiveRMS() : 0); ServoControl::update(); }
    delay(1);
}
