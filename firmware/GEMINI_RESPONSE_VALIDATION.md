# Gemini空応答の診断・修正 R03

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

```powershell
& 'C:\Users\thoma\.platformio\penv\Scripts\python.exe' tools\observe_gemini.py --port COM10 --seconds 60
```

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
