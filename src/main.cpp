#include <M5Unified.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include "ConfigManager.h"
#include "StackChanAvatar.h"
#include "AudioTask.h"
#include "GeminiClient.h"
#include "ServoControl.h"
#include "ReferenceWake.h"
#include "EnrollmentWindow.h"
#include "RuntimeTrace.h"
#include "RequestFailure.h"
#include <esp_system.h>

// Diagnostic images must explicitly opt in to suppressing question capture.
#ifndef STACKCHAN_WAKE_DIAGNOSTIC_HOLD
#define STACKCHAN_WAKE_DIAGNOSTIC_HOLD 0
#endif
#if STACKCHAN_WAKE_DIAGNOSTIC_HOLD != 0 && STACKCHAN_WAKE_DIAGNOSTIC_HOLD != 1
#error "STACKCHAN_WAKE_DIAGNOSTIC_HOLD must be 0 or 1"
#endif

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
static AvatarMode faceModeFor(Mode mode) {
    switch (mode) {
        case Mode::Sleep: return AvatarMode::Sleep;
        case Mode::Cue: case Mode::Recording: return AvatarMode::Listening;
        case Mode::Busy: return AvatarMode::Thinking;
        case Mode::Speaking: return AvatarMode::Speaking;
        default: return AvatarMode::Ready;
    }
}
AppConfig g_config;
StackChanAvatar g_avatar;
ReferenceWake wake;
Mode mode = Mode::Ready;
conversation::CaptureGate gate;
uint32_t lastActivity = 0, entered = 0, lastDraw = 0;
uint32_t errorDeadline = 0;
uint32_t wakeDiagnosticAt = 0, wakeDrawMaxMs = 0;
ReferenceWake::Diagnostics wakeBaseline;
Mode micResume = Mode::Ready;
bool touching = false, cancelRequested = false;
String notice;
int16_t frame[320];
enum class EnrollmentPhase { Calibrating, Cue, Listening, Analyzing, VerificationCalibration, VerificationWarmup, Verifying, Saved };
EnrollmentPhase enrollmentPhase = EnrollmentPhase::Calibrating;
EnrollmentWindow enrollmentWindow;
uint32_t enrollmentVerificationAt = 0;
unsigned enrollmentVerificationFrames = 0;
unsigned enrollmentFirst=0, enrollmentEnd=0, enrollmentCursor=0, enrollmentClipped=0;
float enrollmentLevelSum=0,enrollmentPeak=0;
unsigned enrollmentDc=0;

static void setEmotion(AvatarEmotion e) {
    g_avatar.setEmotion(e); if (g_config.servo_enabled) ServoControl::setEmotion(e);
}
static void ready(const String& message = "") {
    AudioTask::stopPlayback(); AudioTask::stopRecording(); AudioTask::clearRecording();
    GeminiClient::clear(); wake.cancelEnrollment(); wake.resetDetection();
    M5.Display.setBrightness(100); mode = Mode::Ready; entered = lastActivity = millis();
    bool micOk = AudioTask::setMicEnabled(wake.registered());
    AudioTask::beginListeningCalibration();
    wakeDiagnosticAt = millis(); wakeDrawMaxMs = 0; wakeBaseline = wake.diagnostics();
    notice = !micOk ? "マイクの準備に失敗しました" : message;
    setEmotion(EMOTION_NORMAL);
}
static void sleepNow() {
    AudioTask::setMicEnabled(false); AudioTask::clearRecording();
    GeminiClient::clear(true); wake.resetDetection(); mode = Mode::Sleep;
    setEmotion(EMOTION_SLEEP);
    g_avatar.setMode(AvatarMode::Sleep); g_avatar.setStatus("");
    g_avatar.setLipSyncLevel(0); g_avatar.update();
    M5.Display.setBrightness(18);
}
static void enterSetting() {
    wake.resetDetection(); AudioTask::setMicEnabled(false); mode = Mode::Setting;
}
static void beginQuestion() {
    AudioTask::stopRecording(); AudioTask::clearRecording(); wake.resetDetection();
    if (!AudioTask::playCue()) { ready("開始音を再生できません"); return; }
    mode=Mode::Cue; entered=lastActivity=millis(); notice=""; lastDraw=0;
}
static void serviceQuestionCue() {
    if (mode != Mode::Cue) return;
    if (AudioTask::isPlaying()) return;
    if (!AudioTask::setMicEnabled(true)) ready("マイクの準備に失敗しました");
    else { AudioTask::startRecording(); gate.reset(millis()); mode=Mode::Recording; }
}
static String enrollmentRetentionMessage(const String& message) {
    if (wake.publicationUncertain()) return "保存結果を確認できません。再起動後に確認してください";
    return message + (wake.registered() ? "。旧登録を保持しています" : "。未登録のままです");
}
static void microphoneError(Mode resume) {
    AudioTask::stopRecording(); AudioTask::setMicEnabled(false);
    AudioTask::clearRecording();
    if (resume == Mode::Enroll) wake.cancelEnrollment();
    micResume = resume; mode = Mode::MicError;
    notice = resume == Mode::Enroll ? enrollmentRetentionMessage("マイクを再準備し、最初から登録してください") : String("マイクを再準備してください");
    lastDraw = 0;
}
static void serviceReadyWake(uint32_t now) {
    if (mode != Mode::Ready) return;
    if (now - lastActivity >= conversation::IdleMs) { sleepNow(); return; }
    if (!wake.registered()) return;
    if (!AudioTask::readFrame(frame)) { microphoneError(Mode::Ready); return; }
    // Calibrate during the existing return-to-Ready guard, not afterwards.
    if (millis() - entered < 1200 || !wake.detectWindow(frame)) return;
#if STACKCHAN_WAKE_DIAGNOSTIC_HOLD
    lastActivity = millis(); wake.resetDetection();
    Serial.println("[DEBUG-r13w2] held=1");
#else
    // Use the same cue/capture path as every question; it clears the wake window
    // and prior PCM before playback, then waits for the cue before recording.
    beginQuestion();
#endif
}
static void enrollmentFailed(const String& message) {
    AudioTask::stopPlayback(); AudioTask::stopRecording(); AudioTask::clearRecording();
    wake.cancelEnrollment(); enterSetting(); notice=enrollmentRetentionMessage(message); lastDraw=0;
}
static bool prepareEnrollment() {
    AudioTask::stopRecording(); AudioTask::clearRecording();
    if (!AudioTask::setMicEnabled(true)) return false;
    AudioTask::beginListeningCalibration(); wake.resetDetection();
    enrollmentWindow.reset();
    enrollmentPhase=EnrollmentPhase::Calibrating;
    notice="背景音を測定中。まだ話さないでください"; lastDraw=0;
    return true;
}
static void beginEnrollmentSession() {
    wake.beginEnrollment(); mode = Mode::Enroll;
    entered = lastActivity = millis(); notice = "";
    if (!prepareEnrollment()) microphoneError(Mode::Enroll);
}
static void retryMicrophone() {
    if (!AudioTask::setMicEnabled(true)) return;
    if (micResume == Mode::Ready) ready();
    // microphoneError cancels the previous transaction. A retry must create a
    // new session and collect all three takes before verification can resume.
    else beginEnrollmentSession();
}
static void cancelEnrollmentSession() {
    if (enrollmentPhase == EnrollmentPhase::Saved) { ready(notice); return; }
    enrollmentFailed("登録を中止しました");
}
static void serviceEnrollment() {
    if (mode != Mode::Enroll) return;
    if (enrollmentPhase == EnrollmentPhase::Saved) {
        if (millis() - enrollmentVerificationAt >= 3000) ready(notice);
        return;
    }
    if (enrollmentPhase == EnrollmentPhase::VerificationCalibration || enrollmentPhase == EnrollmentPhase::VerificationWarmup || enrollmentPhase == EnrollmentPhase::Verifying) {
        if (!wake.enrollmentCandidateReady()) { enrollmentFailed("登録候補を確認できません。最初から登録してください"); return; }
        const bool verifying = enrollmentPhase == EnrollmentPhase::Verifying;
        if (verifying && millis() - enrollmentVerificationAt >= 30000) {
            enrollmentFailed("待機時に呼びかけを確認できませんでした"); return;
        }
        if (!AudioTask::readFrame(frame)) { microphoneError(Mode::Enroll); return; }
        if (enrollmentPhase == EnrollmentPhase::VerificationCalibration) {
            if (millis() - enrollmentVerificationAt >= 1200) {
                wake.resetDetection(); enrollmentVerificationFrames = 0;
                enrollmentPhase = EnrollmentPhase::VerificationWarmup;
                notice = "背景音を確認中。静かに待ってください"; lastDraw = 0;
            }
            return;
        }
        // Feed the exact rolling input path while warming, but only an explicit
        // post-prompt match may authorize saving the candidate.
        const bool matched = wake.verifyEnrollmentWindow(frame, verifying);
        if (!verifying) {
            if (++enrollmentVerificationFrames >= 50) {
                enrollmentPhase = EnrollmentPhase::Verifying; enrollmentVerificationAt = millis();
                notice = "同じ呼びかけを1回話してください"; lastDraw = 0;
            }
        } else if (matched) {
            if (wake.commitEnrollment()) {
                AudioTask::setMicEnabled(false);
                enrollmentPhase = EnrollmentPhase::Saved; enrollmentVerificationAt = millis();
                notice = "呼びかけを検出し、保存しました"; lastDraw = 0;
            }
            else enrollmentFailed("保存できませんでした");
        }
        return;
    }
    if (enrollmentPhase==EnrollmentPhase::Cue) {
        if (!AudioTask::isPlaying()) {
            if (!AudioTask::setMicEnabled(true)) { microphoneError(Mode::Enroll); return; }
            // The beep has finished; keep the measured pre-beep background.
            // No post-beep recalibration or discarded second of user speech.
            AudioTask::startRecording(); enrollmentPhase=EnrollmentPhase::Listening;
            notice="5秒間録音します。同じ呼びかけを1回話してください"; lastDraw=0;
        }
        return;
    }
    if (enrollmentPhase==EnrollmentPhase::Analyzing) {
        size_t count=0; const int16_t* pcm=AudioTask::getRecordedPCM(&count);
        if (!pcm || count<enrollmentEnd*320) { enrollmentFailed("録音を確認できません。最初から登録してください"); return; }
        const int16_t* part=pcm+enrollmentCursor*320;
        float level=EnrollmentWindow::level(part); enrollmentLevelSum+=level; enrollmentPeak=fmaxf(enrollmentPeak,level);
        int32_t dc=0; for(unsigned i=0;i<320;++i) dc+=part[i]; dc/=320;
        enrollmentDc=std::max(enrollmentDc,unsigned(dc<0 ? -dc : dc));
        for (unsigned i=0;i<320;++i) if (part[i]>32000 || part[i]<-32000) ++enrollmentClipped;
        if (++enrollmentCursor<enrollmentEnd) return;
#ifdef ARDUINO
        Serial.printf("[DEBUG-sv2] take=%u noise=%u warm=%u gate=%u rms=%u peak=%u dc=%u clips=%u\n",wake.enrollmentCount()+1,
            unsigned(enrollmentWindow.backgroundRms()),unsigned(enrollmentWindow.warmedBackgroundRms()),unsigned(enrollmentWindow.triggerRms()),
            unsigned(enrollmentLevelSum/(enrollmentEnd-enrollmentFirst)),unsigned(enrollmentPeak),enrollmentDc,enrollmentClipped);
#endif
        if (enrollmentClipped>=100) { enrollmentFailed("音が大きすぎました。最初から登録してください"); return; }
        int result=wake.submitEnrollment(pcm+enrollmentFirst*320,(enrollmentEnd-enrollmentFirst)*320);
        runtimetrace::mark(runtimetrace::EnrollmentResult, unsigned(result+2));
        AudioTask::clearRecording();
        if (result==2) {
            if (!wake.enrollmentCandidateReady()) { enrollmentFailed("登録候補を確認できません。最初から登録してください"); return; }
            if (!AudioTask::setMicEnabled(true)) { microphoneError(Mode::Enroll); return; }
            AudioTask::beginListeningCalibration(); wake.resetDetection();
            enrollmentPhase = EnrollmentPhase::VerificationCalibration; enrollmentVerificationAt = millis();
            enrollmentVerificationFrames = 0;
            notice = "3回の声を確認しました。まだ話さないでください"; lastDraw = 0;
        }
        else if (result==-2) enrollmentFailed("登録候補を作成できませんでした");
        else if (result==-1) enrollmentFailed("3回の声が一致しません。最初から登録してください");
        else if (!prepareEnrollment()) microphoneError(Mode::Enroll);
        return;
    }
    if (!AudioTask::readFrame(frame)) { microphoneError(Mode::Enroll); return; }
    if (enrollmentPhase==EnrollmentPhase::Calibrating) {
        if (enrollmentWindow.background(frame)) {
            if (!AudioTask::playCue()) { enrollmentFailed("開始音を再生できません"); return; }
            enrollmentPhase=EnrollmentPhase::Cue;
            notice="合図の後に同じ呼びかけを話してください"; lastDraw=0;
        }
        return;
    }
    if (!enrollmentWindow.capture(frame)) return;
    AudioTask::stopRecording(); AudioTask::setMicEnabled(false);
    unsigned narrowFirst=0,narrowEnd=0;
    bool narrowOk=enrollmentWindow.range(narrowFirst,narrowEnd,10);
    bool cropOk=enrollmentWindow.range(enrollmentFirst,enrollmentEnd);
#ifdef ARDUINO
    Serial.printf("[DEBUG-e06] take=%u first=%u end=%u narrow_first=%u narrow_end=%u\n",wake.enrollmentCount()+1,
        cropOk ? enrollmentFirst : 0,cropOk ? enrollmentEnd : 0,narrowOk ? narrowFirst : 0,narrowOk ? narrowEnd : 0);
#else
    (void)narrowOk;
#endif
    if (!cropOk) {
        enrollmentFailed("声の区間を確認できません。最初から登録してください"); return;
    }
    enrollmentCursor=enrollmentFirst; enrollmentClipped=0;
    enrollmentLevelSum=enrollmentPeak=0; enrollmentDc=0;
    enrollmentPhase=EnrollmentPhase::Analyzing; notice="録音を切り出しています"; lastDraw=0;
}
static void requestError() {
    Serial.printf("[DEBUG-r13d2] api_stage=%u status=%d reason=%u\n",
        unsigned(GeminiClient::stage()), GeminiClient::httpStatus(),
        unsigned(requestfailure::classify(GeminiClient::error())));
    mode = Mode::Error; entered = millis(); notice = "";
    errorDeadline = conversation::retryDeadline(entered, GeminiClient::retryWaitMs());
    setEmotion(EMOTION_SAD);
}
static void drawMenu(const String& title, const String& action, const String& detail) {
    g_avatar.drawMenu(title, action, detail);
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
static String readyStatus() {
#if STACKCHAN_WAKE_DIAGNOSTIC_HOLD
    return String("診断中：質問録音は停止") + (notice.length() ? String(" / ") + notice : String(""));
#else
    return ""; // UI04 keeps the normal Ready face free of status text.
#endif
}
void setup() {
    auto cfg = M5.config(); cfg.serial_baudrate = 115200; cfg.internal_spk = true; cfg.internal_mic = true;
    M5.begin(cfg);
    Serial.println("[System] CoreS3 file conversation R13.4");
    Serial.println("[UI] revision=UI04");
    Serial.println("[DIAG] revision=S08");
    Serial.printf("[Wake] diagnostic_hold=%u\n", unsigned(STACKCHAN_WAKE_DIAGNOSTIC_HOLD));
    runtimetrace::mark(runtimetrace::Boot, unsigned(esp_reset_reason()));
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
    Serial.printf("[Wake] registered=%d\n", int(wake.registered()));
    GeminiClient::init(g_config.gemini_api_key, g_config.tts_voice);
    ready();
    Serial.println("[System] ready=1");
}
void loop() {
    static int traceMode = -1;
    static uint32_t traceTime = 0;
    if (traceMode != int(mode)) {
        traceMode = int(mode); runtimetrace::mark(runtimetrace::ModeChanged, unsigned(mode));
    }
    if (millis() - traceTime >= 5000) {
        traceTime = millis(); runtimetrace::mark(runtimetrace::Tick, unsigned(mode));
    }
    M5.update();
    uint32_t now = millis();
    bool down = M5.Touch.getCount() > 0;
    bool tap = (down && !touching) || M5.BtnA.wasClicked();
    touching = down;
    int y = down ? M5.Touch.getDetail().y : 200;
    if (tap) {
        lastActivity = now;
        switch (mode) {
            case Mode::Sleep: ready(); break;
            case Mode::Ready: enterSetting(); break;
            case Mode::Setting:
                if (y >= 85 && y <= 145) {
                    beginEnrollmentSession();
                } else if (y >= 180) ready();
                break;
            case Mode::Enroll: if (y >= 180) cancelEnrollmentSession(); break;
            case Mode::MicError:
                if (y >= 85 && y <= 145) {
                    retryMicrophone();
                } else if (y >= 180) {
                    if (micResume == Mode::Enroll) cancelEnrollmentSession();
                    else ready();
                }
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
        serviceReadyWake(now);
    } else if (mode == Mode::Enroll) {
        serviceEnrollment();
    } else if (mode == Mode::Cue) {
        serviceQuestionCue();
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

    if (mode == Mode::Ready && millis() - wakeDiagnosticAt >= 2000) {
        const auto& d = wake.diagnostics();
        char diagnosticLine[256];
        const int diagnosticBytes = snprintf(diagnosticLine, sizeof(diagnosticLine), "[DEBUG-r13w1] window_ms=%u ready_ms=%u frames=%u voice=%u complete=%u short=%u clipped=%u long=%u matched=%u last_matches=%u draw_max_ms=%u\n",
            unsigned(millis() - wakeDiagnosticAt), unsigned(millis() - entered),
            unsigned(d.frames - wakeBaseline.frames), unsigned(d.voice - wakeBaseline.voice),
            unsigned(d.complete - wakeBaseline.complete), unsigned(d.tooShort - wakeBaseline.tooShort),
            unsigned(d.clipped - wakeBaseline.clipped), unsigned(d.tooLong - wakeBaseline.tooLong),
            unsigned(d.matched - wakeBaseline.matched), d.lastMatches, unsigned(wakeDrawMaxMs));
        if (diagnosticBytes > 0 && diagnosticBytes < sizeof(diagnosticLine) && Serial.availableForWrite() >= diagnosticBytes)
            Serial.write(reinterpret_cast<const uint8_t*>(diagnosticLine), diagnosticBytes);
        wakeBaseline = d; wakeDiagnosticAt = millis(); wakeDrawMaxMs = 0;
    }
    // A full face transfer takes ~46 ms. Leave time for 50 microphone frames/s.
    const uint32_t drawInterval = (mode == Mode::Ready || mode == Mode::Recording) ? 100 : 50;
    if (mode != Mode::Sleep && millis() - lastDraw >= drawInterval) {
        const uint32_t drawStarted = millis();
        lastDraw = millis();
        if (mode == Mode::Setting) drawMenu("設定", "ウェイクワード登録 / 更新", notice.isEmpty() ? "同じ人が同じ呼びかけを3回録音" : notice);
        else if (mode == Mode::Enroll) drawMenu("呼びかけの登録",
            enrollmentPhase == EnrollmentPhase::Saved ? String("登録できました") :
            enrollmentPhase == EnrollmentPhase::VerificationCalibration || enrollmentPhase == EnrollmentPhase::VerificationWarmup ? String("静かに待ってください") :
            enrollmentPhase == EnrollmentPhase::Verifying ? String("同じ呼びかけを1回") :
            String(wake.enrollmentCount() + 1) + (enrollmentPhase == EnrollmentPhase::Listening ? " / 3  5秒間録音中" : enrollmentPhase == EnrollmentPhase::Analyzing ? " / 3  録音を確認中" : " / 3  背景音・合図の準備"), notice);
        else if (mode == Mode::MicError) drawMenu("録音できませんでした", "マイクを再準備", notice);
        else if (mode == Mode::Error) drawMenu("応答できませんでした", "失敗した処理から再試行", notice.isEmpty() ? requestErrorText() : notice);
        else {
            g_avatar.setMode(faceModeFor(mode));
            String status = mode == Mode::Ready ? readyStatus() :
                mode == Mode::Cue ? "合図の後に話してください" : mode == Mode::Recording ? "録音中 / タッチで中止" :
                mode == Mode::Speaking ? "お話し中 / タッチで中止" : stageText() + " / タッチで中止";
            g_avatar.setStatus(status); g_avatar.setLipSyncLevel(mode == Mode::Speaking ? AudioTask::getLiveRMS() * 2.5f : 0);
            g_avatar.update();
        }
        const uint32_t drawMs = millis() - drawStarted;
        if (drawMs > wakeDrawMaxMs) wakeDrawMaxMs = drawMs;
    }
    if (g_config.servo_enabled) { ServoControl::setLipSync(mode == Mode::Speaking ? AudioTask::getLiveRMS() : 0); ServoControl::update(); }
    delay(1);
}
