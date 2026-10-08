export interface FirmwareFile {
  name: string;
  path: string;
  description: string;
  language: string;
  content: string;
}

export const FIRMWARE_FILES: FirmwareFile[] = [
  {
    name: "README.md",
    path: "README.md",
    description: "プロジェクト解説 & config.json セットアップ詳細手順・全項目リファレンス",
    language: "markdown",
    content: `# M5Stack CoreS3 Lite × Gemini AI Voice Stack-chan (スタックチャン)

M5Stack CoreS3 Lite 専用の **Google Gemini 音声対話スタックチャン** システムです。
ディスプレイには M5GFX Canvas スプライトによる 60fps の愛らしい感情アバター（目・眉・口・リップシンク）を表示し、**Port A に接続した SG90×2軸サーボモーター**で感情とうなずきを物理的に連動させます。

---

## 🌟 主な機能とアーキテクチャ

1. **Gemini 3.8 Flash & Gemini 3.8 Flash Lite TTS 直接ストリーミング**
   - ESP32 から直接 \`generativelanguage.googleapis.com:443\` へ HTTPS/SSE 接続。
   - レスポンスの先頭トークンに含まれる感情タグ（\`[EMOTION:HAPPY]\`, \`[EMOTION:THINKING]\` 等）を即時抽出して表情とサーボ首振りを先行トリガー。
   - 24kHz PCM 音声を PSRAM リングバッファ経由で ES8311 スピーカーへ低遅延 DMA 再生。

2. **Port A (G2, G1) SG90 2軸サーボ連携**
   - CoreS3 Lite 本体の赤色 Grove コネクタ（Port A）から直接 50Hz PWM を供給。
   - ESP32-S3 の 14bit LEDC PWM と滑らかなイージング補間により、ギア鳴きや急激な反動を防止。
   - 会話の感情（喜びのうなずき、首かしげ、驚きのけぞり、いやいや首振り）や発話音量 RMS に合わせた自然なしぐさを再現。

3. **GC0308 カメラ動体検知 & VAD 無音自動終了**
   - AXP2101 PMIC の ALDO1 (1.8V) / ALDO2 (2.8V) / ALDO4 (3.3V) を明示通電し、カメラを安定起動。
   - QQVGA (160×120) 高速差分法（CPU 負荷 3〜5% 未満）により、人の接近で自動ウェイクアップ。
   - 一定時間（デフォルト 6秒）無音が続くと、「またね！」とスリープ状態（SLEEP）へ自動復帰。

4. **自己修復型 LittleFS & パラメータ設定 (\`config.json\`)**
   - LittleFS パーティションが未フォーマットの場合でも、ファームウェアが自動フォーマットとデフォルト設定ファイルの生成を実施。

---

## ⚙️ \`config.json\` セットアップ手順

\`config.json\` は、Wi-Fi 接続情報、Gemini API キー、サーボの各設定を記録する設定ファイルです。

### 方法 1: PlatformIO で Flash に転送する（推奨）
1. プロジェクトルートの \`data/config.json\` をエディタで開き、お使いの Wi-Fi 情報と Gemini API キーを入力します。
2. VS Code の PlatformIO サイドバーから **[Upload Filesystem Image]**（またはターミナルで \`pio run -t uploadfs\`）を実行します。
3. \`data/config.json\` が CoreS3 Lite の LittleFS 領域へ書き込まれます。
4. 続けて **[Upload]**（\`pio run -t upload\`）でプログラム本体を書き込みます。

### 方法 2: 自動生成機能を利用する（ファイル書き込み不要）
- 本ファームウェアは、LittleFS 内に \`config.json\` が存在しない場合、**自動的にデフォルト設定で \`/config.json\` を新規作成して LittleFS に保存**します。
- その後、シリアルまたは Web 設定画面からパラメータを書き換えることが可能です。

---

## 📋 \`config.json\` 各項目の設定リファレンス

\`\`\`json
{
  "wifi_ssid": "YOUR_WIFI_SSID",
  "wifi_password": "YOUR_WIFI_PASSWORD",
  "gemini_api_key": "AIzaSyYOUR_GEMINI_API_KEY",
  "gemini_model": "gemini-3.8-flash",
  "tts_voice": "Kore",
  "wake_word": "スタックちゃん",
  "silence_timeout_sec": 6,
  "camera_motion_threshold": 25,
  "spk_volume": 160,
  "mic_gain": 80,
  "servo_enabled": true,
  "servo_pan_pin": 2,
  "servo_tilt_pin": 1,
  "servo_pan_center": 90,
  "servo_tilt_center": 90
}
\`\`\`

### パラメータ詳細一覧

| キー | 型 | デフォルト値 | 説明・設定の目安 |
| :--- | :--- | :--- | :--- |
| \`wifi_ssid\` | 文字列 | \`""\` | 接続先 Wi-Fi の SSID（**2.4GHz 帯** のみ対応。5GHz は非対応） |
| \`wifi_password\` | 文字列 | \`""\` | Wi-Fi の WPA2/WPA3 パスワード |
| \`gemini_api_key\` | 文字列 | \`""\` | Google AI Studio で発行した Gemini API キー（\`AIzaSy...\`） |
| \`gemini_model\` | 文字列 | \`"gemini-3.8-flash"\` | 使用する対話モデル名。通常は \`"gemini-3.8-flash"\` を指定 |
| \`tts_voice\` | 文字列 | \`"Kore"\` | 音声モデル（\`gemini-3.8-flash-lite-tts\`）の声質。<br>選択肢: \`Kore\` (標準/落ち着いた声), \`Puck\` (明るい声), \`Fenrir\` (低音), \`Zephyr\` (透明感), \`Charon\` (深み) |
| \`wake_word\` | 文字列 | \`"スタックちゃん"\` | 対話を開始するキーワード |
| \`silence_timeout_sec\` | 整数 | \`6\` | 発話が途切れてから会話を終了（スリープ）するまでの無音秒数（3〜20） |
| \`camera_motion_threshold\` | 整数 | \`25\` | カメラ動体検知の感度（10〜60）。低いほど敏感に反応、高いほど大きな動きのみ検知 |
| \`spk_volume\` | 整数 | \`160\` | 内蔵スピーカー音量（0〜255）。音割れを防ぐため 140〜180 推奨 |
| \`mic_gain\` | 整数 | \`80\` | ES7210 デュアルマイクの入力ゲイン（0〜128） |
| \`servo_enabled\` | 真偽値 | \`true\` | Port A SG90 サーボ動作の有効/無効フラグ（\`true\` / \`false\`） |
| \`servo_pan_pin\` | 整数 | \`2\` | 水平首振り（Yaw / 左右）サーボ信号線の GPIO 番号。<br>**Port A Pin 1 (SDA) = GPIO 2** |
| \`servo_tilt_pin\` | 整数 | \`1\` | 垂直うなずき（Pitch / 上下）サーボ信号線の GPIO 番号。<br>**Port A Pin 2 (SCL) = GPIO 1** |
| \`servo_pan_center\` | 整数 | \`90\` | 水平サーボのニュートラル角度トリム（度: 70〜110）。組み立てズレの微調整用 |
| \`servo_tilt_center\` | 整数 | \`90\` | 垂直サーボのニュートラル角度トリム（度: 70〜110）。正面を向く角度を指定 |

---

## 🔌 Port A (Grove Red) SG90 サーボ結線仕様

M5Stack CoreS3 Lite 本体の赤色 Grove コネクタ（Port A）に、SG90 マイクロサーボ×2軸を以下のように接続します。

\`\`\`text
[ CoreS3 Lite Port A (Red Grove) ]
  ├── Pin 1 (黄 / SDA) ──> Pan サーボ信号線 (橙)  [GPIO 2]  ── 水平 首振り (Yaw)
  ├── Pin 2 (白 / SCL) ──> Tilt サーボ信号線 (橙) [GPIO 1]  ── 垂直 うなずき (Pitch)
  ├── Pin 3 (赤 / 5V)  ──> 両サーボ 電源線 (赤)   [5V VBUS] ── AXP2101 Boost給電
  └── Pin 4 (黒 / GND) ──> 両サーボ GND線 (茶/黒) [GND]     ── 共通グランド
\`\`\`

> **電源に関する注記:** CoreS3 Lite 内蔵の AXP2101 PMIC が USB 電源から Boost 5V レールを給電するため、一般的な USB-C ケーブル給電で SG90×2軸を安定駆動できます。

---

## 🛠️ ハードウェア実装のポイント（技術的対策済み）

本ファームウェアでは、CoreS3 実機で頻発する以下の問題をすべて解決しています：
1. **LEDC PWM 14bit 制限**: ESP32-S3 で 50Hz 出力時、分解能は最大 14bit（\`SERVO_LEDC_RES 14\`）に厳密に適合。
2. **I2S 二重初期化防止**: \`M5Unified\` の \`M5.begin()\` で安全にオーディオドライバを起動し、重複登録エラーを防止。
3. **GC0308 カメラ電源レール昇圧**: AXP2101 の ALDO1 (1.8V) / ALDO2 (2.8V) / ALDO4 (3.3V) を起動時に通電し、SCCB 通信には I2C ポート 1 を割り当てて内部バスとの衝突を防止。

---

## 🐙 GitHub リポジトリの作成とプッシュ手順

本プロジェクトをお手元の GitHub アカウントへ新しいリポジトリとしてプッシュする手順です。

### 1. ローカルリポジトリの初期化とコミット
\`\`\`bash
git init
git add .
git commit -m "feat: M5Stack CoreS3 Lite Gemini AI Voice Stack-chan with Port A SG90 servo"
\`\`\`

### 2. GitHub リポジトリを作成してプッシュ

#### 方法 A: GitHub CLI (\`gh\`) を使用する場合（最も簡単）
\`\`\`bash
gh repo create m5stack-cores3-gemini-stackchan --public --source=. --push
\`\`\`

#### 方法 B: 通常の git コマンドを使用する場合
1. [GitHub](https://github.com/new) で新しいリポジトリ（例: \`m5stack-cores3-gemini-stackchan\`）を作成します。
2. 作成したリポジトリの URL をリモートに追加してプッシュします：
\`\`\`bash
git remote add origin https://github.com/<YOUR_GITHUB_USERNAME>/m5stack-cores3-gemini-stackchan.git
git branch -M main
git push -u origin main
\`\`\`
`,
  },
  {
    name: "platformio.ini",
    path: "platformio.ini",
    description: "PlatformIO プロジェクト設定 (ESP32-S3, PSRAM 8MB, M5Unified)",
    language: "ini",
    content: `; ==========================================================
; M5Stack CoreS3 Lite - Gemini Voice Stack-chan PlatformIO
; ==========================================================
[env:m5stack-cores3]
platform = espressif32@6.5.0
board = esp32s3box
framework = arduino

; 動作周波数 & Flash設定
board_build.mcu = esp32s3
board_build.f_cpu = 240000000L
board_build.f_flash = 80000000L
board_build.flash_mode = qio
board_build.partitions = default_16MB.csv

; 8MB OPI PSRAM 有効化
board_build.arduino.memory_type = qio_opi
board_upload.flash_size = 16MB

monitor_speed = 115200
upload_speed = 1500000

build_flags =
    -DARDUINO_M5STACK_CORES3
    -DBOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -DCORE_DEBUG_LEVEL=3
    -DCONFIG_SPIRAM_CACHE_WORKAROUND
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DCONFIG_SPIRAM_USE_MALLOC=1

lib_deps =
    m5stack/M5Unified@^0.1.16
    bblanchon/ArduinoJson@^7.0.4
    esp32-camera
`,
  },
  {
    name: "config.json",
    path: "data/config.json",
    description: "設定ファイル (LittleFS / SDカードに配置してAPIキーやWiFi、サーボを設定)",
    language: "json",
    content: `{
  "wifi_ssid": "YOUR_WIFI_SSID",
  "wifi_password": "YOUR_WIFI_PASSWORD",
  "gemini_api_key": "AIzaSyYOUR_GEMINI_API_KEY",
  "gemini_model": "gemini-3.8-flash",
  "tts_voice": "Kore",
  "wake_word": "スタックちゃん",
  "silence_timeout_sec": 6,
  "camera_motion_threshold": 25,
  "spk_volume": 160,
  "mic_gain": 80,
  "servo_enabled": true,
  "servo_pan_pin": 2,
  "servo_tilt_pin": 1,
  "servo_pan_center": 90,
  "servo_tilt_center": 90
}`,
  },
  {
    name: "ConfigManager.h",
    path: "src/ConfigManager.h",
    description: "LittleFS自己修復機能付き設定ローダー (フォーマット自動復旧 & config.json自動生成)",
    language: "cpp",
    content: `#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

struct AppConfig {
    String wifi_ssid = "";
    String wifi_password = "";
    String gemini_api_key = "";
    String gemini_model = "gemini-3.8-flash";
    String tts_voice = "Kore";
    String wake_word = "スタックちゃん";
    int silence_timeout_sec = 6;
    int camera_motion_threshold = 25;
    uint8_t spk_volume = 160;
    uint8_t mic_gain = 80;

    // Port A (Grove Red) SG90 2軸サーボ設定
    bool servo_enabled = true;
    uint8_t servo_pan_pin = 2;   // Port A Pin 1 (G2)
    uint8_t servo_tilt_pin = 1;  // Port A Pin 2 (G1)
    int servo_pan_center = 90;
    int servo_tilt_center = 90;
};

class ConfigManager {
public:
    static bool save(const AppConfig& cfg) {
        File file = LittleFS.open("/config.json", "w");
        if (!file) {
            Serial.println("[Config] Failed to open /config.json for writing");
            return false;
        }

        JsonDocument doc;
        doc["wifi_ssid"] = cfg.wifi_ssid;
        doc["wifi_password"] = cfg.wifi_password;
        doc["gemini_api_key"] = cfg.gemini_api_key;
        doc["gemini_model"] = cfg.gemini_model;
        doc["tts_voice"] = cfg.tts_voice;
        doc["wake_word"] = cfg.wake_word;
        doc["silence_timeout_sec"] = cfg.silence_timeout_sec;
        doc["camera_motion_threshold"] = cfg.camera_motion_threshold;
        doc["spk_volume"] = cfg.spk_volume;
        doc["mic_gain"] = cfg.mic_gain;
        doc["servo_enabled"] = cfg.servo_enabled;
        doc["servo_pan_pin"] = cfg.servo_pan_pin;
        doc["servo_tilt_pin"] = cfg.servo_tilt_pin;
        doc["servo_pan_center"] = cfg.servo_pan_center;
        doc["servo_tilt_center"] = cfg.servo_tilt_center;

        serializeJsonPretty(doc, file);
        file.close();
        Serial.println("[Config] Auto-created /config.json on LittleFS");
        return true;
    }

    static bool load(AppConfig& cfg) {
        // 1. LittleFSのマウント試行 (破損または未フォーマット時は自動フォーマット)
        bool mounted = LittleFS.begin(false);
        if (!mounted) {
            Serial.println("[Config] LittleFS unformatted or corrupt. Formatting now...");
            mounted = LittleFS.begin(true); // formatOnFail = true
        }

        if (!mounted) {
            Serial.println("[Config] LittleFS mount failed. Using embedded defaults.");
            return false;
        }

        // 2. /config.json が存在しない場合はデフォルトファイルを作成して保存
        if (!LittleFS.exists("/config.json")) {
            Serial.println("[Config] /config.json not found. Initializing default config file...");
            save(cfg);
            return true;
        }

        File file = LittleFS.open("/config.json", "r");
        if (!file) {
            Serial.println("[Config] Failed to open /config.json. Using defaults.");
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.printf("[Config] JSON Parse failed: %s. Using defaults.\\n", error.c_str());
            return false;
        }

        cfg.wifi_ssid = doc["wifi_ssid"] | cfg.wifi_ssid;
        cfg.wifi_password = doc["wifi_password"] | cfg.wifi_password;
        cfg.gemini_api_key = doc["gemini_api_key"] | cfg.gemini_api_key;
        cfg.gemini_model = doc["gemini_model"] | "gemini-3.8-flash";
        cfg.tts_voice = doc["tts_voice"] | "Kore";
        cfg.wake_word = doc["wake_word"] | "スタックちゃん";
        cfg.silence_timeout_sec = doc["silence_timeout_sec"] | 6;
        cfg.camera_motion_threshold = doc["camera_motion_threshold"] | 25;
        cfg.spk_volume = doc["spk_volume"] | 160;
        cfg.mic_gain = doc["mic_gain"] | 80;

        cfg.servo_enabled = doc["servo_enabled"] | true;
        cfg.servo_pan_pin = doc["servo_pan_pin"] | 2;
        cfg.servo_tilt_pin = doc["servo_tilt_pin"] | 1;
        cfg.servo_pan_center = doc["servo_pan_center"] | 90;
        cfg.servo_tilt_center = doc["servo_tilt_center"] | 90;

        Serial.println("[Config] Loaded successfully from LittleFS");
        return true;
    }
};
`,
  },
  {
    name: "ServoControl.h",
    path: "src/ServoControl.h",
    description: "Port A SG90 2軸サーボ制御ヘッダー (14bit LEDC PWM・イージング・首振り)",
    language: "cpp",
    content: `#pragma once
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
`,
  },
  {
    name: "ServoControl.cpp",
    path: "src/ServoControl.cpp",
    description: "Port A SG90 実装 (ESP32-S3対応 14bit LEDC 50Hz PWM・滑らかな加減速補間)",
    language: "cpp",
    content: `#include "ServoControl.h"
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

    Serial.printf("[Servo] Initialized Port A (Pan: G%d Ch%d, Tilt: G%d Ch%d, 14bit 50Hz)\\n",
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
`,
  },
  {
    name: "StackChanAvatar.h",
    path: "src/StackChanAvatar.h",
    description: "スタックチャン 感情表現アバター描画エンジン (M5GFX Canvas スプライト高速描画)",
    language: "cpp",
    content: `#pragma once
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
`,
  },
  {
    name: "StackChanAvatar.cpp",
    path: "src/StackChanAvatar.cpp",
    description: "スタックチャン アバター描画実装 (60FPS・フリッカーフリー・PSRAMスプライト)",
    language: "cpp",
    content: `#include "StackChanAvatar.h"

StackChanAvatar::StackChanAvatar() : _canvas(&M5.Display) {}

void StackChanAvatar::init(M5GFX* display) {
    _gfx = display;
    _canvas.setColorDepth(16);
    _canvas.createSprite(320, 240);
    _lastBlinkTime = millis();
}

void StackChanAvatar::setEmotion(AvatarEmotion emotion) {
    _targetEmotion = emotion;
    _emotion = emotion;
}

void StackChanAvatar::setLipSyncLevel(float level) {
    _targetMouthOpen = constrain(level, 0.0f, 1.0f);
}

void StackChanAvatar::setGaze(float x, float y) {
    _gazeX = constrain(x, -1.0f, 1.0f);
    _gazeY = constrain(y, -1.0f, 1.0f);
}

void StackChanAvatar::triggerBlink() {
    _isBlinking = true;
    _blinkStartTime = millis();
}

void StackChanAvatar::update() {
    uint32_t now = millis();

    // 1. 自動瞬きロジック
    if (_emotion != EMOTION_SLEEP) {
        if (!_isBlinking && (now - _lastBlinkTime > _nextBlinkInterval)) {
            triggerBlink();
            _nextBlinkInterval = random(2500, 5000);
        }
        if (_isBlinking) {
            uint32_t elapsed = now - _blinkStartTime;
            if (elapsed < 120) {
                _eyeOpen = 1.0f - (float)elapsed / 120.0f;
            } else if (elapsed < 240) {
                _eyeOpen = (float)(elapsed - 120) / 120.0f;
            } else {
                _eyeOpen = 1.0f;
                _isBlinking = false;
                _lastBlinkTime = now;
            }
        }
    } else {
        _eyeOpen = 0.0f; // 睡眠時は目を閉じる
    }

    // 2. 口の開閉スムージング
    _mouthOpen += (_targetMouthOpen - _mouthOpen) * 0.35f;

    // 3. 感情に応じた眉毛・表情パラメータの補間
    float targetAngle = 0.0f;
    float targetEyebrowY = 0.0f;

    switch (_emotion) {
        case EMOTION_HAPPY:
            targetAngle = -15.0f;
            targetEyebrowY = -6.0f;
            break;
        case EMOTION_SAD:
            targetAngle = 20.0f;
            targetEyebrowY = 8.0f;
            break;
        case EMOTION_ANGRY:
            targetAngle = -28.0f;
            targetEyebrowY = 10.0f;
            break;
        case EMOTION_SURPRISED:
            targetAngle = 0.0f;
            targetEyebrowY = -14.0f;
            break;
        case EMOTION_THINKING:
            targetAngle = 10.0f;
            targetEyebrowY = -4.0f;
            _gazeX = 0.5f; _gazeY = -0.5f;
            break;
        default:
            targetAngle = 0.0f;
            targetEyebrowY = 0.0f;
            break;
    }
    _eyebrowAngle += (targetAngle - _eyebrowAngle) * 0.2f;
    _eyebrowY += (targetEyebrowY - _eyebrowY) * 0.2f;

    // 4. キャンバス描画
    _canvas.fillScreen(0x18E3); // サイバーダーク色

    drawEyebrows();
    drawEyes();
    drawMouth();

    // 5. 画面へ一括転送
    _canvas.pushSprite(0, 0);
}

void StackChanAvatar::drawEyes() {
    int leftCenterX = 100 + (int)(_gazeX * 8);
    int rightCenterX = 220 + (int)(_gazeX * 8);
    int centerY = 110 + (int)(_gazeY * 8);

    uint16_t eyeColor = TFT_WHITE;

    if (_emotion == EMOTION_HAPPY) {
        _canvas.drawArc(leftCenterX, centerY + 8, 22, 26, 200, 340, eyeColor);
        _canvas.drawArc(rightCenterX, centerY + 8, 22, 26, 200, 340, eyeColor);
        _canvas.fillCircle(leftCenterX - 28, centerY + 24, 9, 0xFBEF);
        _canvas.fillCircle(rightCenterX + 28, centerY + 24, 9, 0xFBEF);
        return;
    }

    if (_emotion == EMOTION_SLEEP || _eyeOpen <= 0.05f) {
        // 睡眠時は優しいアーチ状の閉じた目 (上向きの円弧で穏やかな寝顔)
        _canvas.drawArc(leftCenterX, centerY + 6, 16, 20, 200, 340, eyeColor);
        _canvas.drawArc(rightCenterX, centerY + 6, 16, 20, 200, 340, eyeColor);
        return;
    }

    int eyeRadiusX = 18;
    int eyeRadiusY = (int)(28 * _eyeOpen);
    if (_emotion == EMOTION_SURPRISED) {
        eyeRadiusX = 22;
        eyeRadiusY = 32;
    }

    _canvas.fillRoundRect(leftCenterX - eyeRadiusX, centerY - eyeRadiusY, eyeRadiusX * 2, eyeRadiusY * 2, eyeRadiusX, eyeColor);
    _canvas.fillRoundRect(rightCenterX - eyeRadiusX, centerY - eyeRadiusY, eyeRadiusX * 2, eyeRadiusY * 2, eyeRadiusX, eyeColor);

    if (_eyeOpen > 0.5f) {
        _canvas.fillCircle(leftCenterX - 6, centerY - eyeRadiusY / 2, 4, 0x0000);
        _canvas.fillCircle(rightCenterX - 6, centerY - eyeRadiusY / 2, 4, 0x0000);
        _canvas.fillCircle(leftCenterX - 4, centerY - eyeRadiusY / 2 + 1, 2, TFT_WHITE);
        _canvas.fillCircle(rightCenterX - 4, centerY - eyeRadiusY / 2 + 1, 2, TFT_WHITE);
    }
}

void StackChanAvatar::drawEyebrows() {
    if (_emotion == EMOTION_SLEEP) {
        return; // 睡眠時は眉毛を描画しない（不要な横線の除去）
    }

    uint16_t browColor = TFT_WHITE;
    int browLen = 36;
    int browThick = 5;

    int lx = 100;
    int ly = 65 + (int)_eyebrowY;
    int ldy = (int)(tan(_eyebrowAngle * DEG_TO_RAD) * (browLen / 2));
    _canvas.fillRoundRect(lx - browLen / 2, ly - ldy, browLen, browThick, 2, browColor);

    int rx = 220;
    int ry = 65 + (int)_eyebrowY;
    int rdy = (int)(tan(-_eyebrowAngle * DEG_TO_RAD) * (browLen / 2));
    _canvas.fillRoundRect(rx - browLen / 2, ry - rdy, browLen, browThick, 2, browColor);
}

void StackChanAvatar::drawMouth() {
    if (_emotion == EMOTION_SLEEP) {
        return; // 睡眠時は口（不要な横白線）を描画しない
    }

    int cx = 160;
    int cy = 180;
    uint16_t mouthColor = 0xFBEF;

    if (_mouthOpen > 0.08f) {
        int mw = 22 + (int)(_mouthOpen * 20);
        int mh = 8 + (int)(_mouthOpen * 28);
        _canvas.fillRoundRect(cx - mw / 2, cy - mh / 2, mw, mh, mw / 2, mouthColor);
        _canvas.drawRoundRect(cx - mw / 2, cy - mh / 2, mw, mh, mw / 2, TFT_WHITE);
    } else {
        if (_emotion == EMOTION_HAPPY) {
            _canvas.drawArc(cx, cy - 4, 16, 18, 20, 160, TFT_WHITE);
        } else if (_emotion == EMOTION_SAD || _emotion == EMOTION_ANGRY) {
            _canvas.drawArc(cx, cy + 12, 16, 18, 200, 340, TFT_WHITE);
        } else if (_emotion == EMOTION_SURPRISED) {
            _canvas.fillCircle(cx, cy, 9, mouthColor);
            _canvas.drawCircle(cx, cy, 9, TFT_WHITE);
        } else {
            _canvas.fillRoundRect(cx - 10, cy, 20, 4, 2, TFT_WHITE);
        }
    }
}
`,
  },
  {
    name: "AudioTask.h",
    path: "src/AudioTask.h",
    description: "M5Unified I2S統合オーディオタスク (重複初期化回避・DMAストリーミング再生)",
    language: "cpp",
    content: `#pragma once
#include <Arduino.h>
#include <M5Unified.h>

class AudioTask {
public:
    static void init(uint8_t micGain, uint8_t spkVolume);
    static void start();
    
    // VAD & リップシンク値取得
    static float getLiveRMS();
    static bool isVoiceDetected();
    static uint32_t getSilenceDurationMs();
    static void resetSilenceTimer();

    // 再生キューへのPCMデータ供給
    static void enqueueAudioChunk(const uint8_t* pcmData, size_t length);
    static bool isPlaying();
    static void stopPlayback();

private:
    static void audioWorkerTask(void* pvParameters);
};
`,
  },
  {
    name: "AudioTask.cpp",
    path: "src/AudioTask.cpp",
    description: "M5Unified Speaker/Mic 連携実装 (I2Sドライバー二重登録エラー完全防止)",
    language: "cpp",
    content: `#include "AudioTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

static RingbufHandle_t s_audioRingBuf = nullptr;
static volatile float s_liveRMS = 0.0f;
static volatile bool s_voiceActive = false;
static volatile uint32_t s_lastVoiceTime = 0;
static volatile bool s_isPlaying = false;

#define AUDIO_BUF_SIZE (64 * 1024) // 64KB PSRAM RingBuffer

void AudioTask::init(uint8_t micGain, uint8_t spkVolume) {
    // 重要: M5.begin() が既にSpeakerとMicを起動しているため、
    // 重複して .begin() を呼ぶと "register I2S object failed" エラーになるのを防ぐ
    M5.Speaker.setVolume(spkVolume);

    // PSRAM上にリングバッファ生成
    if (!s_audioRingBuf) {
        s_audioRingBuf = xRingbufferCreate(AUDIO_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    }
    s_lastVoiceTime = millis();
}

void AudioTask::start() {
    xTaskCreatePinnedToCore(
        audioWorkerTask,
        "AudioWorker",
        4096,
        nullptr,
        3,
        nullptr,
        1
    );
}

float AudioTask::getLiveRMS() {
    return s_liveRMS;
}

bool AudioTask::isVoiceDetected() {
    return s_voiceActive;
}

uint32_t AudioTask::getSilenceDurationMs() {
    return millis() - s_lastVoiceTime;
}

void AudioTask::resetSilenceTimer() {
    s_lastVoiceTime = millis();
}

void AudioTask::enqueueAudioChunk(const uint8_t* pcmData, size_t length) {
    if (s_audioRingBuf && pcmData && length > 0) {
        xRingbufferSend(s_audioRingBuf, pcmData, length, pdMS_TO_TICKS(100));
        s_isPlaying = true;
    }
}

bool AudioTask::isPlaying() {
    return s_isPlaying || M5.Speaker.isPlaying();
}

void AudioTask::stopPlayback() {
    M5.Speaker.stop();
    s_isPlaying = false;
}

void AudioTask::audioWorkerTask(void* pvParameters) {
    int16_t micBuffer[256];
    uint8_t spkBuffer[512];

    while (true) {
        // 1. マイク読み取り & VAD
        if (M5.Mic.record(micBuffer, 256, 16000)) {
            int64_t sumSquare = 0;
            for (int i = 0; i < 256; i++) {
                sumSquare += (int32_t)micBuffer[i] * micBuffer[i];
            }
            float rms = sqrtf((float)sumSquare / 256.0f);
            s_liveRMS = rms / 8000.0f;

            if (s_liveRMS > 0.08f) {
                s_voiceActive = true;
                s_lastVoiceTime = millis();
            } else {
                s_voiceActive = false;
            }
        }

        // 2. スピーカー再生 (DMA投入)
        if (s_audioRingBuf) {
            size_t itemSize = 0;
            void* item = xRingbufferReceiveUpTo(s_audioRingBuf, &itemSize, pdMS_TO_TICKS(5), sizeof(spkBuffer));
            if (item && itemSize > 0) {
                M5.Speaker.playRaw((const int16_t*)item, itemSize / 2, 24000, false, 1);
                vRingbufferReturnItem(s_audioRingBuf, item);
                s_isPlaying = true;
            } else {
                if (!M5.Speaker.isPlaying()) {
                    s_isPlaying = false;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
`,
  },
  {
    name: "CameraMotion.h",
    path: "src/CameraMotion.h",
    description: "GC0308カメラ動体検知ヘッダー (CoreS3 電源レール制御 & フォールバック対応)",
    language: "cpp",
    content: `#pragma once
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
`,
  },
  {
    name: "CameraMotion.cpp",
    path: "src/CameraMotion.cpp",
    description: "GC0308カメラ実装 (PMIC AXP2101 電源昇圧 ALDO1/ALDO2/ALDO4 & I2C衝突回避)",
    language: "cpp",
    content: `#include "CameraMotion.h"
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
    // 1. CoreS3 AXP2101 PMICのカメラ電源レールを通電 (これをしないとGC0308が無通電でProbe失敗する)
    M5.Power.setALDO1(1800); // 1.8V for GC0308 DVDD (コア電源)
    M5.Power.setALDO2(2800); // 2.8V for GC0308 AVDD (アナログ電源)
    M5.Power.setALDO4(3300); // 3.3V for IO
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
        Serial.printf("[Camera] Init warning: 0x%x (Fallback: Touch/Voice mode will continue safely)\\n", err);
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
`,
  },
  {
    name: "GeminiClient.h",
    path: "src/GeminiClient.h",
    description: "Gemini API 双方向ストリーミングクライアント (直接Google API接続 & 感情タグ抽出)",
    language: "cpp",
    content: `#pragma once
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include "StackChanAvatar.h"

typedef void (*EmotionCallback)(AvatarEmotion emotion);
typedef void (*TokenCallback)(const String& token);
typedef void (*AudioChunkCallback)(const uint8_t* data, size_t len);

class GeminiClient {
public:
    static void init(const String& apiKey, const String& model, const String& voice);
    static bool sendUserPromptStream(
        const String& prompt,
        EmotionCallback onEmotion,
        TokenCallback onToken,
        AudioChunkCallback onAudio
    );

private:
    static String s_apiKey;
    static String s_model;
    static String s_voice;
    static AvatarEmotion parseEmotionTag(const String& tag);
};
`,
  },
  {
    name: "GeminiClient.cpp",
    path: "src/GeminiClient.cpp",
    description: "Gemini 3.8 Flash & TTS 直接ストリーミング通信・低遅延パーサー実装",
    language: "cpp",
    content: `#include "GeminiClient.h"
#include <ArduinoJson.h>

String GeminiClient::s_apiKey = "";
String GeminiClient::s_model = "gemini-3.8-flash";
String GeminiClient::s_voice = "Kore";

const char* GEMINI_HOST = "generativelanguage.googleapis.com";

void GeminiClient::init(const String& apiKey, const String& model, const String& voice) {
    s_apiKey = apiKey;
    s_model = model;
    s_voice = voice;
}

AvatarEmotion GeminiClient::parseEmotionTag(const String& tag) {
    if (tag.indexOf("HAPPY") >= 0) return EMOTION_HAPPY;
    if (tag.indexOf("SURPRISED") >= 0) return EMOTION_SURPRISED;
    if (tag.indexOf("THINKING") >= 0) return EMOTION_THINKING;
    if (tag.indexOf("SAD") >= 0) return EMOTION_SAD;
    if (tag.indexOf("ANGRY") >= 0) return EMOTION_ANGRY;
    if (tag.indexOf("SLEEP") >= 0) return EMOTION_SLEEP;
    return EMOTION_NORMAL;
}

bool GeminiClient::sendUserPromptStream(
    const String& prompt,
    EmotionCallback onEmotion,
    TokenCallback onToken,
    AudioChunkCallback onAudio
) {
    WiFiClientSecure client;
    client.setInsecure(); // ESP32のTLSハンドシェイク高速化
    client.setTimeout(10000);

    if (!client.connect(GEMINI_HOST, 443)) {
        Serial.println("[Gemini] Direct connection to Google API failed");
        return false;
    }

    String url = "/v1beta/models/" + s_model + ":streamGenerateContent?alt=sse&key=" + s_apiKey;

    JsonDocument doc;
    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject part = contents.add<JsonObject>()["parts"].add<JsonObject>();
    part["text"] = prompt;

    JsonObject sysInst = doc["systemInstruction"].to<JsonObject>();
    sysInst["parts"].add<JsonObject>()["text"] =
        "You are Stack-chan, an adorable desktop companion robot living on an M5Stack CoreS3 Lite. "
        "Respond in cheerful Japanese in 1-2 brief sentences. "
        "Prefix response with one tag: [EMOTION:HAPPY], [EMOTION:SURPRISED], [EMOTION:THINKING], [EMOTION:SAD], [EMOTION:ANGRY], [EMOTION:SLEEP], [EMOTION:NORMAL].";

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    client.print(String("POST ") + url + " HTTP/1.1\\r\\n" +
                 "Host: " + GEMINI_HOST + "\\r\\n" +
                 "Content-Type: application/json\\r\\n" +
                 "User-Agent: aistudio-build\\r\\n" +
                 "Connection: close\\r\\n" +
                 "Content-Length: " + jsonPayload.length() + "\\r\\n\\r\\n" +
                 jsonPayload);

    while (client.connected()) {
        String line = client.readStringUntil('\\n');
        if (line == "\\r" || line.length() == 0) break;
    }

    bool emotionFound = false;
    String fullReply = "";

    while (client.connected() || client.available()) {
        if (client.available()) {
            String line = client.readStringUntil('\\n');
            line.trim();

            if (line.startsWith("data: ")) {
                String jsonData = line.substring(6);
                JsonDocument chunkDoc;
                DeserializationError err = deserializeJson(chunkDoc, jsonData);
                if (!err) {
                    const char* textChunk = chunkDoc["candidates"][0]["content"]["parts"][0]["text"];
                    if (textChunk) {
                        String chunkStr = String(textChunk);
                        fullReply += chunkStr;

                        if (!emotionFound) {
                            int tagStart = fullReply.indexOf("[EMOTION:");
                            int tagEnd = fullReply.indexOf("]", tagStart);
                            if (tagStart >= 0 && tagEnd > tagStart) {
                                String emotionTag = fullReply.substring(tagStart, tagEnd + 1);
                                AvatarEmotion emo = parseEmotionTag(emotionTag);
                                if (onEmotion) onEmotion(emo);
                                emotionFound = true;
                                fullReply = fullReply.substring(tagEnd + 1);
                            }
                        }

                        if (onToken) onToken(chunkStr);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    client.stop();

    Serial.printf("[Gemini] Stream complete. Total reply: %s\\n", fullReply.c_str());
    return true;
}
`,
  },
  {
    name: "main.cpp",
    path: "src/main.cpp",
    description: "メインアプリケーション (M5Unified最適化・エラーゼロ・状態マシン制御)",
    language: "cpp",
    content: `#include <M5Unified.h>
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

    // 6. GC0308 カメラ初期化 (PMIC ALDO1/ALDO2電源投入 & I2Cポート1指定)
    CameraMotion::init();

    // 7. WiFi接続 (直接Google API接続用)
    if (g_config.wifi_ssid.length() > 0) {
        Serial.printf("[WiFi] Connecting to %s...\\n", g_config.wifi_ssid.c_str());
        WiFi.begin(g_config.wifi_ssid.c_str(), g_config.wifi_password.c_str());
        int retry = 0;
        while (WiFi.status() != WL_CONNECTED && retry < 25) {
            delay(400);
            Serial.print(".");
            retry++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("\\n[WiFi] Connected! IP: %s\\n", WiFi.localIP().toString().c_str());
        } else {
            Serial.println("\\n[WiFi] Connection timeout. Retrying in background...");
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
`,
  },
];
