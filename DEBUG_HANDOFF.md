# デバッグ引継ぎ & プロジェクト要約（2026-10-09 / R09）

## 対象と現在の状態
M5Stack CoreS3 / CoreS3 Lite（ESP32-S3、8MB Quad SPI PSRAM、AW88298 I2Sアンプ、ES7210 I2Sマイク、GC0308カメラ）用Arduino/PlatformIOファームウェアと、Next.jsの設定・配布ZIP生成アプリ。
目的はWiFiセットアップとGeminiによる音声対話・表情サーボ連動スタックチャン。

- **ビルド対象**: `firmware/platformio.ini` の `m5stack-cores3`
- **現在のファームウェア版**: **R09**（NTP時計同期・Geminiマルチモデルフォールバック・TTS 429フォールバックチャイム・半二重I2S切替安定化・STT認識ログ出力）
- **Gitリポジトリ**: `https://github.com/thomasleftwite/m5stack-cores3-gemini-stackchan.git`（ブランチ: `main`）
- **セキュリティ方針**: APIキーを含む `firmware/data/config.json` はローカル専用・Git除外。チャットやコミット履歴にキー本文を出力・保存しない。

---

## 経過と確認範囲（R01〜R09）

| 版 | 主な課題・変更 | 検証結果・実機挙動 |
| :--- | :--- | :--- |
| **R01** | JSON固定SSIDからWiFiManagerへの移行。接続失敗時はAP立ち上げ | AP名 `StackChan-Setup`、`192.168.4.1`。NVSにWiFi設定を保存。 |
| **R02** | TLSメモリ確保失敗。PSRAM設定を `qio_opi` から `qio_qspi` へ修正 | PSRAM約8MBを正常認識し、Google APIへのTLS接続成功。 |
| **R03** | HTTPエラーを空の成功として扱っていた不具合を修正。HTTPClientストリーミング対応 | HTTP 200, 403, 503のステータス明示。キーはHTTPヘッダー (`x-goog-api-key`) で安全に送信。 |
| **R04** | カメラSCCBのI2C重複初期化 | M5Unifiedの内部I2Cポートを共用。GC0308 160×120 QQVGA動体検知の初期化成功。 |
| **R05/R06**| SG90サーボ接続時に本体消灯・USB切断の再発調査 | サーボ突入電流と給電経路の競合を特定。 |
| **R07** | TAKAO v1.2の給電経路調査。PC USBを外し、外部電源単独・SW1 ONへ変更 | 5V 2.4A外部給電でサーボ・表情が正常動作。画面消灯なしを確認。 |
| **R08** | サーボ/Groveを外してPC USB直結で通信監視 | COM10にて429/503のエラー観測。ブラウンアウト等の電源断なし。 |
| **R09** | **STT録音・Gemini対話・NTP時計同期・429クォータ対策・TTSチャイム** | **【詳細下記】**<br>1. 実録音PCM送信とVAD判定最適化<br>2. 応答の一律化の根本原因解明と修正<br>3. Gemini 429自動フォールバック (flash 3.5 → flash-latest → 3.1-flash-lite)<br>4. TTS 429対策 (flash-lite-tts → flash-tts → ロボットチャイム音)<br>5. NTP時刻同期による正確な日時認識<br>6. CoreS3の半二重I2S切替安定化 (`I2S_NUM_0` 再初期化順序) |

---

## 重要課題と分析・解決策（R09）

### 1. 「時間や天気、ニュースを聞いても回答が一律に感じる」根本原因
実機シリアルログの分析:
```text
[Audio] Recording stopped. Samples=24832 (1552 ms), speechSamples=256 (16 ms)
[VAD] Tap or brief prompt. Sending text greeting to Gemini...
```
- **原因**: ユーザーが発話しても、マイク感度やVADの閾値により「有音サンプル数」が16ms（256サンプル）しかカウントされず、100ms未満判定となった。
- これによりファームウェアが「タップまたは極短発話」とみなし、固定プロンプト `「こんにちは！元気？」` をGeminiに送信していたため、Gemini側も毎回「こんにちは！今日もとっても元気だよ！」と一律の挨拶を返していた。
- **対策**:
  1. VADの無音判定を1.5秒から1.2秒へ最適化し、発話区間の蓄積アルゴリズムを改善。
  2. Geminiからの認識結果ログ（`[STT] Recognized speech: "..."`）を出力するようにし、何を認識したのかシリアルモニタで可視化。
  3. NTP経由でJST時刻（`%Y年%m月%d日 %H時%M分`）を取得し、システムプロンプトに現在時刻を動的注入。Geminiが今の日時を正確に把握した上で対話可能に。

### 2. Gemini API クォータ制限（429 QUOTA_EXCEEDED）と上限回復の仕組み
- **RPM (Requests Per Minute / 1分間リクエスト数)**:
  - 60秒のローリングウィンドウで管理されます。連続して発話した際の一時的な429は、約1分待つことで回復します。
- **RPD (Requests Per Day / 1日あたりリクエスト数)**:
  - 無料枠の1日制限は **米国太平洋時間（PT）の午前0時（00:00 PT）** にリセットされます。
  - **日本時間（JST）換算: 毎日 16:00 JST（夏時間/PDT期間）または 17:00 JST（標準時/PST期間）** に全枠が回復します。
- **ファームウェア側での自律回避設計**:
  - Geminiモデル階層自動切替: `gemini-3.5-flash` → `gemini-flash-latest` → `gemini-3.1-flash-lite`（モデルごとに別枠の制限が適用される場合があるため、即座に低負荷モデルで再試行）。
  - TTSモデル階層自動切替: `gemini-3.8-flash-lite-tts` → `gemini-3.8-flash-tts`。
  - TTS全枠枯渇時: クラッシュや完全沈黙を避け、`AudioTask::playChirp(true)` により可愛らしい電子チャイム音で応答を受領したことを表現。

### 3. ビルド修正記録（2026-10-09 / R10）
- `firmware/src/GeminiClient.cpp`:
  - `Base64StreamDecoder` コンストラクタ初期化子リストから未宣言の `totalDecodedBytes_(0)` を削除。
  - `AudioTask::playChirp` および `AudioTask::finishAudioStream` を呼び出すため、`#include "AudioTask.h"` を追加。
  - `lib/firmware-sources.ts` にも最新の `GeminiClient.cpp` を同期反映。

### 4. 429クォータ枯渇ループ遮断とモデルフォールバック刷新（2026-10-09 / R11）
- **現象**: 実機ログにて、Gemini APIが429（RESOURCE_EXHAUSTED / QUOTA_EXCEEDED）を返した際、ファームウェアが即座に `STATE_WAIT_FOLLOWUP` に遷移し、マイクの環境音等で直ちに `isVoiceDetected()` が発火して2〜3秒間隔で連続再リクエスト（15秒間に4回）を送信し続ける無限ループが発生。これにより無料枠の毎分制限（RPM）を自ら超え続ける事態となっていた。
- **回復タイミング**:
  - **RPM (1分あたり制限)**: 連続送信ループ停止後、**約1〜2分** の待機でローリングウィンドウにより自動回復。
  - **RPD (1日あたり制限)**: 日次枠を使い切った場合は、**米国太平洋時間 00:00 PT = 日本時間 16:00 JST（夏時間）/ 17:00 JST（標準時）** に全枠リセット。
- **ファームウェア対策 (`main.cpp` & `GeminiClient.cpp`)**:
  1. `!requestOk`（429やエラー時）は困惑チャイム音 `AudioTask::playChirp(false)` を鳴らし、再生完了後は `STATE_WAIT_FOLLOWUP` ではなく **`STATE_STANDBY_WAIT_KEYWORD`** へ遷移させて即時ループを完全遮断。
  2. 再生後の追従待機クールダウンを600msから1200msへ拡張。
  3. 標準モデルを実在する最新高速モデル `gemini-2.5-flash` に統一し、フォールバックチェーン（`gemini-2.0-flash` → `gemini-1.5-flash` → `gemini-flash-latest`）を整備。
  4. 429検出時にシリアルへ回復目安時間を診断表示。

### 5. モデル非推奨(404 NOT_FOUND)の修正とアクティブモデルチェーン（2026-10-09 / R12）
- **原因判明**:
  - `gemini-2.5-flash` および `gemini-2.5-flash-lite` はGoogle Gemini APIにおいて新規利用終了（Deprecated / 404 NOT_FOUND: `"This model models/gemini-2.5-flash is no longer available to new users."`）となっていた。
- **恒久対策**:
  - デフォルトモデルおよびフォールバックモデルを現在アクティブに提供されている最新モデルへ移行：
    - 主力モデル: `gemini-flash-latest` (HTTP 200 OK 検証済)
    - 自動フォールバックチェーン: `gemini-3.1-flash-lite` -> `gemini-3.8-flash` -> `gemini-3.5-flash` -> `gemini-flash-latest`
    - `ConfigManager.h` および `GeminiClient.cpp` 内で、万一設定ファイルや古い引数に `gemini-2.5-flash` などの廃止モデルが残っていても `gemini-flash-latest` へ自動正規化する安全ガードを追加。
  - TTS音声合成は検証済の `gemini-3.8-flash-lite-tts` (フォールバック: `gemini-3.8-flash-tts`) を維持。

### 6. ハードウェア給電（TAKAO v1.2 & SG90）とI2Sの留意事項
- **給電経路**: TAKAO v1.2 の Grove CN2 pin3 とサーボ pin2 は同一の +5V ネット。
  - サーボ動作時はPC USBを外し、TAKAO外部USB電源（5V 2A以上）単独・SW1 ONで給電する。
  - PC USB書込み・シリアル監視時は、Groveコネクタを抜いてPC USB単独で接続する。
- **半二重I2S切替**: CoreS3は録音用マイク（ES7210）と再生用アンプ（AW88298）が共通の `I2S_NUM_0` を共有するため、再生前後に必ず `i2s_driver_uninstall` → `i2s_driver_install` を行い、適切なクロックとDMAバッファを設定する。

---

## 再開手順 & ビルドコマンド
1. **ファームウェアビルド & 書込み**:
   ```bash
   platformio run -e m5stack-cores3 -t upload --upload-port COM10
   ```
2. **シリアル監視**:
   ```bash
   python firmware/tools/observe_gemini.py --port COM10 --seconds 60
   ```
3. **Webアプリ（Next.js）検証**:
   ```bash
   npm run build
   ```
