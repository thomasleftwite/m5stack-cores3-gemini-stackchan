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

## ⚙️ WiFiセットアップ（WiFiManager）

初回起動時、または保存済みWiFiへ20秒以内に接続できない場合、本体は設定用AP `StackChan-Setup` を起動します。設定中は対話・カメラ・サーボの初期化を待ちます。

1. スマホのWiFi設定で `StackChan-Setup` に接続します（設定用APはパスワードなし）。インターネット接続がない旨の表示が出ても接続を維持してください。
2. 自動表示される設定画面、またはブラウザの **http://192.168.4.1** を開きます。
3. **Configure WiFi** から接続先の2.4GHz WiFiを選び、パスワードを入力して **Save** を押します。
4. 接続成功後、設定用APを終了し、通常の対話処理を開始します。スマホを通常のWiFiへ戻してください。

SSID・パスワードはESP32のNVSへ保存され、次回起動時は自動接続します。パスワードを間違えた場合は設定用APへ再接続してやり直します。設定画面は時間制限なく待機します。

### WiFiを再設定する

本体を再起動し、起動画面が出てから5秒以内にタッチ画面を3秒間押し続けてください。保存済みWiFiが利用可能でも設定用APを起動します。保存済み設定を起動ごとに消去する処理はありません。

### その他の設定（config.json）

`data/config.json` が手元にない場合は `data/config.example.json` をコピーして作成します（既存の設定は上書きしないでください）。

Gemini APIキー・音声・サーボ等は引き続き `data/config.json` で設定します。WiFi情報は含めません。旧 `wifi_ssid` / `wifi_password` 項目が残っていても利用されないため、手元の設定から取り除いてください。

実機用フォルダはリポジトリの `firmware/` です。以下はそのフォルダで実行します（Webから取得したZIPでは展開先ルート）。

```powershell
pio run -e m5stack-cores3
# 以下は実機へ書き込む際に実行
pio run -e m5stack-cores3 -t uploadfs
pio run -e m5stack-cores3 -t upload
pio device monitor -b 115200
```

`uploadfs` はGemini等の設定転送に使用します。WiFi設定だけなら不要です。LittleFSに設定ファイルがなければデフォルトを生成しますが、Gemini APIキーは別途設定が必要です。APIキー入りファイルや接続情報をコミット・共有しないでください。

### 対象・依存関係・検証

WiFi設定変更版: **2026-10-09 / WiFiManager移行 R01**。対象はM5Stack CoreS3 Lite、ESP32-S3 / Arduino、PlatformIO環境 `m5stack-cores3`。既存の `esp32s3box` ボード定義と `espressif32@6.5.0` を維持し、WiFiManagerを `tzapu/WiFiManager@2.0.17` に固定しています。その他の依存関係は `platformio.ini` を参照してください。

実機では、未設定起動 → スマホ設定 → 接続 → 再起動による自動接続、誤パスワードからの再入力、保存済みAP不在時の設定画面、画面長押しによる再設定を確認します。2026-10-09に実機への書込みとシリアルログによる設定AP起動を確認しました。保存済みWiFiへの再起動後の接続とTLS接続はR02で確認済みです。スマホ画面での設定操作・接続先変更・サーボ等の全体動作は未確認です。検証状態は `WIFI_SETUP_VALIDATION.md` を参照してください。

一次情報: [WiFiManager公式リポジトリ](https://github.com/tzapu/WiFiManager)

---

## 📋 `config.json` 各項目の設定リファレンス

```json
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
```

### パラメータ詳細一覧

| キー | 型 | デフォルト値 | 説明・設定の目安 |
| :--- | :--- | :--- | :--- |
| `gemini_api_key` | 文字列 | `""` | Google AI Studio で発行した Gemini API キー（`AIzaSy...`） |
| `gemini_model` | 文字列 | `"gemini-3.8-flash"` | 使用する対話モデル名。通常は `"gemini-3.8-flash"` を指定 |
| `tts_voice` | 文字列 | `"Kore"` | 音声モデル（`gemini-3.8-flash-lite-tts`）の声質。<br>選択肢: `Kore` (標準/落ち着いた声), `Puck` (明るい声), `Fenrir` (低音), `Zephyr` (透明感), `Charon` (深み) |
| `wake_word` | 文字列 | `"スタックちゃん"` | 対話を開始するキーワード |
| `silence_timeout_sec` | 整数 | `6` | 発話が途切れてから会話を終了（スリープ）するまでの無音秒数（3〜20） |
| `camera_motion_threshold` | 整数 | `25` | カメラ動体検知の感度（10〜60）。低いほど敏感に反応、高いほど大きな動きのみ検知 |
| `spk_volume` | 整数 | `160` | 内蔵スピーカー音量（0〜255）。音割れを防ぐため 140〜180 推奨 |
| `mic_gain` | 整数 | `80` | ES7210 デュアルマイクの入力ゲイン（0〜128） |
| `servo_enabled` | 真偽値 | `true` | Port A SG90 サーボ動作の有効/無効フラグ。R05ではビルド設定による停止が優先されます |
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
  ├── Pin 3 (赤 / 5V)  ──> 独立給電ではサーボ電源線へ接続しない
  │                       両サーボの赤線は定格に合う独立電源へ接続
  └── Pin 4 (黒 / GND) ──> 両サーボ GND線 (茶/黒) [GND]     ── 共通グランド
```

> **電源に関する注記:** PCのUSB給電でSG90を動かした際、本体消灯・USB切断の報告がありました。一般的なUSB給電で2軸を安定駆動できるとは保証できません。R05はサーボ出力を停止しています。独立給電ではGNDを共通にし、外部電源のプラスとPort Aの5Vを直結しないでください。詳細は `POWER_VALIDATION.md` を参照してください。

---

## 🛠️ ハードウェア実装のポイント（技術的対策済み）

本ファームウェアでは、CoreS3 実機で頻発する以下の問題をすべて解決しています：
1. **LEDC PWM 14bit 制限**: ESP32-S3 で 50Hz 出力時、分解能は最大 14bit（`SERVO_LEDC_RES 14`）に厳密に適合。
2. **I2S 二重初期化防止**: `M5Unified` の `M5.begin()` で安全にオーディオドライバを起動し、重複登録エラーを防止。
3. **GC0308 カメラI2C共有**: M5Unifiedの既存内部I2Cバスを再利用し、ドライバの二重登録を防止。起動時に160x120グレースケールフレーム取得を確認（R04）。

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

## TLSメモリ不足修正（2026-10-09 / R02）

CoreS3 LiteのQuad SPI PSRAMに合わせ、`board_build.arduino.memory_type = qio_qspi` を使用します。旧OPI設定では実機のPSRAM容量が0になり、TLSが内部RAM不足で失敗しました。変更後は約8MBのPSRAM認識とTLS接続成功を実測しています。詳細と回帰確認手順は `TLS_MEMORY_DIAGNOSTIC.md` を参照してください。

## Gemini空応答修正（2026-10-09 / R03）

本体LittleFSのAPIキーが未設定でHTTP403が返っていました。設定転送後、HTTP200と非空テキスト受信を実機確認しました。通常のuploadは設定を更新しないため、Gemini設定を変更した場合はuploadfsが必要です（LittleFS全体を置換します）。HTTPエラー・空応答は成功扱いせず、ログはステータスと本文バイト数だけに限定します。再現手順・検証範囲は `GEMINI_RESPONSE_VALIDATION.md` を参照してください。音声認識・TTSの対話全体は未完成です。

## カメラ初期化修正（2026-10-09 / R04）

SCCBによる内部I2Cドライバの二重登録を修正しました。実機の160x120グレースケールフレーム取得とCamera=readyの起動状態を確認しました。カメラ利用不可時は起動ログにフォールバック状態を表示します。詳細と再現手順は `CAMERA_VALIDATION.md` を参照してください。動体による起床感度と長時間運用は未検証です。

## サーボ接続時の電源断対策（2026-10-09 / R05）

サーボ接続時の本体消灯・USB切断を調査中です。サーボ出力停止時にはHTTP200と応答受信を確認しました。暫定版では `STACKCHAN_SERVO_OUTPUT_DISABLED=1` が `servo_enabled` より優先され、サーボは動きません。給電と機構負荷の確認後に0へ変更して再ビルドしてください。サーボ有効状態での修正完了は未確認です。詳細は `POWER_VALIDATION.md` を参照してください。

## 外部給電申告後の再検証（2026-10-09 / R06）

サーボを再有効化してタッチ→Gemini送信を確認しましたが、本体消灯・USB切断が再発しました。現在はサーボ停止設定1へ戻しています。設計元TAKAO基板v1.2.1はGroveとサーボの5Vが共通であり、スイッチだけで給電分離できるとは限りません。実物の版数・配線・電源容量を確認するまで電源分離完了とは扱いません。詳細は `POWER_VALIDATION.md` のR06を参照してください。

## 外部電源単独での検証（2026-10-09 / R07）

現在はSTACKCHAN_SERVO_OUTPUT_DISABLED=0のサーボ再有効化版です。TAKAO v1.2はGroveとサーボの5Vが共通で、SW1は外部USB入力を切ります。動作時はPC USBを外し、通常Grove接続・SW1 ON・5V最大2.4Aの外部電源のみを使用します。書込み時は外部電源OFF、Groveを外してPC USBへ接続します。
利用者がアニメーション・サーボ正常動作、消灯なし、静止後のタップ復帰を2回確認しました。静止と復帰はコードの待機処理と整合します。長時間安定性・電源過渡測定・この構成でのHTTP応答結果は未検証です。詳細はfirmware/POWER_VALIDATION.md（firmware内ではPOWER_VALIDATION.md）を参照してください。
