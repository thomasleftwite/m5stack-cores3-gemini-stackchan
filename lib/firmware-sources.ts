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

## ⚙️ WiFiセットアップ（WiFiManager）

初回起動時、または保存済みWiFiへ20秒以内に接続できない場合、本体は設定用AP \`StackChan-Setup\` を起動します。設定中は対話・カメラ・サーボの初期化を待ちます。

1. スマホのWiFi設定で \`StackChan-Setup\` に接続します（設定用APはパスワードなし）。インターネット接続がない旨の表示が出ても接続を維持してください。
2. 自動表示される設定画面、またはブラウザの **http://192.168.4.1** を開きます。
3. **Configure WiFi** から接続先の2.4GHz WiFiを選び、パスワードを入力して **Save** を押します。
4. 接続成功後、設定用APを終了し、通常の対話処理を開始します。スマホを通常のWiFiへ戻してください。

SSID・パスワードはESP32のNVSへ保存され、次回起動時は自動接続します。パスワードを間違えた場合は設定用APへ再接続してやり直します。設定画面は時間制限なく待機します。

### WiFiを再設定する

本体を再起動し、起動画面が出てから5秒以内にタッチ画面を3秒間押し続けてください。保存済みWiFiが利用可能でも設定用APを起動します。保存済み設定を起動ごとに消去する処理はありません。

### その他の設定（config.json）

\`data/config.json\` が手元にない場合は \`data/config.example.json\` をコピーして作成します（既存の設定は上書きしないでください）。

Gemini APIキー・音声・サーボ等は引き続き \`data/config.json\` で設定します。WiFi情報は含めません。旧 \`wifi_ssid\` / \`wifi_password\` 項目が残っていても利用されないため、手元の設定から取り除いてください。

実機用フォルダはリポジトリの \`firmware/\` です。以下はそのフォルダで実行します（Webから取得したZIPでは展開先ルート）。

\`\`\`powershell
pio run -e m5stack-cores3
# 以下は実機へ書き込む際に実行
pio run -e m5stack-cores3 -t uploadfs
pio run -e m5stack-cores3 -t upload
pio device monitor -b 115200
\`\`\`

\`uploadfs\` はGemini等の設定転送に使用します。WiFi設定だけなら不要です。LittleFSに設定ファイルがなければデフォルトを生成しますが、Gemini APIキーは別途設定が必要です。APIキー入りファイルや接続情報をコミット・共有しないでください。

### 対象・依存関係・検証

WiFi設定変更版: **2026-10-09 / WiFiManager移行 R01**。対象はM5Stack CoreS3 Lite、ESP32-S3 / Arduino、PlatformIO環境 \`m5stack-cores3\`。既存の \`esp32s3box\` ボード定義と \`espressif32@6.5.0\` を維持し、WiFiManagerを \`tzapu/WiFiManager@2.0.17\` に固定しています。その他の依存関係は \`platformio.ini\` を参照してください。

実機では、未設定起動 → スマホ設定 → 接続 → 再起動による自動接続、誤パスワードからの再入力、保存済みAP不在時の設定画面、画面長押しによる再設定を確認します。2026-10-09に実機への書込みとシリアルログによる設定AP起動を確認しました。保存済みWiFiへの再起動後の接続とTLS接続はR02で確認済みです。スマホ画面での設定操作・接続先変更・サーボ等の全体動作は未確認です。検証状態は \`WIFI_SETUP_VALIDATION.md\` を参照してください。

一次情報: [WiFiManager公式リポジトリ](https://github.com/tzapu/WiFiManager)

---

## 📋 \`config.json\` 各項目の設定リファレンス

\`\`\`json
{
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
| \`gemini_api_key\` | 文字列 | \`""\` | Google AI Studio で発行した Gemini API キー（\`AIzaSy...\`） |
| \`gemini_model\` | 文字列 | \`"gemini-3.8-flash"\` | 使用する対話モデル名。通常は \`"gemini-3.8-flash"\` を指定 |
| \`tts_voice\` | 文字列 | \`"Kore"\` | 音声モデル（\`gemini-3.8-flash-lite-tts\`）の声質。<br>選択肢: \`Kore\` (標準/落ち着いた声), \`Puck\` (明るい声), \`Fenrir\` (低音), \`Zephyr\` (透明感), \`Charon\` (深み) |
| \`wake_word\` | 文字列 | \`"スタックちゃん"\` | 対話を開始するキーワード |
| \`silence_timeout_sec\` | 整数 | \`6\` | 発話が途切れてから会話を終了（スリープ）するまでの無音秒数（3〜20） |
| \`camera_motion_threshold\` | 整数 | \`25\` | カメラ動体検知の感度（10〜60）。低いほど敏感に反応、高いほど大きな動きのみ検知 |
| \`spk_volume\` | 整数 | \`160\` | 内蔵スピーカー音量（0〜255）。音割れを防ぐため 140〜180 推奨 |
| \`mic_gain\` | 整数 | \`80\` | ES7210 デュアルマイクの入力ゲイン（0〜128） |
| \`servo_enabled\` | 真偽値 | \`true\` | Port A SG90 サーボ動作の有効/無効フラグ。R05ではビルド設定による停止が優先されます |
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
  ├── Pin 3 (赤 / 5V)  ──> 独立給電ではサーボ電源線へ接続しない
  │                       両サーボの赤線は定格に合う独立電源へ接続
  └── Pin 4 (黒 / GND) ──> 両サーボ GND線 (茶/黒) [GND]     ── 共通グランド
\`\`\`

> **電源に関する注記:** PCのUSB給電でSG90を動かした際、本体消灯・USB切断の報告がありました。一般的なUSB給電で2軸を安定駆動できるとは保証できません。R05はサーボ出力を停止しています。独立給電ではGNDを共通にし、外部電源のプラスとPort Aの5Vを直結しないでください。詳細は \`POWER_VALIDATION.md\` を参照してください。

---

## 🛠️ ハードウェア実装のポイント（技術的対策済み）

本ファームウェアでは、CoreS3 実機で頻発する以下の問題をすべて解決しています：
1. **LEDC PWM 14bit 制限**: ESP32-S3 で 50Hz 出力時、分解能は最大 14bit（\`SERVO_LEDC_RES 14\`）に厳密に適合。
2. **I2S 二重初期化防止**: \`M5Unified\` の \`M5.begin()\` で安全にオーディオドライバを起動し、重複登録エラーを防止。
3. **GC0308 カメラI2C共有**: M5Unifiedの既存内部I2Cバスを再利用し、ドライバの二重登録を防止。起動時に160x120グレースケールフレーム取得を確認（R04）。

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

## TLSメモリ不足修正（2026-10-09 / R02）

CoreS3 LiteのQuad SPI PSRAMに合わせ、\`board_build.arduino.memory_type = qio_qspi\` を使用します。旧OPI設定では実機のPSRAM容量が0になり、TLSが内部RAM不足で失敗しました。変更後は約8MBのPSRAM認識とTLS接続成功を実測しています。詳細と回帰確認手順は \`TLS_MEMORY_DIAGNOSTIC.md\` を参照してください。

## Gemini空応答修正（2026-10-09 / R03）

本体LittleFSのAPIキーが未設定でHTTP403が返っていました。設定転送後、HTTP200と非空テキスト受信を実機確認しました。通常のuploadは設定を更新しないため、Gemini設定を変更した場合はuploadfsが必要です（LittleFS全体を置換します）。HTTPエラー・空応答は成功扱いせず、ログはステータスと本文バイト数だけに限定します。再現手順・検証範囲は \`GEMINI_RESPONSE_VALIDATION.md\` を参照してください。音声認識・TTSの対話全体は未完成です。

## カメラ初期化修正（2026-10-09 / R04）

SCCBによる内部I2Cドライバの二重登録を修正しました。実機の160x120グレースケールフレーム取得とCamera=readyの起動状態を確認しました。カメラ利用不可時は起動ログにフォールバック状態を表示します。詳細と再現手順は \`CAMERA_VALIDATION.md\` を参照してください。動体による起床感度と長時間運用は未検証です。

## サーボ接続時の電源断対策（2026-10-09 / R05）

サーボ接続時の本体消灯・USB切断を調査中です。サーボ出力停止時にはHTTP200と応答受信を確認しました。暫定版では \`STACKCHAN_SERVO_OUTPUT_DISABLED=1\` が \`servo_enabled\` より優先され、サーボは動きません。給電と機構負荷の確認後に0へ変更して再ビルドしてください。サーボ有効状態での修正完了は未確認です。詳細は \`POWER_VALIDATION.md\` を参照してください。

## 外部給電申告後の再検証（2026-10-09 / R06）

サーボを再有効化してタッチ→Gemini送信を確認しましたが、本体消灯・USB切断が再発しました。現在はサーボ停止設定1へ戻しています。設計元TAKAO基板v1.2.1はGroveとサーボの5Vが共通であり、スイッチだけで給電分離できるとは限りません。実物の版数・配線・電源容量を確認するまで電源分離完了とは扱いません。詳細は \`POWER_VALIDATION.md\` のR06を参照してください。

## 外部電源単独での検証（2026-10-09 / R07）

現在はSTACKCHAN_SERVO_OUTPUT_DISABLED=0のサーボ再有効化版です。TAKAO v1.2はGroveとサーボの5Vが共通で、SW1は外部USB入力を切ります。動作時はPC USBを外し、通常Grove接続・SW1 ON・5V最大2.4Aの外部電源のみを使用します。書込み時は外部電源OFF、Groveを外してPC USBへ接続します。
利用者がアニメーション・サーボ正常動作、消灯なし、静止後のタップ復帰を2回確認しました。静止と復帰はコードの待機処理と整合します。長時間安定性・電源過渡測定・この構成でのHTTP応答結果は未検証です。詳細はfirmware/POWER_VALIDATION.md（firmware内ではPOWER_VALIDATION.md）を参照してください。
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

; 8MB Quad SPI PSRAM (CoreS3 Lite)
board_build.arduino.memory_type = qio_qspi
board_upload.flash_size = 16MB

monitor_speed = 115200
upload_speed = 1500000

; Servo enabled for external-supply-only validation (PC USB disconnected during use).
; Set 1 to restore servo output disable if power loss recurs.
build_flags =
    -DSTACKCHAN_SERVO_OUTPUT_DISABLED=0
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
    tzapu/WiFiManager@2.0.17
    esp32-camera

board_build.filesystem = littlefs`,
  },
  {
    name: "config.json",
    path: "data/config.json",
    description: "設定ファイル (LittleFS / SDカードに配置してAPIキーやWiFi、サーボを設定)",
    language: "json",
    content: `{
  "gemini_api_key": "",
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
`,
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
    String gemini_api_key = "";
    String gemini_model = "gemini-3.5-flash";
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

enum AudioMode {
    AUDIO_MODE_UNINIT = 0,
    AUDIO_MODE_MIC,
    AUDIO_MODE_PLAYBACK
};

class AudioTask {
public:
    static void init(uint8_t micGain, uint8_t spkVolume);
    static void start();
    
    // I2S排他モード制御 (CoreS3 ハーフデュプレックスハードウェア対応)
    static void requestMicMode();
    static void requestPlaybackMode();
    static AudioMode getCurrentMode();

    // VAD & リップシンク値取得
    static float getLiveRMS();
    static bool isVoiceDetected();
    static uint32_t getSilenceDurationMs();
    static void resetSilenceTimer();

    // 再生キューへのPCMデータ供給
    static void enqueueAudioChunk(const uint8_t* pcmData, size_t length);
    static bool isPlaying();
    static void stopPlayback();

    // 録音機能 (STT / Gemini Multimodal用)
    static void startRecording();
    static void stopRecording();
    static bool isRecording();
    static const int16_t* getRecordedPCM(size_t* outSamples);
    static size_t getRecordedBytes();
    static bool hasMeaningfulSpeech();

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
#include <esp_heap_caps.h>

static RingbufHandle_t s_audioRingBuf = nullptr;
static volatile float s_liveRMS = 0.0f;
static volatile bool s_voiceActive = false;
static volatile uint32_t s_lastVoiceTime = 0;
static volatile bool s_isPlaying = false;

static uint8_t s_spkVolume = 160;
static uint8_t s_micGain = 80;

// CoreS3 は ES7210 (Mic) と AW88298 (Speaker) が同じ I2S バスを共有するため、
// audioWorkerTask 内で直列に排他制御を行う
static volatile AudioMode s_requestedMode = AUDIO_MODE_MIC;
static AudioMode s_currentMode = AUDIO_MODE_UNINIT;

#define AUDIO_BUF_SIZE (96 * 1024) // 96KB PSRAM RingBuffer for 24kHz speaker playback
#define RECORD_MAX_SAMPLES (16000 * 4) // 4秒間 @ 16kHz mono = 64,000 samples = 128KB

static int16_t* s_recordBuffer = nullptr;
static volatile size_t s_recordSampleCount = 0;
static volatile bool s_isRecording = false;
static volatile size_t s_speechSampleCount = 0;

void AudioTask::init(uint8_t micGain, uint8_t spkVolume) {
    s_micGain = micGain;
    s_spkVolume = spkVolume;

    // PSRAM上にスピーカー再生リングバッファ生成
    if (!s_audioRingBuf) {
        s_audioRingBuf = xRingbufferCreate(AUDIO_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    }

    // PSRAM上にマイク録音バッファ (128KB) を確保
    if (!s_recordBuffer) {
        s_recordBuffer = (int16_t*)heap_caps_malloc(RECORD_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (!s_recordBuffer) {
            s_recordBuffer = (int16_t*)malloc(RECORD_MAX_SAMPLES * sizeof(int16_t));
        }
        if (s_recordBuffer) {
            memset(s_recordBuffer, 0, RECORD_MAX_SAMPLES * sizeof(int16_t));
        }
    }

    // M5.begin(cfg) で両方のデバイス(ES7210マイク/AW88298スピーカー)が給電・初期化されている。
    // CoreS3の単一I2Sバス競合を防ぐため、初期状態はスピーカーを一旦停止・終了し、
    // マイク待受モードを排他起動する。
    M5.Speaker.stop();
    M5.Speaker.end();
    vTaskDelay(pdMS_TO_TICKS(20));

    s_currentMode = AUDIO_MODE_MIC;
    s_requestedMode = AUDIO_MODE_MIC;
    s_lastVoiceTime = millis();
    Serial.println("[Audio] Audio subsystem initialized in MIC mode.");
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

void AudioTask::requestMicMode() {
    s_requestedMode = AUDIO_MODE_MIC;
}

void AudioTask::requestPlaybackMode() {
    s_requestedMode = AUDIO_MODE_PLAYBACK;
}

AudioMode AudioTask::getCurrentMode() {
    return s_currentMode;
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
        // スピーカーモードへの切替を要求
        if (s_requestedMode != AUDIO_MODE_PLAYBACK) {
            s_requestedMode = AUDIO_MODE_PLAYBACK;
        }
        s_isPlaying = true;
        xRingbufferSend(s_audioRingBuf, pcmData, length, pdMS_TO_TICKS(150));
    }
}

bool AudioTask::isPlaying() {
    return s_isPlaying || (s_currentMode == AUDIO_MODE_PLAYBACK && M5.Speaker.isPlaying());
}

void AudioTask::stopPlayback() {
    s_requestedMode = AUDIO_MODE_MIC;
    s_isPlaying = false;
    // リングバッファに残っている未再生PCMをフラッシュ
    if (s_audioRingBuf) {
        size_t dummySize = 0;
        void* dummy = nullptr;
        while ((dummy = xRingbufferReceiveUpTo(s_audioRingBuf, &dummySize, 0, 1024)) != nullptr) {
            vRingbufferReturnItem(s_audioRingBuf, dummy);
        }
    }
}

void AudioTask::startRecording() {
    s_recordSampleCount = 0;
    s_speechSampleCount = 0;
    s_isRecording = true;
    s_lastVoiceTime = millis();
    s_requestedMode = AUDIO_MODE_MIC;
    Serial.println("[Audio] Recording started for speech input (max 4.0s @ 16kHz)...");
}

void AudioTask::stopRecording() {
    s_isRecording = false;
    Serial.printf("[Audio] Recording stopped. Samples=%u (%u ms), speechSamples=%u (%u ms)\n",
                  static_cast<unsigned>(s_recordSampleCount),
                  static_cast<unsigned>(s_recordSampleCount * 1000 / 16000),
                  static_cast<unsigned>(s_speechSampleCount),
                  static_cast<unsigned>(s_speechSampleCount * 1000 / 16000));
}

bool AudioTask::isRecording() {
    return s_isRecording;
}

const int16_t* AudioTask::getRecordedPCM(size_t* outSamples) {
    if (outSamples) *outSamples = s_recordSampleCount;
    return s_recordBuffer;
}

size_t AudioTask::getRecordedBytes() {
    return s_recordSampleCount * sizeof(int16_t);
}

bool AudioTask::hasMeaningfulSpeech() {
    // 250ms以上の実発話 (音量閾値超え) が蓄積されていれば有意な発話とみなす (16000 * 25 / 100 = 4000 samples)
    return s_speechSampleCount >= 4000;
}

void AudioTask::audioWorkerTask(void* pvParameters) {
    int16_t micBuffer[256];

    // 8面ローテーションバッファ (各512サンプル = 1024バイト = 約21.3ms @ 24kHz)
    // M5.Speaker.playRaw はポインタをDMAキューに保持するため、
    // 8面 (~170ms) のローテーションバッファでDMA転送完了前のデータ上書きを確実に防止
    static int16_t spkBuffers[8][512];
    static size_t spkBufIdx = 0;
    static uint32_t lastChunkMillis = 0;

    while (true) {
        // 1. I2Sバス排他モード切替 (すべてのI2S呼出をこのワーカタスク内で直列化)
        if (s_requestedMode != s_currentMode) {
            if (s_requestedMode == AUDIO_MODE_PLAYBACK) {
                Serial.println("[Audio] Switching I2S: MIC (ES7210) -> SPEAKER (AW88298)...");
                if (s_currentMode == AUDIO_MODE_MIC) {
                    M5.Mic.end();
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                M5.Speaker.begin();
                M5.Speaker.setVolume(s_spkVolume);
                M5.Speaker.setChannelVolume(0, s_spkVolume);
                s_currentMode = AUDIO_MODE_PLAYBACK;
                lastChunkMillis = millis();
                Serial.println("[Audio] I2S switched to SPEAKER mode successfully.");
            } else if (s_requestedMode == AUDIO_MODE_MIC) {
                Serial.println("[Audio] Switching I2S: SPEAKER (AW88298) -> MIC (ES7210)...");
                if (s_currentMode == AUDIO_MODE_PLAYBACK) {
                    M5.Speaker.stop();
                    while (M5.Speaker.isPlaying()) {
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                    M5.Speaker.end();
                    vTaskDelay(pdMS_TO_TICKS(20));
                }
                M5.Mic.begin();
                s_currentMode = AUDIO_MODE_MIC;
                s_isPlaying = false;
                s_liveRMS = 0.0f;
                s_voiceActive = false;
                s_lastVoiceTime = millis();
                Serial.println("[Audio] I2S switched to MIC mode successfully.");
            }
        }

        // 2. モード別の実行処理
        if (s_currentMode == AUDIO_MODE_MIC) {
            if (M5.Mic.record(micBuffer, 256, 16000)) {
                int64_t sumSquare = 0;
                for (int i = 0; i < 256; i++) {
                    sumSquare += (int32_t)micBuffer[i] * micBuffer[i];
                }
                float rms = sqrtf((float)sumSquare / 256.0f);
                s_liveRMS = rms / 8000.0f;

                // 人間の通常発話 (30-50cm) に適したVAD閾値 (RMS ~360)
                if (s_liveRMS > 0.045f) {
                    s_voiceActive = true;
                    s_lastVoiceTime = millis();
                    if (s_isRecording) {
                        s_speechSampleCount += 256;
                    }
                } else {
                    s_voiceActive = false;
                }

                if (s_isRecording && s_recordBuffer) {
                    if (s_recordSampleCount + 256 <= RECORD_MAX_SAMPLES) {
                        memcpy(&s_recordBuffer[s_recordSampleCount], micBuffer, 256 * sizeof(int16_t));
                        s_recordSampleCount += 256;
                    } else {
                        s_isRecording = false;
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        else if (s_currentMode == AUDIO_MODE_PLAYBACK) {
            // M5Unified Speaker Channel 0 の空き状態確認
            // (0: 停止中, 1: 再生中で空きキューあり, 2: キュー満杯)
            size_t playingState = M5.Speaker.isPlaying(0);
            bool chunkFed = false;

            if (playingState < 2 && s_audioRingBuf) {
                size_t itemSize = 0;
                void* item = xRingbufferReceiveUpTo(s_audioRingBuf, &itemSize, pdMS_TO_TICKS(5), sizeof(spkBuffers[0]));
                if (item && itemSize > 0) {
                    size_t samples = itemSize / sizeof(int16_t);
                    if (samples > 512) samples = 512;

                    memcpy(spkBuffers[spkBufIdx], item, samples * sizeof(int16_t));
                    vRingbufferReturnItem(s_audioRingBuf, item);

                    // リップシンク用RMS計算 (TTS音声に合わせてアバターの口を同期)
                    int64_t sumSquare = 0;
                    for (size_t i = 0; i < samples; i++) {
                        sumSquare += (int32_t)spkBuffers[spkBufIdx][i] * spkBuffers[spkBufIdx][i];
                    }
                    float rms = sqrtf((float)sumSquare / (float)samples);
                    s_liveRMS = rms / 6000.0f;

                    // Channel 0 で24kHz mono 16bit PCM再生
                    M5.Speaker.playRaw(spkBuffers[spkBufIdx], samples, 24000, false, 1, 0);
                    spkBufIdx = (spkBufIdx + 1) % 8;

                    s_isPlaying = true;
                    lastChunkMillis = millis();
                    chunkFed = true;
                }
            }

            // リングバッファが空で、かつスピーカー再生キューも空になった場合
            if (!chunkFed && playingState == 0) {
                // 最後のPCMチャンク投入から250ms以上経過していたら再生完了
                if (s_isPlaying && (millis() - lastChunkMillis > 250)) {
                    s_isPlaying = false;
                    s_liveRMS = 0.0f;
                    Serial.println("[Audio] Playback completed. Returning to MIC mode.");
                    s_requestedMode = AUDIO_MODE_MIC;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
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
    // M5.begin() initializes the CoreS3 PMIC; ensure the camera's ALDO3 rail is on.
    M5.Power.Axp2101.setALDO3(3300);
    delay(100);

    if (s_initialized) return true;
    camera_config_t config{};
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
    // A negative SDA selects the existing bus instead of installing a second driver.
    config.pin_sccb_sda = -1;
    config.pin_sccb_scl = -1;
    config.sccb_i2c_port = M5.In_I2C.getPort();
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
        esp_camera_deinit();
        s_initialized = false;
        return false;
    }

    camera_fb_t* first = esp_camera_fb_get();
    if (!first || first->width != IMG_W || first->height != IMG_H ||
        first->format != PIXFORMAT_GRAYSCALE || first->len < IMG_W * IMG_H) {
        if (first) esp_camera_fb_return(first);
        free(s_prevFrame);
        s_prevFrame = nullptr;
        esp_camera_deinit();
        Serial.println("[Camera] Failed to acquire a valid grayscale frame; using Touch/Voice mode");
        return false;
    }
    memcpy(s_prevFrame, first->buf, IMG_W * IMG_H);
    Serial.printf("[Camera] Frame verified: %ux%u grayscale bytes=%u\\n",
                  static_cast<unsigned>(first->width), static_cast<unsigned>(first->height),
                  static_cast<unsigned>(first->len));
    esp_camera_fb_return(first);

    s_initialized = true;
    Serial.println("[Camera] GC0308 Initialized successfully with QQVGA Motion Detector");
    return true;
}

bool CameraMotion::checkMotion(int threshold) {
    if (!s_initialized || !s_prevFrame) return false;

    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) return false;
    if (fb->width != IMG_W || fb->height != IMG_H ||
        fb->format != PIXFORMAT_GRAYSCALE || fb->len < IMG_W * IMG_H) {
        esp_camera_fb_return(fb);
        return false;
    }

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
    
    // テキスト入力によるストリーミング対話 + 自動TTS音声合成
    static bool sendUserPromptStream(
        const String& prompt,
        EmotionCallback onEmotion,
        TokenCallback onToken,
        AudioChunkCallback onAudio
    );

    // 実機マイク録音PCM (16kHz mono) によるマルチモーダル音声対話 (STT + LLM + TTS)
    static bool sendUserAudioDialogue(
        const int16_t* pcmSamples,
        size_t sampleCount,
        EmotionCallback onEmotion,
        TokenCallback onToken,
        AudioChunkCallback onAudio
    );

    // テキストから24kHz PCM音声を合成して再生ストリームへ供給
    static bool generateTTS(
        const String& text,
        AudioChunkCallback onAudio
    );

private:
    static String s_apiKey;
    static String s_model;
    static String s_voice;
    static AvatarEmotion parseEmotionTag(const String& tag);
    static String extractCleanText(const String& rawText);
};
`,
  },
  {
    name: "GeminiClient.cpp",
    path: "src/GeminiClient.cpp",
    description: "Gemini 3.8 Flash & TTS 直接ストリーミング通信・低遅延パーサー実装",
    language: "cpp",
    content: `#include "GeminiClient.h"
#include "AudioTask.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <functional>

namespace {
// Base64 table
static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void encodeBase64(const uint8_t* in, size_t inLen, char* out) {
    size_t i = 0, j = 0;
    while (i < inLen) {
        uint32_t octet_a = i < inLen ? in[i++] : 0;
        uint32_t octet_b = i < inLen ? in[i++] : 0;
        uint32_t octet_c = i < inLen ? in[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = B64_CHARS[(triple >> 18) & 0x3F];
        out[j++] = B64_CHARS[(triple >> 12) & 0x3F];
        out[j++] = (i > inLen + 1) ? '=' : B64_CHARS[(triple >> 6) & 0x3F];
        out[j++] = (i > inLen) ? '=' : B64_CHARS[triple & 0x3F];
    }
    out[j] = '\\0';
}

void buildWavHeader(uint8_t* h, uint32_t pcmLen, uint32_t sampleRate = 16000) {
    uint32_t totalLen = pcmLen + 36;
    uint32_t byteRate = sampleRate * 1 * 2;
    memcpy(h, "RIFF", 4);
    h[4] = (uint8_t)(totalLen & 0xff); h[5] = (uint8_t)((totalLen >> 8) & 0xff);
    h[6] = (uint8_t)((totalLen >> 16) & 0xff); h[7] = (uint8_t)((totalLen >> 24) & 0xff);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0; // subchunk1 size
    h[20] = 1; h[21] = 0; // PCM format
    h[22] = 1; h[23] = 0; // mono
    h[24] = (uint8_t)(sampleRate & 0xff); h[25] = (uint8_t)((sampleRate >> 8) & 0xff);
    h[26] = (uint8_t)((sampleRate >> 16) & 0xff); h[27] = (uint8_t)((sampleRate >> 24) & 0xff);
    h[28] = (uint8_t)(byteRate & 0xff); h[29] = (uint8_t)((byteRate >> 8) & 0xff);
    h[30] = (uint8_t)((byteRate >> 16) & 0xff); h[31] = (uint8_t)((byteRate >> 24) & 0xff);
    h[32] = 2; h[33] = 0; // block align
    h[34] = 16; h[35] = 0; // bits per sample
    memcpy(h + 36, "data", 4);
    h[40] = (uint8_t)(pcmLen & 0xff); h[41] = (uint8_t)((pcmLen >> 8) & 0xff);
    h[42] = (uint8_t)((pcmLen >> 16) & 0xff); h[43] = (uint8_t)((pcmLen >> 24) & 0xff);
}

// Streaming Base64 Decoder: 4文字 -> 3バイト変換し、44バイトのWAVヘッダーをスキップしてPCMを供給
class Base64StreamDecoder {
public:
    typedef std::function<void(const uint8_t* pcm, size_t len)> ChunkCallback;

    explicit Base64StreamDecoder(ChunkCallback cb)
        : cb_(cb), bufIdx_(0) {}

    void write(char c) {
        int val = decodeChar(c);
        if (val >= 0) {
            buf_[bufIdx_++] = (uint8_t)val;
            if (bufIdx_ == 4) {
                flushBlock(3);
                bufIdx_ = 0;
            }
        } else if (c == '=') {
            // パディング処理
            if (bufIdx_ == 3) {
                flushBlock(2);
                bufIdx_ = 0;
            } else if (bufIdx_ == 2) {
                flushBlock(1);
                bufIdx_ = 0;
            }
        }
    }

    void finish() {
        if (bufIdx_ >= 2) {
            flushBlock(bufIdx_ - 1);
            bufIdx_ = 0;
        }
    }

private:
    ChunkCallback cb_;
    uint8_t buf_[4];
    uint8_t bufIdx_;

    static int decodeChar(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    void flushBlock(size_t outBytes) {
        uint8_t raw[3];
        raw[0] = (buf_[0] << 2) | ((buf_[1] >> 4) & 0x03);
        if (outBytes >= 2) raw[1] = ((buf_[1] & 0x0f) << 4) | ((buf_[2] >> 2) & 0x0f);
        if (outBytes >= 3) raw[2] = ((buf_[2] & 0x03) << 6) | (buf_[3] & 0x3f);

        if (cb_) {
            cb_(raw, outBytes);
        }
    }
};

// RIFF WAVヘッダーを動的に解析し、厳密にPCMサンプル境界 (16-bit aligned) から音声を取り出すストリッパー
class RiffWavStripper {
public:
    using OutputCallback = std::function<void(const uint8_t*, size_t)>;
    explicit RiffWavStripper(OutputCallback cb)
        : cb_(cb), state_(STATE_INIT), byteIndex_(0), dataMatchIdx_(0), skipBytesLeft_(0) {}

    void processBytes(const uint8_t* data, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            processByte(data[i]);
        }
    }

private:
    enum State {
        STATE_INIT,       // 'R','I','F','F' を確認
        STATE_SCAN_DATA,  // "data" チャンク識別子を検索
        STATE_SKIP_SIZE,  // 4バイトのdataチャンク長をスキップ
        STATE_PCM         // 厳密にアラインされた生PCMストリーム
    };

    OutputCallback cb_;
    State state_;
    size_t byteIndex_;
    uint8_t dataMatchIdx_;
    uint8_t skipBytesLeft_;
    uint8_t headerLead_[4];

    void processByte(uint8_t b) {
        if (state_ == STATE_PCM) {
            if (cb_) cb_(&b, 1);
            return;
        }

        if (state_ == STATE_INIT) {
            headerLead_[byteIndex_++] = b;
            if (byteIndex_ == 4) {
                if (headerLead_[0] == 'R' && headerLead_[1] == 'I' &&
                    headerLead_[2] == 'F' && headerLead_[3] == 'F') {
                    state_ = STATE_SCAN_DATA;
                    dataMatchIdx_ = 0;
                } else {
                    // RIFFコンテナではない場合は全データを直接生PCMとして出力
                    state_ = STATE_PCM;
                    if (cb_) cb_(headerLead_, 4);
                }
            }
            return;
        }

        if (state_ == STATE_SCAN_DATA) {
            byteIndex_++;
            static const uint8_t DATA_TAG[4] = {'d', 'a', 't', 'a'};
            if (b == DATA_TAG[dataMatchIdx_]) {
                dataMatchIdx_++;
                if (dataMatchIdx_ == 4) {
                    state_ = STATE_SKIP_SIZE;
                    skipBytesLeft_ = 4;
                }
            } else {
                dataMatchIdx_ = (b == DATA_TAG[0]) ? 1 : 0;
            }
            if (byteIndex_ > 256 && state_ == STATE_SCAN_DATA) {
                state_ = STATE_PCM;
            }
            return;
        }

        if (state_ == STATE_SKIP_SIZE) {
            skipBytesLeft_--;
            if (skipBytesLeft_ == 0) {
                state_ = STATE_PCM;
            }
            return;
        }
    }
};

// TTSストリーミングレスポンス処理用Sink
class TTSResponseSink : public Stream {
public:
    explicit TTSResponseSink(AudioChunkCallback onAudio)
        : onAudio_(onAudio), inBase64Data_(false), state_(0), matchStage_(0), totalPcmBytes_(0),
          wavStripper_([this](const uint8_t* pcm, size_t len) {
              for (size_t i = 0; i < len; ++i) {
                  pcmBuffer_[pcmBufIdx_++] = pcm[i];
                  totalPcmBytes_++;
                  // 1024バイト (512サンプル @ 24kHz = ~21.3ms) 単位でコールバック
                  if (pcmBufIdx_ >= sizeof(pcmBuffer_)) {
                      if (onAudio_) onAudio_(pcmBuffer_, pcmBufIdx_);
                      pcmBufIdx_ = 0;
                  }
              }
          }),
          decoder_([this](const uint8_t* rawData, size_t rawLen) {
              wavStripper_.processBytes(rawData, rawLen);
          }) {}

    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        for (size_t i = 0; i < size; ++i) {
            char c = static_cast<char>(data[i]);
            if (!inBase64Data_) {
                // "data" キーを検索
                static const char KEY[] = "\\"data\\"";
                if (matchStage_ == 0) {
                    if (c == KEY[state_]) {
                        state_++;
                        if (state_ == strlen(KEY)) {
                            matchStage_ = 1; // "data" 一致、次は ':' を待つ
                            state_ = 0;
                        }
                    } else {
                        state_ = (c == KEY[0]) ? 1 : 0;
                    }
                } else if (matchStage_ == 1) {
                    if (c == ':') {
                        matchStage_ = 2; // ':' 検出、次は '"' を待つ
                    } else if (c != ' ' && c != '\\t' && c != '\\r' && c != '\\n') {
                        matchStage_ = 0;
                        state_ = 0;
                    }
                } else if (matchStage_ == 2) {
                    if (c == '"') {
                        inBase64Data_ = true; // Base64音声データ開始
                        matchStage_ = 0;
                        state_ = 0;
                    } else if (c != ' ' && c != '\\t' && c != '\\r' && c != '\\n') {
                        matchStage_ = 0;
                        state_ = 0;
                    }
                }
            } else {
                if (c == '"') {
                    // Base64データ終端
                    inBase64Data_ = false;
                    decoder_.finish();
                    // 残りのサンプルを2バイト偶数アラインで送信
                    size_t evenBytes = pcmBufIdx_ & ~1;
                    if (evenBytes > 0 && onAudio_) {
                        onAudio_(pcmBuffer_, evenBytes);
                        pcmBufIdx_ = 0;
                    }
                } else if (c != '\\r' && c != '\\n' && c != ' ') {
                    decoder_.write(c);
                }
            }
        }
        return size;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t getPcmBytes() const { return totalPcmBytes_; }

private:
    AudioChunkCallback onAudio_;
    bool inBase64Data_;
    size_t state_;
    uint8_t matchStage_ = 0;
    size_t totalPcmBytes_;
    uint8_t pcmBuffer_[1024];
    size_t pcmBufIdx_ = 0;
    RiffWavStripper wavStripper_;
    Base64StreamDecoder decoder_;
};

// HTTPClient removes HTTP chunk framing before writing decoded bytes here.
class ResponseSink : public Stream {
public:
    explicit ResponseSink(std::function<void(const String&)> onEvent = nullptr)
        : onEvent_(onEvent) {}
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        for (size_t i = 0; i < size; ++i) {
            if (overflow_) return i;
            const char ch = static_cast<char>(data[i]);
            if (!onEvent_) {
                if (body_.length() >= 32768) { overflow_ = true; return i; }
                body_ += ch;
            } else if (ch == '\\n') {
                consumeLine();
            } else if (ch != '\\r') {
                if (line_.length() >= 65536) { overflow_ = true; return i; }
                line_ += ch;
            }
        }
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    bool complete() const { return !overflow_; }
    const String& body() const { return body_; }
private:
    void consumeLine() {
        if (line_.isEmpty()) {
            if (!event_.isEmpty()) { onEvent_(event_); event_ = ""; }
        } else if (line_.startsWith("data:")) {
            String data = line_.substring(5);
            if (data.startsWith(" ")) data.remove(0, 1);
            if (event_.length() + data.length() + 1 > 65536) { overflow_ = true; }
            else { if (!event_.isEmpty()) event_ += '\\n'; event_ += data; }
        }
        line_ = "";
    }
    std::function<void(const String&)> onEvent_;
    String line_, event_, body_;
    bool overflow_ = false;
};

// Only fixed categories and uppercase API enums may reach serial output.
String safeApiEnum(const char* value) {
    if (!value || !*value || strlen(value) > 80) return "UNKNOWN";
    for (const char* p = value; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) return "UNKNOWN";
    }
    return String(value);
}
void reportApiError(const String& body) {
    JsonDocument errorDoc;
    if (deserializeJson(errorDoc, body)) {
        Serial.println("[Gemini] API error category=UNREADABLE_ERROR_BODY");
        return;
    }
    String status = safeApiEnum(errorDoc["error"]["status"] | "UNKNOWN");
    String reason = "UNKNOWN";
    for (JsonObject detail : errorDoc["error"]["details"].as<JsonArray>()) {
        if (detail["reason"].is<const char*>()) reason = safeApiEnum(detail["reason"]);
    }
    String message = errorDoc["error"]["message"] | "";
    String category = "UNKNOWN";
    String lower = message;
    lower.toLowerCase();
    if (lower.indexOf("overloaded") >= 0 || lower.indexOf("high demand") >= 0) category = "MODEL_OVERLOADED";
    else if (lower.indexOf("quota") >= 0 || lower.indexOf("rate-limit") >= 0) category = "QUOTA_EXCEEDED";
    else if (lower.indexOf("unavailable") >= 0) category = "SERVICE_UNAVAILABLE";
    else if (message.indexOf("reported as leaked") >= 0) category = "API_KEY_LEAKED";
    else if (message.indexOf("unrestricted") >= 0) category = "API_KEY_UNRESTRICTED";
    else if (message.indexOf("denied access") >= 0) category = "PROJECT_ACCESS_DENIED";
    else if (message.indexOf("referer") >= 0 || message.indexOf("referrer") >= 0) category = "REFERRER_RESTRICTION";
    else if (message.indexOf("API key not valid") >= 0) category = "API_KEY_INVALID";
    else if (message.indexOf("unregistered callers") >= 0) category = "API_KEY_MISSING_OR_INVALID";
    else if (message.indexOf("disabled") >= 0) category = "SERVICE_OR_KEY_DISABLED";
    Serial.printf("[Gemini] API error status=%s reason=%s category=%s\\n", status.c_str(), reason.c_str(), category.c_str());
}
}

String GeminiClient::s_apiKey = "";
String GeminiClient::s_model = "gemini-2.5-flash";
String GeminiClient::s_voice = "Kore";
String GeminiClient::s_currentDateTime = "";

const char* GEMINI_HOST = "generativelanguage.googleapis.com";

void GeminiClient::init(const String& apiKey, const String& model, const String& voice) {
    s_apiKey = apiKey;
    s_model = (model.isEmpty() || model == "gemini-3.5-flash" || model == "gemini-3.8-flash" || model == "gemini-3.1-flash-lite") ? "gemini-2.5-flash" : model;
    s_voice = voice.isEmpty() ? "Kore" : voice;
}

void GeminiClient::setCurrentDateTime(const String& dateTimeStr) {
    s_currentDateTime = dateTimeStr;
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

String GeminiClient::extractHeardText(const String& rawText) {
    int start = rawText.indexOf("[HEARD:");
    if (start >= 0) {
        int end = rawText.indexOf("]", start);
        if (end > start) {
            String heard = rawText.substring(start + 7, end);
            heard.trim();
            return heard;
        }
    }
    return "";
}

String GeminiClient::extractCleanText(const String& rawText) {
    String clean = rawText;
    int heardStart = clean.indexOf("[HEARD:");
    if (heardStart >= 0) {
        int heardEnd = clean.indexOf("]", heardStart);
        if (heardEnd > heardStart) {
            clean.remove(heardStart, (heardEnd - heardStart) + 1);
        }
    }
    int tagStart = clean.indexOf("[EMOTION:");
    if (tagStart >= 0) {
        int tagEnd = clean.indexOf("]", tagStart);
        if (tagEnd > tagStart) {
            clean.remove(tagStart, (tagEnd - tagStart) + 1);
        }
    }
    clean.trim();
    return clean;
}

// 1. テキスト対話ストリーミング + TTS音声合成
bool GeminiClient::sendUserPromptStream(
    const String& prompt,
    EmotionCallback onEmotion,
    TokenCallback onToken,
    AudioChunkCallback onAudio
) {
    if (s_apiKey.isEmpty()) {
        Serial.println("[Gemini] Stream failed. reason=API_KEY_MISSING");
        return false;
    }

    String modelToUse = s_model;
    if (modelToUse.isEmpty() || modelToUse == "gemini-3.5-flash" || modelToUse == "gemini-3.8-flash" || modelToUse == "gemini-3.1-flash-lite") {
        modelToUse = "gemini-2.5-flash";
    }

    WiFiClientSecure client;
    client.setInsecure(); // ESP32のTLSハンドシェイク高速化
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(30000);
    http.setReuse(false);

    String url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":streamGenerateContent?alt=sse";
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", s_apiKey);
    const char* responseHeaders[] = {"Content-Type"};
    http.collectHeaders(responseHeaders, 1);

    JsonDocument doc;
    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject part = contents.add<JsonObject>()["parts"].add<JsonObject>();
    part["text"] = prompt;

    // Google Search Groundingツール (天気・最新ニュースのリアルタイム検索)
    JsonArray tools = doc["tools"].to<JsonArray>();
    tools.add<JsonObject>()["googleSearch"].to<JsonObject>();

    String sysText =
        "You are Stack-chan, an adorable desktop companion robot living on an M5Stack CoreS3 Lite. "
        "Answer questions accurately, helpfully, and cheerfully in 1-2 brief Japanese sentences. ";
    if (!s_currentDateTime.isEmpty()) {
        sysText += "Current local time: " + s_currentDateTime + " (Japan Standard Time, JST). ";
    }
    sysText +=
        "Prefix response format: [HEARD: <recognized prompt>][EMOTION: <EMOTION_TAG>] <reply>. "
        "Emotion tags: [EMOTION:HAPPY], [EMOTION:SURPRISED], [EMOTION:THINKING], [EMOTION:SAD], [EMOTION:ANGRY], [EMOTION:NORMAL]. "
        "For current weather, latest news, or current events, use Google Search results to provide accurate facts.";

    JsonObject sysInst = doc["systemInstruction"].to<JsonObject>();
    sysInst["parts"].add<JsonObject>()["text"] = sysText;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int status = http.POST(jsonPayload);
    Serial.printf("[Gemini] HTTP status=%d (model=%s)\\n", status, modelToUse.c_str());

    // 429または503の場合はgemini-flash-latestへ自動リトライ、さらに必要ならgemini-3.1-flash-lite
    if ((status == 429 || status == 503) && modelToUse != "gemini-3.1-flash-lite") {
        http.end();
        Serial.println("[Gemini] Retrying with model: gemini-3.1-flash-lite...");
        modelToUse = "gemini-3.1-flash-lite";
        url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":streamGenerateContent?alt=sse";
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("x-goog-api-key", s_apiKey);
            http.collectHeaders(responseHeaders, 1);
            status = http.POST(jsonPayload);
            Serial.printf("[Gemini] Retry HTTP status=%d (model=%s)\\n", status, modelToUse.c_str());
        }
    }
    if ((status == 429 || status == 503) && modelToUse != "gemini-flash-latest") {
        http.end();
        Serial.println("[Gemini] Retrying with model: gemini-flash-latest...");
        modelToUse = "gemini-flash-latest";
        url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":streamGenerateContent?alt=sse";
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("x-goog-api-key", s_apiKey);
            http.collectHeaders(responseHeaders, 1);
            status = http.POST(jsonPayload);
            Serial.printf("[Gemini] Retry HTTP status=%d (model=%s)\\n", status, modelToUse.c_str());
        }
    }

    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        Serial.println("[Gemini] Stream failed. HTTP request rejected.");
        return false;
    }

    bool emotionFound = false;
    String fullReply = "";
    bool parseFailed = false;
    bool apiFailed = false;
    size_t textBytes = 0;

    ResponseSink sink([&](const String& jsonData) {
        if (jsonData == "[DONE]") return;
        JsonDocument chunkDoc;
        if (deserializeJson(chunkDoc, jsonData)) { parseFailed = true; return; }
        if (!chunkDoc["error"].isNull()) { apiFailed = true; return; }
        for (JsonObject p : chunkDoc["candidates"][0]["content"]["parts"].as<JsonArray>()) {
            if ((p["thought"] | false) || !p["text"].is<const char*>()) continue;
            String chunkStr = p["text"].as<String>();
            textBytes += chunkStr.length();
            fullReply += chunkStr;

            if (!emotionFound && fullReply.length() < 256) {
                int tagStart = fullReply.indexOf("[EMOTION:");
                int tagEnd = tagStart >= 0 ? fullReply.indexOf("]", tagStart) : -1;
                if (tagEnd > tagStart && tagStart >= 0) {
                    if (onEmotion) onEmotion(parseEmotionTag(fullReply.substring(tagStart, tagEnd + 1)));
                    emotionFound = true;
                }
            }
            if (onToken) onToken(chunkStr);
        }
    });

    const int transferred = http.writeToStream(&sink);
    http.end();

    if (transferred < 0 || parseFailed || apiFailed || textBytes == 0) {
        Serial.printf("[Gemini] Stream failed. Transport=%d parse=%u api=%u text_bytes=%u\\n",
                      transferred, parseFailed, apiFailed, static_cast<unsigned>(textBytes));
        return false;
    }

    Serial.printf("[Gemini] Stream complete. Text bytes=%u\\n", static_cast<unsigned>(textBytes));

    // 音声コールバックがあればTTSで音声を合成・再生
    String cleanSpeech = extractCleanText(fullReply);
    if (!cleanSpeech.isEmpty() && onAudio) {
        Serial.printf("[TTS] Synthesizing speech for: \\"%s\\" (voice=%s)...\\n",
                      cleanSpeech.c_str(), s_voice.c_str());
        generateTTS(cleanSpeech, onAudio);
    }

    return true;
}

// 2. 実機マイク録音PCM (16kHz mono) によるマルチモーダル音声対話 (STT + LLM + TTS)
bool GeminiClient::sendUserAudioDialogue(
    const int16_t* pcmSamples,
    size_t sampleCount,
    EmotionCallback onEmotion,
    TokenCallback onToken,
    AudioChunkCallback onAudio
) {
    if (s_apiKey.isEmpty()) {
        Serial.println("[Gemini] Audio dialogue failed. reason=API_KEY_MISSING");
        return false;
    }
    if (!pcmSamples || sampleCount < 1600) {
        Serial.println("[Gemini] Audio sample too short, falling back to text prompt.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    // 16kHz mono 16-bit PCM を WAVフォーマットにパック
    const uint32_t pcmBytes = sampleCount * sizeof(int16_t);
    const uint32_t wavBytes = 44 + pcmBytes;
    const size_t b64Bytes = ((wavBytes + 2) / 3) * 4;

    uint8_t* wavBuffer = (uint8_t*)heap_caps_malloc(wavBytes, MALLOC_CAP_SPIRAM);
    if (!wavBuffer) {
        Serial.println("[Gemini] PSRAM allocation failed for wavBuffer.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    buildWavHeader(wavBuffer, pcmBytes, 16000);
    memcpy(wavBuffer + 44, pcmSamples, pcmBytes);

    char* b64Audio = (char*)heap_caps_malloc(b64Bytes + 1, MALLOC_CAP_SPIRAM);
    if (!b64Audio) {
        heap_caps_free(wavBuffer);
        Serial.println("[Gemini] PSRAM allocation failed for b64Audio.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    encodeBase64(wavBuffer, wavBytes, b64Audio);
    heap_caps_free(wavBuffer); // WAVバッファは即時解放

    // JSONペイロード構築 (STT文字起こし [HEARD:...], 感情 [EMOTION:...], JST時刻, Google Search Groundingツール)
    String promptInstruction =
        "ユーザーの音声を忠実に認識し、時間、天気、ニュース、雑談など質問に対して正確に1〜2文の愛らしい日本語で答えてください。";
    if (!s_currentDateTime.isEmpty()) {
        promptInstruction += " 現在の日本時間は「" + s_currentDateTime + "」です。時間を尋ねられたらこの現在時刻を正確に答えてください。";
    }
    promptInstruction += " 今日の天気や最新ニュースを聞かれた場合はリアルタイム検索結果に基づいて事実を答えてください。";
    promptInstruction += " 返答フォーマット: [HEARD: 認識したユーザー発話][EMOTION:HAPPYなどの感情タグ] 回答本文";

    // JSONエスケープ
    promptInstruction.replace("\\"", "\\\\\\"");

    String jsonPrefix =
        "{\\"contents\\":[{\\"parts\\":[{\\"inlineData\\":{\\"mimeType\\":\\"audio/wav\\",\\"data\\":\\"";
    String jsonMid =
        "\\"}},{\\"text\\":\\"" + promptInstruction + "\\"}]}],\\"tools\\":[{\\"googleSearch\\":{}}]}";

    const size_t prefixLen = jsonPrefix.length();
    const size_t suffixLen = jsonMid.length();
    const size_t totalJsonLen = prefixLen + b64Bytes + suffixLen;

    char* jsonPayload = (char*)heap_caps_malloc(totalJsonLen + 1, MALLOC_CAP_SPIRAM);
    if (!jsonPayload) {
        heap_caps_free(b64Audio);
        Serial.println("[Gemini] PSRAM allocation failed for jsonPayload.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    memcpy(jsonPayload, jsonPrefix.c_str(), prefixLen);
    memcpy(jsonPayload + prefixLen, b64Audio, b64Bytes);
    memcpy(jsonPayload + prefixLen + b64Bytes, jsonMid.c_str(), suffixLen + 1);
    heap_caps_free(b64Audio); // Base64バッファは即時解放

    String modelToUse = s_model;
    if (modelToUse.isEmpty() || modelToUse == "gemini-3.5-flash" || modelToUse == "gemini-3.8-flash" || modelToUse == "gemini-3.1-flash-lite") {
        modelToUse = "gemini-2.5-flash";
    }

    Serial.printf("[Gemini] Sending %u bytes of audio (%u ms) to %s...\\n",
                  static_cast<unsigned>(pcmBytes),
                  static_cast<unsigned>(sampleCount * 1000 / 16000),
                  modelToUse.c_str());

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(30000);
    http.setReuse(false);

    String url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":generateContent";
    if (!http.begin(client, url)) {
        heap_caps_free(jsonPayload);
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", s_apiKey);

    int status = http.sendRequest("POST", (uint8_t*)jsonPayload, totalJsonLen);
    Serial.printf("[Gemini] Audio dialogue HTTP status=%d (model=%s)\\n", status, modelToUse.c_str());

    // 429または503の場合は、ユーザー音声を破棄せず別のモデルへ自動リトライ
    const char* fallbackModels[] = {"gemini-2.0-flash", "gemini-1.5-flash", "gemini-flash-latest"};
    for (const char* fbModel : fallbackModels) {
        if ((status == 429 || status == 503) && modelToUse != fbModel) {
            http.end();
            Serial.printf("[Gemini] Retrying audio dialogue with fallback model: %s...\\n", fbModel);
            modelToUse = fbModel;
            url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":generateContent";
            if (http.begin(client, url)) {
                http.addHeader("Content-Type", "application/json");
                http.addHeader("x-goog-api-key", s_apiKey);
                status = http.sendRequest("POST", (uint8_t*)jsonPayload, totalJsonLen);
                Serial.printf("[Gemini] Audio retry HTTP status=%d (model=%s)\\n", status, modelToUse.c_str());
            }
        }
    }

    heap_caps_free(jsonPayload); // 送信後は即時解放

    if (status == 429) {
        Serial.println("[Gemini] API status=429 (QUOTA_EXCEEDED). Wait ~1-2 min for rolling RPM recovery, or 16:00/17:00 JST for daily RPD reset.");
    }

    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        Serial.println("[Gemini] Audio dialogue failed across retries.");
        return false;
    }

    ResponseSink sink;
    http.writeToStream(&sink);
    http.end();

    JsonDocument resDoc;
    if (deserializeJson(resDoc, sink.body())) {
        Serial.println("[Gemini] JSON parsing error on dialogue response.");
        return false;
    }

    const char* replyCStr = resDoc["candidates"][0]["content"]["parts"][0]["text"] | "";
    String replyText = String(replyCStr);
    if (replyText.isEmpty()) {
        Serial.println("[Gemini] Empty reply received.");
        return false;
    }

    // 認識されたユーザー発話 (STT) を抽出してシリアルに明示出力
    String heardText = extractHeardText(replyText);
    if (!heardText.isEmpty()) {
        Serial.printf("[STT] Recognized speech: \\"%s\\"\\n", heardText.c_str());
    }

    // 感情タグ抽出
    int tagStart = replyText.indexOf("[EMOTION:");
    if (tagStart >= 0) {
        int tagEnd = replyText.indexOf("]", tagStart);
        if (tagEnd > tagStart) {
            if (onEmotion) onEmotion(parseEmotionTag(replyText.substring(tagStart, tagEnd + 1)));
        }
    } else {
        if (onEmotion) onEmotion(EMOTION_HAPPY);
    }

    String cleanSpeech = extractCleanText(replyText);
    Serial.printf("[Gemini] Reply: \\"%s\\"\\n", cleanSpeech.c_str());
    if (onToken) onToken(cleanSpeech);

    // TTS音声合成・再生
    if (!cleanSpeech.isEmpty() && onAudio) {
        Serial.printf("[TTS] Synthesizing speech for: \\"%s\\" (voice=%s)...\\n",
                      cleanSpeech.c_str(), s_voice.c_str());
        generateTTS(cleanSpeech, onAudio);
    }

    return true;
}

// 3. テキストから24kHz PCM音声を合成 (gemini-3.8-flash-lite-tts, 429時はgemini-3.8-flash-ttsへ自動フォールバック)
bool GeminiClient::generateTTS(const String& text, AudioChunkCallback onAudio) {
    if (s_apiKey.isEmpty() || text.isEmpty() || !onAudio) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(25000);
    http.setReuse(false);

    String ttsModel = "gemini-3.8-flash-lite-tts";
    String url = String("https://") + GEMINI_HOST + "/v1beta/models/" + ttsModel + ":generateContent";
    if (!http.begin(client, url)) return false;

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", s_apiKey);

    JsonDocument doc;
    JsonObject part = doc["contents"].to<JsonArray>().add<JsonObject>()["parts"].add<JsonObject>();
    part["text"] = text;

    JsonObject genCfg = doc["generationConfig"].to<JsonObject>();
    genCfg["responseModalities"].to<JsonArray>().add("AUDIO");
    genCfg["speechConfig"]["voiceConfig"]["prebuiltVoiceConfig"]["voiceName"] = s_voice;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int status = http.POST(jsonPayload);
    Serial.printf("[TTS] HTTP status=%d (model=%s)\\n", status, ttsModel.c_str());

    // 429または503時はgemini-3.8-flash-ttsへ自動リトライ
    if ((status == 429 || status == 503) && ttsModel != "gemini-3.8-flash-tts") {
        http.end();
        Serial.println("[TTS] Retrying with model: gemini-3.8-flash-tts...");
        ttsModel = "gemini-3.8-flash-tts";
        url = String("https://") + GEMINI_HOST + "/v1beta/models/" + ttsModel + ":generateContent";
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("x-goog-api-key", s_apiKey);
            status = http.POST(jsonPayload);
            Serial.printf("[TTS] Retry HTTP status=%d (model=%s)\\n", status, ttsModel.c_str());
        }
    }

    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        Serial.println("[TTS] TTS API rate-limited (429). Triggering cheerful robot chime fallback.");
        AudioTask::playChirp(true);
        return false;
    }

    // ストリーミングデコードでPCMを取り出して即時再生リングバッファへ供給
    TTSResponseSink ttsSink(onAudio);
    http.writeToStream(&ttsSink);
    http.end();
    AudioTask::finishAudioStream();

    Serial.printf("[TTS] Completed! Decoded PCM bytes=%u (~%u ms @ 24kHz)\\n",
                  static_cast<unsigned>(ttsSink.getPcmBytes()),
                  static_cast<unsigned>(ttsSink.getPcmBytes() * 1000 / (24000 * 2)));

    return ttsSink.getPcmBytes() > 0;
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
            Serial.println("\\n[WiFi] Associated with access point. Waiting for DHCP...");
            break;
        case ARDUINO_EVENT_WIFI_STA_GOT_IP:
            Serial.printf("\\n[WiFi] Connected! IP: %s\\n", WiFi.localIP().toString().c_str());
            break;
        case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
            Serial.printf("\\n[WiFi] Disconnected from access point. Reason: %u\\n",
                          static_cast<unsigned>(info.wifi_sta_disconnected.reason));
            break;
        case ARDUINO_EVENT_WIFI_STA_LOST_IP:
            Serial.println("\\n[WiFi] Lost IP address.");
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

    Serial.printf("[System] Startup complete. Camera=%s. Entering SLEEP state.\\n",
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
                    Serial.printf("[VAD] User speech captured (%u samples, %u ms). Sending audio to Gemini...\\n",
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
`,
  },
  {
    name: "WIFI_SETUP_VALIDATION.md",
    path: "WIFI_SETUP_VALIDATION.md",
    description: "WiFiManager移行の検証状態と実機確認手順",
    language: "markdown",
    content: `# WiFiManager移行 R01 検証記録

日付: 2026-10-09（日本時間）
目的: SSID・パスワードをconfig.jsonで配布する方式から、CoreS3 Liteの設定APへスマホで接続して登録する方式へ移行する。
対象: M5Stack CoreS3 Lite / ESP32-S3 / Arduino / PlatformIO m5stack-cores3。
依存関係: espressif32@6.5.0、tzapu/WiFiManager@2.0.17。その他はplatformio.ini参照。

## 作業状態

- [x] 実装・手順更新（2026-10-09）: src/main.cpp、platformio.ini、README.md、data/config.example.json。Web側はapp/page.tsx、components/SettingsPanel.tsx、components/FirmwareCodeStudio.tsx、lib/firmware-sources.ts。
- [x] 静的確認（2026-10-09）: ZIP用README・main.cpp・ConfigManager.h・platformio.iniの実機側ファイルとの一致、設定サンプルと生成JSONの検査。WebのWiFi入力・設定JSON内WiFi項目の除去。WiFi設定が周辺機器初期化に先行すること。ファイル操作診断物残存0件。
- [x] ファームウェアのコンパイル成功（2026-10-09）: PlatformIO Core 6.2.0、Arduino-ESP32 2.0.14、WiFiManager 2.0.17で終了コード0。RAM 53,904 / 327,680 bytes、Flash 1,308,853 / 6,553,600 bytes。成果物: .pio/build/m5stack-cores3/firmware.bin、firmware.elf。初回の取得エラーと案件内キャッシュのパス長制限は、ネットワーク取得の再実行と短い一時キャッシュへ切替えて解消した。
- [x] Webの型検査・ビルド成功（2026-10-09）: TypeScript 5.9.3で変更4ファイルの構文検査と全体のtsc --noEmit --incremental falseが成功。Next.js 15.5.27のnpm run buildも終了コード0。lintは既存設定でスキップされ、Gemini API呼出しは未検証。ビルド時のAPIキー未設定警告は想定どおり。
- [x] ターゲットへのフラッシュ・AP起動確認（2026-10-09）: COM10のESP32-S3 revision v0.2へPlatformIO uploadを実行。書込み・ハッシュ検証・RTSによる再起動が成功（終了コード0）。uploadfsは実行していない。再起動後のシリアル読取り8秒間で起動バナーと \`[WiFi] Setup AP active.\` を確認し、panicやboot-loopマーカーは観測しなかった。生ログや接続情報は保存していない。
- [ ] スマホ設定・接続後の実機検証: スマホの設定画面操作、接続先変更、サーボ等は未確認。R02で再起動後に保存済みWiFiへ接続し、Gemini向けTLS接続に至ることを確認。

ビルド時の解決済みライブラリ: M5Unified 0.1.17、M5GFX 0.2.32、ArduinoJson 7.4.3、WiFiManager 2.0.17（esp32-camera 2.0.4も取得）。既存依存のバージョン範囲は維持しているため、後日のビルドではWiFiManager以外の解決版が変わり得る。今回のツールキャッシュは \`$env:TEMP\\stackchan-pio-r01\`。ライブラリ/Arduino本体の非推奨・戻り値警告は残るが、アプリのコンパイルとリンクは成功した。

## 再現手順

実機用firmwareフォルダ（ZIPでは展開先ルート）で、PlatformIOを利用して以下を実行する。

\`\`\`powershell
pio run -e m5stack-cores3
\`\`\`

Webの依存導入・検査はリポジトリルートで実行する。

\`\`\`powershell
npm install
npx tsc --noEmit
npm run build
\`\`\`

成果物の絶対パス:

- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/.pio/build/m5stack-cores3/firmware.bin\`
- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/src/main.cpp\`
- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/lib/firmware-sources.ts\`

今後の接続後の周辺機器検証ではサーボを無効化した設定から開始する。書込みの再実行手順はREADMEのuploadfs / uploadを参照する。APIキー入り設定はGitに追加しない。

| 確認項目 | 期待する結果 | 状態 |
| --- | --- | --- |
| 設定用APの起動 | StackChan-SetupとブラウザURLが本体に表示される | AP起動ログ確認済み／本体表示・NVS未設定か接続失敗かの判別は未確認 |
| スマホでAPへ接続しhttp://192.168.4.1を開く | Configure WiFiで2.4GHz接続先を選べる | 未実施 |
| 正しい接続先とパスワードをSave | 接続後にAPを終了し通常処理を開始する | 未実施 |
| 本体を再起動 | 保存済みWiFiへ自動接続する | R02の複数回再フラッシュ後、TLS接続成功まで確認済み |
| 誤ったパスワードをSave | 設定用APに再接続して修正できる | 未実施 |
| 保存済み接続先が不在 | 接続待機後に設定用APを起動する | 未実施 |
| 起動直後の5秒間に画面を3秒長押し | 保存済み接続先が利用可能でも設定用APを起動する | 未実施 |
| 接続先を変更して再起動 | 新しい接続先をNVSから復元する | 未実施 |

注意: 設定用APはパスワードなし。接続情報はESP32 NVSに保存する。設定画面は時間制限なく待機し、設定完了前は対話・カメラ・サーボを初期化しない。旧config.jsonのWiFi項目は読み込まないが、既存ファイルに残る項目の自動削除・移行は行わない。

## R02の補足（2026-10-09）

TLSメモリ確保失敗の修正でPSRAMモードを \`qio_qspi\` へ変更し、PSRAM約8MBの認識と実機TLS接続成功を確認した。最新の原因・測定・最終フラッシュ結果は \`TLS_MEMORY_DIAGNOSTIC.md\` を参照する。R01のサイズ値はその時点の値。
`,
  },
  {
    name: "config.example.json",
    path: "data/config.example.json",
    description: "WiFi情報を含まない設定サンプル",
    language: "json",
    content: `{
  "gemini_api_key": "",
  "gemini_model": "gemini-3.5-flash",
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
`,
  },
  {
    name: "TLS_MEMORY_DIAGNOSTIC.md",
    path: "TLS_MEMORY_DIAGNOSTIC.md",
    description: "TLSメモリ不足の原因と実機回帰確認",
    language: "markdown",
    content: `# TLSメモリ確保失敗の修正 R02

日付: 2026-10-09（日本時間）
対象: M5Stack CoreS3 Lite / ESP32-S3 / Arduino-ESP32 2.0.14 / PlatformIO m5stack-cores3 / COM10。
目的: Gemini向けTLS接続の -32512（SSL - Memory allocation failed）を実機で再現・修正する。

## 原因と変更

PSRAMモードが実機と一致せず、PSRAMが使えない状態で描画・音声のバッファが内部RAMへ割り当てられていた。TLS接続直前の内部RAMは約17KBしかなく、最大連続領域は約8KBだった。

CoreS3-Liteの公式回路図3ページ目ではU7 ESPSRAMはSIO0～SIO3のQuad SPI構成。既存platformio.iniはqio_opiだったため、qio_qspiへ変更した。その他のボード定義、Flashサイズ、周辺機器設定、バッファ容量は維持した。変更した条件をPSRAMモードだけに限定し、同じ操作で検証した。

一次情報:

- [M5Stack公式CoreS3-Lite資料](https://docs.m5stack.com/en/core/CoreS3-Lite)（PSRAM 8MB）
- [公式回路図](https://m5stack-doc.oss-cn-shenzhen.aliyuncs.com/490/Sch_M5_CoreS3_v1.0.pdf)（3ページ目U7）
- 実際にビルドしたArduino-ESP32 2.0.14のsdkconfig.h: TLS割当はCONFIG_MBEDTLS_INTERNAL_MEM_ALLOC、Quad SPI設定はtools/sdk/esp32s3/qio_qspi。

## 実測

| 状態 | PSRAM全体 bytes | 内部RAM空き bytes | 最大連続領域 bytes | TLS結果 |
| --- | ---: | ---: | ---: | --- |
| 変更前・TLS直前 | 0 | 17,080 | 8,180 | TLS_ALLOCATION_FAILED |
| qio_qspi・起動時 | 8,388,607 | 313,416 | 278,516 | 接続前 |
| qio_qspi・TLS直前 | 8,388,607 | 250,976 | 241,652 | 接続前 |
| qio_qspi・TLS接続後 | 8,388,607 | 209,004 | 196,596 | TLS_CONNECTED |

変更前は同じエラーを繰り返し観測。変更後はTLS接続成功とStream completeマーカーを観測した。ただしHTTPステータス・Gemini応答内容・音声対話全体の正しさはこの検証の対象外で、成功確認には含めない。

## 再現・回帰確認方法

観測ツール tools/observe_tls.py（R02）はシリアルへコマンドを送らず、DTR/RTSはfalseに設定する。APIキー、SSID、パスワード、会話本文を出力・保存せず、固定のTLS成否だけを表示する。Python 3 / pyserial（既存PlatformIO仮想環境）に依存する。

IDEのCOM10モニターを閉じ、リポジトリルートで実行して45秒以内に同じ画面タップ・発話操作を行う。TLS_ALLOC失敗はTLS接続そのものに依存するため、PC上のモックではなく実機を試験境界にする。Windows環境のため人の操作を伴う再現ループはPythonシリアル監視で構成した。

\`\`\`powershell
& 'C:\\Users\\thoma\\.platformio\\penv\\Scripts\\python.exe' firmware\\tools\\observe_tls.py --port COM10 --seconds 45
\`\`\`

TLS_ALLOCATION_FAILEDは失敗、TLS_CONNECTEDはTLS成功マーカーまたはStream completeを検出した状態。NO_TLS_ATTEMPT_OBSERVEDは判定不能、PORT_UNAVAILABLEなら別のシリアルモニターを閉じる。TLS成功はGemini応答の内容検証を意味しない。

## 作業状態

- [x] 再現用観測ツール作成・同じ実機TLS失敗を再現（2026-10-09）。
- [x] 内部RAM・最大連続空き領域・PSRAMを実測（2026-10-09）。
- [x] メモリモード修正、コンパイル・フラッシュ・ハッシュ検証成功（2026-10-09）。
- [x] 診断版で同じ操作のTLS接続成功を確認（2026-10-09）。
- [x] 一時測定ログとMemoryDiagnostics.hを除去（2026-10-09）。通常の[Gemini] TLS connectedマーカーのみ維持。
- [x] 測定用コードを除いた最終版のコンパイル・フラッシュ・ハッシュ検証成功（2026-10-09）。同じ45秒観測ループでTLS_CONNECTED（終了コード0）。TLS接続成功を2回観測し、TLS_ALLOCATION_FAILEDは観測しなかった。

成果物の絶対パス:

- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/platformio.ini\`
- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/src/GeminiClient.cpp\`
- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/tools/observe_tls.py\`
- \`C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/.pio/build/m5stack-cores3/firmware.bin\`

再発予防: ボードやメモリモード変更時は、マクロ定義だけでPSRAM有効と扱わず、実機のPSRAM容量とTLS接続を確認する。初期化後のRAM不足はホスト上のコンパイルだけでは判定できない。
`,
  },
  {
    name: "observe_tls.py",
    path: "tools/observe_tls.py",
    description: "秘密情報を出力しない実機TLS回帰確認",
    language: "python",
    content: `"""Observe the real TLS failure on the target without sending commands or logging secrets."""
import argparse
import json
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='COM10')
parser.add_argument('--seconds', type=float, default=45)
args = parser.parse_args()
port = serial.Serial()
port.port = args.port
port.baudrate = 115200
port.timeout = 0.25
port.dtr = False
port.rts = False
try:
    port.open()
except serial.SerialException:
    print(json.dumps({'result': 'PORT_UNAVAILABLE', 'port': args.port}))
    raise SystemExit(3)
print('[TLS-OBSERVE] Monitoring started. Trigger the same screen tap / speech.', flush=True)
line_buffer = ''
result = 'NO_TLS_ATTEMPT_OBSERVED'
try:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        line_buffer += port.read(4096).decode('utf-8', errors='replace')
        while '\\n' in line_buffer:
            line, line_buffer = line_buffer.split('\\n', 1)
            if 'SSL - Memory allocation failed' in line or 'start_ssl_client: -32512' in line:
                result = 'TLS_ALLOCATION_FAILED'
                print('[TLS-OBSERVE] TLS_ALLOCATION_FAILED', flush=True)
            elif '[VAD] End of user speech' in line:
                print('[TLS-OBSERVE] TLS attempt triggered', flush=True)
            elif '[Gemini] TLS connected' in line or '[Gemini] Stream complete.' in line:
                if result != 'TLS_ALLOCATION_FAILED':
                    result = 'TLS_CONNECTED'
                print('[TLS-OBSERVE] TLS_CONNECTED', flush=True)
        if len(line_buffer) > 8192:
            line_buffer = ''
finally:
    port.close()
print(json.dumps({'result': result, 'port': args.port, 'seconds': args.seconds}))
raise SystemExit({'TLS_CONNECTED': 0, 'TLS_ALLOCATION_FAILED': 1}.get(result, 2))
`,
  },
  {
    name: "GEMINI_RESPONSE_VALIDATION.md",
    path: "GEMINI_RESPONSE_VALIDATION.md",
    description: "Gemini空応答の実機診断と回帰手順 R03",
    language: "markdown",
    content: `# Gemini空応答の診断・修正 R03

日付: 2026-10-09（日本時間）
目的: TLS接続後に空のStream completeを成功扱いする問題を修正する。
対象: CoreS3 Lite / ESP32-S3 / COM10 / PlatformIO m5stack-cores3。
依存: espressif32 6.5.0、Arduino-ESP32 2.0.14（HTTPClient）、ArduinoJson 7.4.3、Python 3 + pyserial。

## 原因と実機検証

- 変更前の通常リクエストを、周辺機器初期化後の一時的な起動送信で再現。EMPTY_REPLYとTLS read -76を観測した。
- HTTPステータスだけを追加測定すると403。既存実装はステータスとエラー本文を読み捨て、テキストがなくてもtrueを返していた。
- HTTPClientでエラー本文をメモリ内解析し、PERMISSION_DENIED / API_KEY_MISSING_OR_INVALIDを観測。キー値や本文は記録していない。
- 本体のAPIキー空チェックでAPI_KEY_MISSINGを実測。手元のdata/config.jsonにはキーがあったが、本体のLittleFSへ未転送だった。
- uploadfsで既存の手元設定を転送（LittleFS設定置換、NVSのWiFi情報は維持）。書込みハッシュ検証成功。
- 転送直後はHTTP503 / UNAVAILABLE。続く同一リクエストの検証は503、503、200となり、3回目でTEXT_RECEIVED（本文バイト数が0より大きい）を確認した。503はこの検証では一時的だった。HTTP200の成功試行では-76を観測していない。
- 一時的な起動送信は検証後に削除。通常版は利用者の画面タップ・発話終端から呼び出す。
- 通常版の最終ビルドとCOM10書込み成功、フラッシュハッシュ検証成功。firmware.binは1,303,456 bytes、RAM 53,984 bytes。TypeScript型検査と観測Python構文検査は成功。配布ZIPと実ファイルの一致、検証用起動送信の除去を確認。HTTP200の実測は一時的な同一要求の起動再生で行い、最終通常版のタップからの操作は未確認。

## 変更

src/GeminiClient.cpp: HTTPClientでステータスとMIMEを確認し、writeToStreamでHTTPのchunked転送を復号してSSEを処理する。複数data行・イベント境界・複数textパートに対応。エラー本文16KiB、イベント64KiBの上限を設け、本文全体を保持しない。APIキーはURLクエリからx-goog-api-keyヘッダーへ移動した。未設定キー、HTTPエラー、読込エラー、不完全イベント、JSONエラー、空テキストはfalseを返す。ログは数値と固定分類に限定し、応答本文は出力しない。

src/main.cpp: 失敗時は悲しい表情に切り替え、既存の追加入力待機へ戻す。
lib/firmware-sources.ts: 配布ZIP内のコード・README・検証手順を同じ版へ同期。

## 再現方法

IDEのシリアルモニターを閉じ、firmwareフォルダで次を実行し、60秒以内に画面をタップして発話終端まで待つ。

\`\`\`powershell
& 'C:\\Users\\thoma\\.platformio\\penv\\Scripts\\python.exe' tools\\observe_gemini.py --port COM10 --seconds 60
\`\`\`

HTTP_STATUS=200およびTEXT_RECEIVEDで成功。STREAM_FAILEDまたはEMPTY_REPLYは失敗。NO_GEMINI_ATTEMPT_OBSERVEDは未実施であり成功ではない。ツールはDTR/RTSをfalseにし、シリアルへ書き込まず、キー・WiFi情報・応答本文をログに出さない。

設定ファイル変更にはuploadfsが必要。通常のuploadはコードだけを更新する。uploadfsはLittleFS全体をdataフォルダの内容で置き換えるので、現在の設定を確認してから実施する。APIキー入りconfig.jsonとlittlefs.binを共有・コミットしない。

## 完了状況と限界

- [x] 空応答を成功扱いする不具合の修正とキー未転送の解消（2026-10-09）。成果物: 本ディレクトリのsrc/GeminiClient.cpp、src/main.cpp、tools/observe_gemini.py。
- [x] 実機のHTTP403を再現し、キー設定転送後にHTTP200と非空テキスト受信を確認（2026-10-09）。
- [ ] 長時間運用・全ての転送異常パターン・本文意味の検証。
- [ ] 録音の音声認識とTTSの音声対話全体。現在のリクエストは既存の固定文であり、onAudioへ音声を渡す処理は未実装。今回の成功は音声対話完成を意味しない。

一次情報: [Google APIキー仕様](https://ai.google.dev/gemini-api/docs/api-key)、[Google生成API仕様](https://ai.google.dev/api/generate-content)、実ビルドに使用したArduino-ESP32 2.0.14のHTTPClient.cpp（writeToStream）。

## R08: サーボを外したUSB監視（2026-10-09）

利用者へサーボ・Groveを外し本体をPC USBのみで接続する手順を案内し、了承後にCOM10を60秒読み取り監視した。結果: SERIAL_ATTACHED、要求開始3回、HTTP503が1回、HTTP429が2回、応答完了0回。USB切断・CPUパニック・ブラウンアウトのマーカーは観測しなかった。結果NO_COMPLETED_REPLY。この観測だけで長時間安定や物理的なサーボ取り外しを直接検証したとは扱わない。

コード確認: main.cppはGeminiに固定文を送信する。AudioTask.cppは録音をRMS/VADに使い、録音の蓄積や音声のAPI送信はない。GeminiClient.cppはテキストパートのみを扱い、onAudioを呼ばない。音声入力に応じた応答と音声合成は未実装。503/429の詳細原因・クォータ種別は今回の固定マーカー監視では取得していない。Google公式は429/503の一時エラーに対して上限付き指数バックオフを案内する。資料: https://ai.google.dev/gemini-api/docs/troubleshooting

- [x] COM10でサーボ未接続手順後の通信結果と切断なしを60秒観測（2026-10-09）。
- [ ] 429の詳細原因の確認、発話開始判定と再要求抑制の検証。
- [ ] 実録音入力の送信およびTTS出力の実装・実機検証。
`,
  },
  {
    name: "observe_gemini.py",
    path: "tools/observe_gemini.py",
    description: "秘密情報と応答本文を記録しないGemini実機観測",
    language: "python",
    content: `"""Observe Gemini response success/failure without logging credentials or response text."""
import argparse
import json
import time
import serial

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--port', default='COM10')
parser.add_argument('--seconds', type=float, default=45)
args = parser.parse_args()
port = serial.Serial()
port.port = args.port
port.baudrate = 115200
port.timeout = 0.25
port.dtr = False
port.rts = False
try:
    port.open()
except serial.SerialException:
    print(json.dumps({'result': 'PORT_UNAVAILABLE', 'port': args.port}))
    raise SystemExit(3)
print('[GEMINI-OBSERVE] Monitoring started. Trigger the same screen tap / speech.', flush=True)
line_buffer = ''
result = 'NO_GEMINI_ATTEMPT_OBSERVED'
try:
    deadline = time.monotonic() + args.seconds
    while time.monotonic() < deadline:
        line_buffer += port.read(4096).decode('utf-8', errors='replace')
        while '\\n' in line_buffer:
            line, line_buffer = line_buffer.split('\\n', 1)
            if '[Gemini] Stream complete. Total reply:' in line:
                empty = not line.split('Total reply:', 1)[1].strip()
                result = 'EMPTY_REPLY' if empty else 'TEXT_RECEIVED'
                print('[GEMINI-OBSERVE] ' + result, flush=True)
            elif '[Gemini] HTTP status=' in line:
                import re
                match = re.search(r'HTTP status=(-?\\d+)', line)
                if match:
                    code = int(match.group(1))
                    print('[GEMINI-OBSERVE] HTTP_STATUS=' + str(code), flush=True)
                    if code != 200:
                        result = 'HTTP_FAILED'
            elif '[Gemini] Stream complete. Text bytes=' in line:
                import re
                match = re.search(r'Text bytes=(\\d+)', line)
                result = 'TEXT_RECEIVED' if match and int(match.group(1)) > 0 else 'EMPTY_REPLY'
                print('[GEMINI-OBSERVE] ' + result, flush=True)
            elif '[Gemini] API error ' in line:
                import re
                fields = re.findall(r'(?:status|reason|category)=[A-Z0-9_]+', line)
                print('[GEMINI-OBSERVE] API_ERROR ' + ' '.join(fields), flush=True)
            elif '[Gemini] Stream failed.' in line:
                result = 'STREAM_FAILED'
                import re
                reason = re.search(r'reason=([A-Z0-9_]+)', line)
                print('[GEMINI-OBSERVE] STREAM_FAILED' + (' reason=' + reason.group(1) if reason else ''), flush=True)
            elif 'UNKNOWN ERROR CODE (004C)' in line:
                print('[GEMINI-OBSERVE] TLS_READ_ERROR_76', flush=True)
            elif '[VAD] End of user speech' in line:
                print('[GEMINI-OBSERVE] Request triggered', flush=True)
        if len(line_buffer) > 8192:
            line_buffer = ''
finally:
    port.close()
print(json.dumps({'result': result, 'port': args.port, 'seconds': args.seconds}))
raise SystemExit(0 if result == 'TEXT_RECEIVED' else 2 if result == 'NO_GEMINI_ATTEMPT_OBSERVED' else 1)
`,
  },
  {
    name: "CAMERA_VALIDATION.md",
    path: "CAMERA_VALIDATION.md",
    description: "カメラI2C競合の修正と実フレーム検証 R04",
    language: "markdown",
    content: `# カメラI2C初期化失敗の修正 R04

日付: 2026-10-09（日本時間）
目的: 起動時のi2c driver install error / camera sccb init errを修正し、実フレーム取得と起動状態表示を確認する。
対象: M5Stack CoreS3 Lite / ESP32-S3 / COM10 / PlatformIO m5stack-cores3。
依存: espressif32 6.5.0 / Arduino-ESP32 2.0.14 / M5Unified 0.1.17 / M5GFX 0.2.32 / Python 3 + pyserial。

## 原因

既存コードはpin_sccb_sda=12 / pin_sccb_scl=11を指定し、sccb_i2c_port=1で競合を回避できると想定していた。しかし、このSDKではSDAが-1以外のときSCCB_Initへ入り、既定ポートへI2Cドライバを新規登録する。sccb_i2c_portが使われるのはSDA=-1で既存バスを利用するときだけ。

実ビルドのsdkconfig.hではCONFIG_SCCB_HARDWARE_I2C_PORT1=1。M5Unifiedの内部I2Cもポート1であり、ドライバを二重登録して失敗していた。SDA/SCL=-1、sccb_i2c_port=M5.In_I2C.getPort()へ変更すると初期化成功した。電源設定・画像形式・解像度・ピン配線・ライブラリ版を維持し、このバス接続条件だけで初期化結果が変わることを検証した。

一次情報:

- 実使用SDKのtools/sdk/esp32s3/include/esp32-camera/driver/include/esp_camera.h（sccb_i2c_portの条件）。
- M5Unified 0.1.17 src/M5Unified.cpp、M5GFX 0.2.32（内部バスの初期化）。
- [M5Stack公式カメラ例](https://docs.m5stack.com/en/arduino/m5cores3/camera)も既存内部I2Cとの調整が必要な構成を示す。本修正はドライバを解放・再登録する方法ではなく、SDKが提供する既存バス再利用を使う。

## 変更ファイル

- src/CameraMotion.cpp: 既存内部I2Cバス再利用、camera_config_tのゼロ初期化、初回160x120グレースケールフレームのサイズ・形式・長さ確認。確認したフレームを動体検出の初期基準にし、取得失敗時はカメラ資源を解放。後続フレームもサイズ・形式・長さを確認してからアクセスする。
- src/main.cpp: 初期化結果に応じてCamera=readyまたはCamera=unavailable (Touch/Voice mode)を表示。全周辺機器成功という無条件ログを除去。
- tools/observe_camera.py: I2C/SCCB失敗を検出し、実フレーム確認とCamera=readyの両方で成功判定。IP・キー・WiFi情報・画像データを記録しない。
- README.mdおよび配布元lib/firmware-sources.tsを同期。

## 検証と再現

シリアルモニターを閉じ、本体をCOM10へ接続する。firmwareフォルダで、既存のPlatformIO仮想環境を使う。

\`\`\`powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $env:TEMP 'stackchan-pio-r01'
& 'C:\\Users\\thoma\\.platformio\\penv\\Scripts\\platformio.exe' run -e m5stack-cores3 -t upload --upload-port COM10
if ($LASTEXITCODE -eq 0) { & 'C:\\Users\\thoma\\.platformio\\penv\\Scripts\\python.exe' tools\\observe_camera.py --port COM10 --seconds 25 }
\`\`\`

同じ起動経路で観測した結果:

| 版 | 観測 | 判定 |
| --- | --- | --- |
| 修正前 | I2C_INSTALL_FAILED / CAMERA_FAILED / 無条件SYSTEM_READY | CAMERA_FAILED |
| バス再利用のみ | CAMERA_INITIALIZED（フレーム検証は未追加） | 初期化成功、フレーム未検証 |
| 最終版 | CAMERA_FRAME_VERIFIED / CAMERA_INITIALIZED / SYSTEM_READY | CAMERA_FRAME_VERIFIED、camera_ready=true |

最終版はビルド・COM10書込み・フラッシュハッシュ検証成功。TypeScript型検査、観測Python構文検査、配布ZIPと実ファイルの一致確認も成功。firmware.bin 1,303,776 bytes、静的RAM 53,984 bytes。実フレーム確認は寸法160x120・グレースケール・バッファ長19,200 bytes以上を必須とする。観測時間25秒以内にI2C登録失敗・SCCB失敗はなかった。画像データをPCへ転送・保存していない。

PC上のカメラモックではこのドライバ競合を再現できないため、回帰試験は実機起動を試験境界にする。NO_CAMERA_RESULTは未検証であり成功ではない。カメラ未初期化でもタッチ/音声モードへフォールバックする設計を維持する。

## ToDo

- [x] 原因の切り分け、I2C初期化修正、起動ログ修正、実フレーム取得確認（完了2026-10-09、成果物は上記ファイル）。
- [ ] 実際の人の動きによる起床の感度評価、長時間連続撮影、カメラ故障時のフォールバック実機試験。
- [ ] タッチ/音声/Geminiを含む全体の再試験。今回の検証は起動とカメラフレーム取得の範囲。
`,
  },
  {
    name: "observe_camera.py",
    path: "tools/observe_camera.py",
    description: "画像や秘密情報を保存しないカメラ起動観測",
    language: "python",
    content: `"""Observe camera boot/frame results without logging WiFi or credentials."""
import argparse, json, time
import serial
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--port', default='COM10')
p.add_argument('--seconds', type=float, default=25)
a = p.parse_args()
s = serial.Serial()
s.port, s.baudrate, s.timeout = a.port, 115200, 0.25
s.dtr = s.rts = False
try:
    s.open()
except serial.SerialException:
    print(json.dumps({'result': 'PORT_UNAVAILABLE'}))
    raise SystemExit(3)
result = 'NO_CAMERA_RESULT'
buffer = ''
ready = False
try:
    end = time.monotonic() + a.seconds
    while time.monotonic() < end:
        buffer += s.read(4096).decode('utf-8', errors='replace')
        while '\\n' in buffer:
            line, buffer = buffer.split('\\n', 1)
            if 'i2c driver install error' in line:
                print('I2C_INSTALL_FAILED', flush=True)
                result = 'CAMERA_FAILED'
            elif '[Camera] Init warning:' in line or 'camera: sccb init err' in line or '[Camera] Failed' in line:
                print('CAMERA_FAILED', flush=True)
                result = 'CAMERA_FAILED'
            elif '[Camera] GC0308 Initialized successfully' in line:
                print('CAMERA_INITIALIZED', flush=True)
                if result == 'NO_CAMERA_RESULT': result = 'CAMERA_INITIALIZED'
            elif '[Camera] Frame verified:' in line:
                print('CAMERA_FRAME_VERIFIED', flush=True)
                if result != 'CAMERA_FAILED': result = 'CAMERA_FRAME_VERIFIED'
            elif '[System]' in line and 'SLEEP state' in line:
                ready = 'Camera=ready.' in line
                print('SYSTEM_READY' if ready else 'SYSTEM_DEGRADED_OR_OLD_LOG', flush=True)
        if len(buffer) > 8192: buffer = ''
finally:
    s.close()
print(json.dumps({'result': result, 'camera_ready': ready}))
raise SystemExit(0 if result == 'CAMERA_FRAME_VERIFIED' and ready else 1 if result == 'CAMERA_FAILED' else 2)
`,
  },
  {
    name: "POWER_VALIDATION.md",
    path: "POWER_VALIDATION.md",
    description: "サーボ接続時の消灯・USB切断の切り分けと暫定停止 R05",
    language: "markdown",
    content: `# タッチ後の電源断の切り分けと暫定サーボ停止 R05

日付: 2026-10-09（日本時間）
目的: 音声待機終了→Gemini送信時の本体消灯・USB切断を切り分け、サーボ出力を止めた運用を用意する。
対象: CoreS3 Lite / ESP32-S3 / PC USB直結 / 利用者のSG90接続状態 / COM10。
版・依存: PlatformIO espressif32 6.5.0 / Arduino-ESP32 2.0.14 / M5Unified 0.1.17。観測はPython 3 + pyserial。

## 観測と推論

利用者からの実機報告: SG90未接続では問題なし。接続時に画面タッチ後、VAD終了→Gemini送信マーカー直後にCOM10が切断し、画面は消えたまま。PCのUSBへ直結している。原因不明の電源断を反復させる代わりに、この実機ログと接続有無の比較を失敗側の基準とした。

第一候補はサーボ動作による電源負荷。第二候補はソフトウェアの例外・スタック不足、第三候補はUSB経路の問題。ただし電圧低下・過電流保護・機械的負荷のどれが直接原因かは測定していない。

サーボを物理接続したまま、診断版でservo_enabledをメモリ内だけfalseにした。同じ音声待機→VAD→Gemini処理を起動後に1回自動再生し、実機の通常loop内の経路を通した。APIキー・会話本文・設定ファイルを出力せず、LittleFS設定も変更していない。

実機観測（55秒）:

| 項目 | 値 |
| --- | --- |
| 起動後PMICのVBUS値 | 5,015 mV |
| 接続前PMICのVBUS値 | 5,013 mV |
| 接続前の内部RAM空き / 最大連続領域 | 218,444 / 208,884 bytes |
| loopTaskスタック最低余裕・要求前 / 後 | 5,220 / 3,008 bytes |
| リセット理由 | ESP_RST_UNKNOWN (0)、この値で原因を特定していない |
| HTTP / 応答 | 200 / 非空テキスト完了 |
| 観測結果 | REPLY_COMPLETED、attempts=1、replies=1、切断・例外の観測なし |

PMIC値は瞬間的な電圧低下を捕捉する測定ではない。動作時のVBUS・3.3V・サーボ電源をオシロスコープ等で測定していないため、正常電源やブラウンアウトを断定できない。サーボ出力を止めると完走したことは電源/負荷仮説を支持するが、全ソフトウェア故障を否定する結果ではない。

## 暫定運用の変更

- src/main.cpp: STACKCHAN_SERVO_OUTPUT_DISABLED=1でサーボ動作をメモリ内で無効にし、停止理由を起動ログへ表示する。
- platformio.ini: 当面-DSTACKCHAN_SERVO_OUTPUT_DISABLED=1を指定する。data/config.jsonのservo_enabled=trueよりこの停止設定を優先する。
- 検証用の自動起床・自動送信・BootDiagログは通常版から全て除去した。
- tools/observe_disconnect.py: USBの切断・再接続に追従し、例外・通信開始/終了の固定マーカーだけを出力する。DTR/RTSはfalseで、シリアルへ書き込まない。
- READMEとWeb設定画面/配布ZIPへ停止状態と電源上の制約を反映した。

これは暫定措置であり、サーボを使用した状態の問題解決ではない。PWMを止めてもSG90自体の電源線は接続されたままである。

通常版ビルドとCOM10書込み・フラッシュハッシュ検証は成功（firmware.bin 1,303,856 bytes）。実機のHTTP200は一時診断版での自動再生で確認し、通常版での利用者タッチ操作は再確認が必要。設定領域への書込み(uploadfs)は行っていない。

## 再現確認

IDEのシリアルモニターを閉じ、firmwareフォルダで観測する。60秒以内に利用者が画面タップと発話を行う。

\`\`\`powershell
& 'C:\\Users\\thoma\\.platformio\\penv\\Scripts\\python.exe' tools\\observe_disconnect.py --seconds 60
\`\`\`

REPLY_COMPLETEDはこの観測区間の通信完走、FAULT_OR_DISCONNECTは切断/例外検出、NO_COMPLETED_REPLYは成功未確認（操作なし、APIエラーなど）である。切断・クラッシュはUSB経路次第で全メッセージを読めないことがあるので、観測なしは長期安定の証明ではない。

## サーボ再開前の確認

1. 実際のSG90のメーカー/版・定格と電源の仕様を確認する。TowerPro公式SG90 Digitalページは外部アダプター給電を記載し、メーカー回答で4.8～6V・動作電流0.5～2Aとしている。これは利用者のサーボを測定した値ではない。
2. サーボ用の独立した安定化電源を検討する。外部電源のプラスはサーボの赤線へ、GNDはサーボとCoreS3へ共通接続し、信号はG2/G1を維持する。独立電源のプラスをPort Aの5VやPCのUSB5Vへ直結しない。配線変更は電源を切って行う。電源容量は2軸の実負荷・ピーク電流に合わせて選定し、現時点で特定容量の電源を十分と認定していない。
3. ホーンや首機構が引っ掛かっていないか確認し、1軸ずつ動作と電圧を確認する。2軸とWiFi送信の同時負荷でも本体が消灯しないことを実機で確認する。
4. 給電/負荷確認後、STACKCHAN_SERVO_OUTPUT_DISABLEDを0へ変更してビルド・uploadする。LittleFSのservo_enabled=trueで動作再開する。コードだけの変更なのでuploadfsは不要。

一次情報: [TowerPro公式SG90 Digital](https://towerpro.com.tw/product/sg90-7/)、[Espressif公式リセット・ブラウンアウト資料](https://docs.espressif.com/projects/esp-idf/en/v4.4.8/esp32s3/api-guides/fatal-errors.html)、実使用M5UnifiedのPower_Class/AXP2101_Class実装。

## ToDo

- [x] サーボ停止時の実機通信完走、暫定停止版の書込み、自動診断の除去（完了2026-10-09）。成果物: src/main.cpp、platformio.ini、tools/observe_disconnect.py、本記録。
- [ ] 電源断の直接原因の電気的測定、SG90の機構負荷確認、独立給電の配線・仕様確認。
- [ ] サーボ有効状態での再検証と長時間安定運用。未完了。

## R06: 外部給電申告後の再発（2026-10-09）

利用者はTAKAO基板のサーボを外部給電し、本体への給電スイッチをOFF、本体はPC USB給電と申告した。実際の基板版数・スイッチ機能・外部電源容量は未照合。

停止フラグを0にし、ビルド・COM10書込み・ハッシュ検証成功（firmware.bin 1,303,776 bytes）。通常版で利用者がタップ・発話したところ、60秒の観測でGEMINI_REQUEST_STARTED→USB_DISCONNECTEDを記録。結果FAULT_OR_DISCONNECT、attempts=1、replies=0。本体画面も消えたと利用者が確認した。外部給電の申告だけでは再発を防げなかった。

利用者の電源再投入後、停止フラグを1へ戻し、停止版をビルド・書込み・ハッシュ検証成功（firmware.bin 1,303,856 bytes）。自動送信は追加していない。LittleFS設定・WiFi資格情報は変更していない。サーボ有効状態の問題は未解決である。

設計元[Stack-chan_Takao_Base v1.2.1の回路図ソース](https://github.com/akita11/Stack-chan_Takao_Base/blob/main/Stack-chan_Takao_Base_v121.kicad_sch)を静的に確認した。この版ではGrove CN2の5V、サーボJ1/J2の電源が同一+5Vネット。SW1は外部USB CN1のVBUSをその共通+5Vへ接続/切断する。SW1だけではGrove側とサーボ側の5Vを分離しない。OFF状態で外部USB入力が切れ、PC→本体→Groveからサーボへ給電されている可能性がある。ただし実物の版数が未確認なので、この説明が利用者の基板にも当てはまるとは断定しない。

以前の本体給電スイッチOFF案内は、スイッチが本体側だけを切り離せるという前提だった。この前提は確認不足だった。版数と電源経路の照合前にスイッチをONへ変更する案内は行わない。独立給電にはGroveの5V線を分離する必要がある構成もあるため、実物の回路・配線を確認してから判断する。

- [x] 外部給電申告状態での通常版再検証、再発記録、サーボ停止版への復帰（2026-10-09）。
- [ ] 基板表裏の型番/版数/スイッチ表記と電源出力表記の確認。
- [ ] 実際の電源経路の分離、電圧・電流・機構負荷の測定、サーボ有効状態での再検証。

## R07: 外部電源単独でのサーボ再有効化検証（2026-10-09）

利用者からAKITA-038の製品ページ、v1.2 Gerberへのリンクと基板写真が提示された。v1.2の一次PCBデータでCN2 pin3、J1/J2 pin2は共通+5V、SW1 pin1はCN1 USB VBUS、SW1 pin2/pin3は+5Vと確認した。OFFは外部USB入力の切断であり、本体とサーボの5V分離ではない。以前のOFF案内を訂正した。資料: https://github.com/akita11/Stack-chan_Takao_Base/blob/main/Stack-chan_Takao_Base_v12.kicad_pcb

外部電源は利用者申告で5V・最大2.4A。PC USBを外して通常Grove接続・SW1 ON・外部電源単独へ変更した。停止版でまばたき継続・消灯なしを利用者が確認後、STACKCHAN_SERVO_OUTPUT_DISABLED=0へ変更して再ビルド・COM10書込み・フラッシュハッシュ検証成功（firmware.bin 1,303,776 bytes）。LittleFS設定は変更していない。書込み時は外部電源OFF・Groveを外してPC USB接続。動作時はPC USBを外してGroveを戻し外部電源のみで給電する。

再有効化版の実機結果（利用者確認）: 約30秒アニメーションとサーボが正常動作し、その後両方が静止。画面は暗くならず、タップで両方復帰。2回繰り返して確認された。この試験では以前の消灯は再現しなかった。コードには無音タイムアウトでSTATE_SLEEP/EMOTION_SLEEPへ移行し、自動まばたきとランダムサーボ動作を止め、タッチでLISTENING/HAPPYへ戻る処理がある。観察はこの待機動作と整合するが、外部電源単独試験ではシリアルログを取得していないため実際の状態遷移とHTTP結果は直接確認していない。30秒そのものは固定された待機時間ではない。待機中もサーボPWM出力は継続する。

復旧: 電源断が再発した場合は外部電源OFF、Groveを外してPC USBへ接続し、停止フラグ1で再ビルド・書込みする。

- [x] 一次PCB給電ネット照合、電源表示確認、停止版の単体給電での画面安定（2026-10-09）。
- [x] 再有効化版ビルド・COM10書込み・ハッシュ検証、Web型検査、配布ソース内容一致（2026-10-09）。成果物: firmware/platformio.ini、firmware/.pio/build/m5stack-cores3/firmware.bin、components/SettingsPanel.tsx、lib/firmware-sources.ts。
- [x] 外部電源単独でのサーボ動作、消灯なし、静止後のタップ復帰を利用者が2回確認（2026-10-09）。
- [ ] 外部電源単独試験でのHTTP応答結果、電源の過渡波形・ピーク電流・機構負荷・長時間安定性の検証。
`,
  },
  {
    name: "observe_disconnect.py",
    path: "tools/observe_disconnect.py",
    description: "切断・再接続に追従する秘密情報を記録しない診断観測",
    language: "python",
    content: `"""Monitor request/disconnect/reset markers, reconnecting without reset or logging secrets."""
import argparse, json, re, time
import serial
from serial.tools import list_ports
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--seconds', type=float, default=60)
a = p.parse_args()
s = None
buf = ''
attempts = 0
replies = 0
fault = False
disconnect = False
end = time.monotonic() + a.seconds
try:
    while time.monotonic() < end:
        if s is None:
            matches = [x for x in list_ports.comports() if x.vid == 0x303a and x.pid == 0x1001]
            if len(matches) != 1:
                time.sleep(0.25)
                continue
            s = serial.Serial()
            s.port, s.baudrate, s.timeout = matches[0].device, 115200, 0.25
            s.dtr = s.rts = False
            try:
                s.open()
                print('SERIAL_ATTACHED', flush=True)
            except serial.SerialException:
                s = None
                time.sleep(0.25)
                continue
        try:
            buf += s.read(4096).decode('utf-8', errors='replace')
        except (serial.SerialException, OSError):
            print('USB_DISCONNECTED', flush=True)
            disconnect = True
            try: s.close()
            except Exception: pass
            s = None
            buf = ''
            continue
        while '\\n' in buf:
            line, buf = buf.split('\\n', 1)
            if '[BootDiag]' in line:
                safe = re.findall(r'(?:reset|phase|stack|heap|largest|vbus_mv|battery_mv|request)=[A-Z0-9_]+', line)
                print('BOOT_DIAG ' + ' '.join(safe), flush=True)
            elif 'Brownout detector' in line:
                fault = True
                print('BROWNOUT', flush=True)
            elif 'Guru Meditation Error' in line or 'Stack canary' in line or 'stack overflow' in line:
                fault = True
                print('CPU_PANIC_OR_STACK_FAULT', flush=True)
            elif 'watchdog' in line.lower():
                fault = True
                print('WATCHDOG_MESSAGE', flush=True)
            elif '[VAD] End of user speech' in line:
                attempts += 1
                print('GEMINI_REQUEST_STARTED', flush=True)
            elif '[Gemini] HTTP status=' in line:
                match = re.search(r'HTTP status=(-?\\d+)', line)
                if match: print('HTTP_STATUS=' + match.group(1), flush=True)
            elif '[Gemini] Stream complete. Text bytes=' in line:
                replies += 1
                print('GEMINI_REPLY_COMPLETED', flush=True)
            elif '[Gemini] Stream failed.' in line:
                print('GEMINI_REQUEST_FAILED', flush=True)
        if len(buf) > 8192: buf = ''
finally:
    if s is not None: s.close()
result = 'FAULT_OR_DISCONNECT' if fault or disconnect else 'REPLY_COMPLETED' if replies else 'NO_COMPLETED_REPLY'
print(json.dumps({'result': result, 'attempts': attempts, 'replies': replies}))
raise SystemExit(1 if fault or disconnect else 0 if replies else 2)
`,
  },
];
