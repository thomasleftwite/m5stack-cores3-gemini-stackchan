# R13.3 UI03 / 応答失敗の診断D2

日付: 2026-10-26 JST。対象: COM10のCoreS3 Lite、PC USBのみ・Groveなし。
目的: ウェイクワードに反応するが、1度目は無回答、2度目は「応答できませんでした」となる事象を切り分ける。
利用者は「録音中」の表示を見て質問も別に発話済みと回答。呼びかけだけで回答を待っていたとは扱わない。
保存済み登録データ、設定、Wi-Fiを保持して調査する。開発中のフラッシュは利用者が継続承認済み。

## 取得済みの根拠

- UI03の表示仕様は利用者が希望どおりと確認済み。
- `docs/validation/r13-fault-20261026-121344/report.json`では起動後にReady→Cue→Recording→Busy→Errorを記録。Busy開始からErrorは約164msで、既存API状態ログはなかった。
- 現行GeminiClientはHTTP実行より前にも、Wi-Fi切断・時刻未同期・要求メモリ不足・TLS準備失敗等で失敗する。開始時の入力/キー不備、ワーカー生成失敗等もある。
- これらは表示上区別されず、HTTP実行前には既存の状態ログも出ない。そのため、現時点では原因を特定したとは扱わない。

## D2診断

正本: `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/src/RequestFailure.h`。
main.cppの応答エラー進入時だけ、固定失敗文言を分類した数値・処理段階・HTTP状態を出す。エラー文字列そのものは出さず、未知の内容は99にする。
タグは`[DEBUG-r13d2]`、起動識別`[DIAG] revision=D2`。D1のモード/保存/5秒ログを引き継ぐ。
ホストの`tools/observe_r13_fault.py`も固定形式と数値だけを保存する。音声・認識文・API本文・キー・設定値を保存しない。

主なreason: 1=時刻未同期、2=Wi-Fi切断、3=要求メモリ、4=TLS準備、5=入力/キー不備、6=ワーカー生成、7=録音用メモリ、8=再試行待機。
9～20は認識結果・回答・音声形式・応答形式/長さ/受信・API状態等。正確な対応はヘッダーを参照。
この段階では送信/録音/ウェイク/再試行の動作は変更しない。数値の診断追加は原因修正ではない。

## 依存・再現方法

既存のespressif32@6.5.0 / Arduino-ESP32 2.0.14 / M5Unified 0.1.17 / M5GFX 0.2.32を維持。
オフラインC++、描画11項目、配布同期、Web型検査、PlatformIOビルドを使う。
`tools/flash_r13_ui_app.py`でアプリのみ更新し、起動用領域・NVS・FATの前後不変を確認。
起動後の同じシリアル接続で観測中、利用者が呼びかけ→録音中表示→質問を1回実施する。

## ToDo

- [x] 失敗理由を固定数値へ分類する診断を追加（2026-10-26）。
- [x] ビルド・既存オフライン/登録保存/再試行・表示11項目・分類/数値許可リスト・48配布一致・型検査成功（2026-10-26）。
- アプリ画像2,120,016 bytes、SHA256 `58159864c868755c15e3f0e6656ae76b824014635d2c9da35f84b7ab26633c24`。リンク時コード2,119,653 bytes / 3MiB、静的RAM90,412 bytes。
- [x] D2アプリのみ導入・保存域保持（2026-10-26）。アプリのハッシュ/読戻し一致、起動用領域・NVS・FATの前後不変。`docs/validation/r13-ui-app-20261026-133949/report.json`。
- [x] 実機のエラー分類を取得（2026-10-26）。`docs/validation/r13-fault-20261026-135037/report.json`の100.297秒にstage=5/status=0/reason=3（要求メモリ準備の失敗）。これは要求JSONのoverflowまたは送信用バッファ確保失敗を同じ文言にまとめているため、どちらかは未特定。原音・認識文・本文は取得していない。
- [ ] 要求メモリ準備の失敗の内訳と原因を特定。
- [ ] 原因修正・回帰検証・実際の音声回答確認。

本記録: `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/docs/R13_RESPONSE_DIAGNOSIS_2026-10-26.md`。

## その他の観測と現在の優先順位

- 先のD2起動観測`docs/validation/r13-fault-20261026-134308/report.json`には実際の再起動を記録。同じELFへの照合で`WiFiEventCbList`コピー / `WiFiGenericClass::_eventCallback` / `_arduino_event_task`（Arduino-ESP32 2.0.14のWiFiGeneric.cpp:746,1161,305）を確認。応答失敗の要求メモリとは別の未解決事象として扱う。
- `docs/validation/r13-fault-20261026-135037/report.json`では約1.3秒のRecording→Readyも反復。API送信へ進んでいない。利用者は質問を話しており、画面へ触れて中止していないと回答。録音終了判定の原因は未確定。
- 利用者が「まず表情をSleep/Ready/Listening/Thinking/Speaking等で分ける」と指定したため、音声処理の修正を保留しUI04へ切替。D2ログは終了して保存し、停止マーカー残存0件を確認。メモリ分類を得たことを音声回答の修正完了とは扱わない。

## D4の送受信修正と実機結果（2026-10-26）

実ArduinoJson 7.4.3の既定2バイト文字列長では、許可された音声長のBase64を要求JSONへ格納/応答JSONから取得できない。送信640,060文字と受信1,280,000文字の合成テストで既定値の失敗を再現し、全翻訳単位へ`ARDUINOJSON_STRING_LENGTH_SIZE=4`を指定して成功を確認した。未確定だったreason=3の経路に対応する修正。実機の全メモリ故障を解消したとは扱わない。

D4実機記録`docs/validation/r13-fault-20261026-145006/report.json`: STT/Reply/TTSが全てHTTP200、Speakingへ進み、その約4.9秒後にReadyへ戻った。一方で利用者は呼びかけなしの誤起動、回答の途中停止、Wi-Fi起動画面の再表示を報告。誤起動は`R13_WAKE_DIAGNOSIS_2026-10-26.md`のD5で対処する。
この600秒観測全体の起動判定は1起動・fault=falseだったが、詳細イベント上限200件のため後半は欠落する。再生完了/再起動問題の解決証明ではない。D5以降は上限1500件、超過数記録で観測する。

- [x] 音声JSON文字列長の失敗を再現し、設定修正で送受信上限のテストを通した（2026-10-26）。`firmware/tests/json_audio_limits.cpp`, `firmware/platformio.ini`。
- [x] 実機でSTT→Reply→TTSのHTTP200と再生モード進入を確認（2026-10-26）。上記D4記録。
- [ ] 意図した呼びかけだけでの開始、回答を最後まで再生、復帰後の再検出、再起動の有無を実機確認。
