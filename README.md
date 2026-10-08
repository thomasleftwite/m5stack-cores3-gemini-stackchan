# M5Stack CoreS3 Lite × Gemini AI Voice Stack-chan (スタックチャン)

M5Stack CoreS3 Lite 専用の **Google Gemini 音声対話スタックチャン** システムです。
ディスプレイには M5GFX Canvas スプライトによる 60fps の愛らしい感情アバター（目・眉・口・リップシンク）を表示し、**Port A に接続した SG90×2軸サーボモーター**で感情とうなずきを物理的に連動させます。

---

## 🌟 主な機能とアーキテクチャ

1. **Gemini 3.8 Flash & Gemini 3.8 Flash Lite TTS 直接ストリーミング**
   - ESP32 から直接 `generativelanguage.googleapis.com:443` へ HTTPS/SSE 接続。
   - レスポンスの先頭トークンに含まれる感情タグ（`[EMOTION:HAPPY]`, `[EMOTION:THINKING]` 等）を即時抽出して表情とサーボ首振りを先行トリガー。
   - 24kHz PCM 音声を PSRAM リングバッファ経由で ES8311 スピーカーへ低遅延 DMA 再生。

2. **Port A (G2, G1) SG90 2軸サーボ連携**
   - CoreS3 Lite 本体の赤色 Grove コネクタ（Port A）から直接 50Hz PWM を供給。
   - ESP32-S3 の 14bit LEDC PWM と滑らかなイージング補間により、ギア鳴きや急激な反動を防止。
   - 会話の感情（喜びのうなずき、首かしげ、驚きのけぞり、いやいや首振り）や発話音量 RMS に合わせた自然なしぐさを再現。

3. **GC0308 カメラ動体検知 & VAD 無音自動終了**
   - AXP2101 PMIC の ALDO1 (1.8V) / ALDO2 (2.8V) / ALDO4 (3.3V) を明示通電し、カメラを安定起動。
   - QQVGA (160×120) 高速差分法（CPU 負荷 3〜5% 未満）により、人の接近で自動ウェイクアップ。
   - 一定時間（デフォルト 6秒）無音が続くと、「またね！」とスリープ状態（SLEEP）へ自動復帰。

4. **自己修復型 LittleFS & パラメータ設定 (`config.json`)**
   - LittleFS パーティションが未フォーマットの場合でも、ファームウェアが自動フォーマットとデフォルト設定ファイルの生成を実施。

---

## ⚙️ `config.json` セットアップ手順

`config.json` は、Wi-Fi 接続情報、Gemini API キー、サーボの各設定を記録する設定ファイルです。

### 方法 1: PlatformIO で Flash に転送する（推奨）
1. プロジェクトルートの `data/config.json` をエディタで開き、お使いの Wi-Fi 情報と Gemini API キーを入力します。
2. VS Code の PlatformIO サイドバーから **[Upload Filesystem Image]**（またはターミナルで `pio run -t uploadfs`）を実行します。
3. `data/config.json` が CoreS3 Lite の LittleFS 領域へ書き込まれます。
4. 続けて **[Upload]**（`pio run -t upload`）でプログラム本体を書き込みます。

### 方法 2: 自動生成機能を利用する（ファイル書き込み不要）
- 本ファームウェアは、LittleFS 内に `config.json` が存在しない場合、**自動的にデフォルト設定で `/config.json` を新規作成して LittleFS に保存**します。
- その後、シリアルまたは Web 設定画面からパラメータを書き換えることが可能です。

---

## 📋 `config.json` 各項目の設定リファレンス

```json
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
```

### パラメータ詳細一覧

| キー | 型 | デフォルト値 | 説明・設定の目安 |
| :--- | :--- | :--- | :--- |
| `wifi_ssid` | 文字列 | `""` | 接続先 Wi-Fi の SSID（**2.4GHz 帯** のみ対応。5GHz は非対応） |
| `wifi_password` | 文字列 | `""` | Wi-Fi の WPA2/WPA3 パスワード |
| `gemini_api_key` | 文字列 | `""` | Google AI Studio で発行した Gemini API キー（`AIzaSy...`） |
| `gemini_model` | 文字列 | `"gemini-3.8-flash"` | 使用する対話モデル名。通常は `"gemini-3.8-flash"` を指定 |
| `tts_voice` | 文字列 | `"Kore"` | 音声モデル（`gemini-3.8-flash-lite-tts`）の声質。<br>選択肢: `Kore` (標準/落ち着いた声), `Puck` (明るい声), `Fenrir` (低音), `Zephyr` (透明感), `Charon` (深み) |
| `wake_word` | 文字列 | `"スタックちゃん"` | 対話を開始するキーワード |
| `silence_timeout_sec` | 整数 | `6` | 発話が途切れてから会話を終了（スリープ）するまでの無音秒数（3〜20） |
| `camera_motion_threshold` | 整数 | `25` | カメラ動体検知の感度（10〜60）。低いほど敏感に反応、高いほど大きな動きのみ検知 |
| `spk_volume` | 整数 | `160` | 内蔵スピーカー音量（0〜255）。音割れを防ぐため 140〜180 推奨 |
| `mic_gain` | 整数 | `80` | ES7210 デュアルマイクの入力ゲイン（0〜128） |
| `servo_enabled` | 真偽値 | `true` | Port A SG90 サーボ動作の有効/無効フラグ（`true` / `false`） |
| `servo_pan_pin` | 整数 | `2` | 水平首振り（Yaw / 左右）サーボ信号線の GPIO 番号。<br>**Port A Pin 1 (SDA) = GPIO 2** |
| `servo_tilt_pin` | 整数 | `1` | 垂直うなずき（Pitch / 上下）サーボ信号線の GPIO 番号。<br>**Port A Pin 2 (SCL) = GPIO 1** |
| `servo_pan_center` | 整数 | `90` | 水平サーボのニュートラル角度トリム（度: 70〜110）。組み立てズレの微調整用 |
| `servo_tilt_center` | 整数 | `90` | 垂直サーボのニュートラル角度トリム（度: 70〜110）。正面を向く角度を指定 |

---

## 🔌 Port A (Grove Red) SG90 サーボ結線仕様

M5Stack CoreS3 Lite 本体の赤色 Grove コネクタ（Port A）に、SG90 マイクロサーボ×2軸を以下のように接続します。

```text
[ CoreS3 Lite Port A (Red Grove) ]
  ├── Pin 1 (黄 / SDA) ──> Pan サーボ信号線 (橙)  [GPIO 2]  ── 水平 首振り (Yaw)
  ├── Pin 2 (白 / SCL) ──> Tilt サーボ信号線 (橙) [GPIO 1]  ── 垂直 うなずき (Pitch)
  ├── Pin 3 (赤 / 5V)  ──> 両サーボ 電源線 (赤)   [5V VBUS] ── AXP2101 Boost給電
  └── Pin 4 (黒 / GND) ──> 両サーボ GND線 (茶/黒) [GND]     ── 共通グランド
```

> **電源に関する注記:** CoreS3 Lite 内蔵の AXP2101 PMIC が USB 電源から Boost 5V レールを給電するため、一般的な USB-C ケーブル給電で SG90×2軸を安定駆動できます。

---

## 🛠️ ハードウェア実装のポイント（技術的対策済み）

本ファームウェアでは、CoreS3 実機で頻発する以下の問題をすべて解決しています：
1. **LEDC PWM 14bit 制限**: ESP32-S3 で 50Hz 出力時、分解能は最大 14bit（`SERVO_LEDC_RES 14`）に厳密に適合。
2. **I2S 二重初期化防止**: `M5Unified` の `M5.begin()` で安全にオーディオドライバを起動し、重複登録エラーを防止。
3. **GC0308 カメラ電源レール昇圧**: AXP2101 の ALDO1 (1.8V) / ALDO2 (2.8V) / ALDO4 (3.3V) を起動時に通電し、SCCB 通信には I2C ポート 1 を割り当てて内部バスとの衝突を防止。

---

## 🐙 GitHub リポジトリの作成とプッシュ手順

本プロジェクトをお手元の GitHub アカウントへ新しいリポジトリとしてプッシュする手順です。

### 1. ローカルリポジトリの初期化とコミット
```bash
git init
git add .
git commit -m "feat: M5Stack CoreS3 Lite Gemini AI Voice Stack-chan with Port A SG90 servo"
```

### 2. GitHub リポジトリを作成してプッシュ

#### 方法 A: GitHub CLI (`gh`) を使用する場合（最も簡単）
```bash
gh repo create m5stack-cores3-gemini-stackchan --public --source=. --push
```

#### 方法 B: 通常の git コマンドを使用する場合
1. [GitHub](https://github.com/new) で新しいリポジトリ（例: `m5stack-cores3-gemini-stackchan`）を作成します。
2. 作成したリポジトリの URL をリモートに追加してプッシュします：
```bash
git remote add origin https://github.com/<YOUR_GITHUB_USERNAME>/m5stack-cores3-gemini-stackchan.git
git branch -M main
git push -u origin main
```

---

## 📜 ライセンス
MIT License
