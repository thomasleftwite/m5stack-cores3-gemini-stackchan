# デバッグ引継ぎ（2026-10-09 / R08）

## 対象と現在の状態

M5Stack CoreS3 Lite（ESP32-S3、8MB Quad SPI PSRAM）用Arduino/PlatformIOファームウェアと、Next.jsの設定・配布ZIP生成アプリ。目的はWiFiセットアップとGeminiによる音声会話。現在はテキストAPI通信と表情・サーボ制御の検証段階で、音声会話は未完成。

- ビルド対象: `firmware/platformio.ini` の `m5stack-cores3`。
- 現在の書込み: R07サーボ再有効化版。`STACKCHAN_SERVO_OUTPUT_DISABLED=0`。
- 最新の実機構成: サーボとGroveを外し、本体をPC USBへ接続してCOM10で監視。利用者は画面消灯なしを確認。
- APIキー入りの `firmware/data/config.json` はローカル専用・Git除外。再開時も内容を出力・共有しない。

## 経過と確認範囲

| 版 | 問題・変更 | 検証結果 |
| --- | --- | --- |
| R01 | JSONのSSID/passwordからWiFiManagerへ移行。接続失敗時にAP、スマホブラウザで設定 | AP名 `StackChan-Setup`、設定画面 `192.168.4.1`。WiFi資格情報はNVSに保存。既存ローカル設定を保持 |
| R02 | TLSのメモリ確保失敗。PSRAM設定を `qio_opi` から `qio_qspi` へ変更 | PSRAM約8MBを認識し、TLS接続成功。変更前はPSRAM未認識・内部RAM不足 |
| R03 | HTTPエラーを空の成功応答として扱っていた。HTTPClientでHTTP/MIME、chunked転送、SSE、非空テキストを確認 | キー未転送の403を確認し、設定転送後にHTTP200・非空応答。503も観測。キーはHTTPヘッダーで渡し、本文をログへ出さない |
| R04 | カメラSCCBのI2Cドライバ重複初期化 | M5Unifiedの既存I2Cポートを使用。GC0308の160×120グレースケール初回フレームを実機確認 |
| R05/R06 | SG90接続時に本体消灯・USB切断。停止版では通信完走。外部給電申告・SW1 OFFでも再発 | 電源問題が最有力だが、過渡電圧・電流を未測定。ソフトウェア要因の完全除外はしていない |
| R07 | TAKAO v1.2の給電経路を確認。PC USBを外し、外部電源単独・SW1 ONへ変更してサーボ再有効化 | 5V最大2.4A電源でアニメ・サーボ正常、消灯なし、静止後にタップ復帰を利用者が2回確認。長時間安定性は未検証 |
| R08 | サーボ/Groveを外してPC USBでデバッグ | COM10を60秒監視。要求3回、503×1、429×2、応答完了0。切断・パニック・ブラウンアウトのマーカーなし。利用者も消灯なしを確認 |

## 給電についての訂正と運用

TAKAO v1.2のPCBデータではGrove CN2 pin3とサーボJ1/J2 pin2が同じ+5Vネット。SW1は外部USB入力を接続/切断する。以前の「本体への給電をOFFにすればサーボだけ外部給電」の案内は前提確認が不足していた。OFFでも本体→Grove→サーボの給電は残り得る。

- サーボあり・通常4線Groveでの試験: PC USBを外して、TAKAO外部電源のみ・SW1 ON。
- USB書込み・監視: 外部電源OFF、Groveを本体から外し、本体をPC USBへ接続。
- 本体PC USBとサーボ外部給電を分離する場合: Groveの5Vだけを分離し、GNDと信号2本を維持する必要がある。分離ケーブルは利用者の手元にない。
- 配線変更は電源OFF。再発時はサーボ停止フラグ1で再ビルド・書込みして復旧する。

[一次PCBデータ](https://github.com/akita11/Stack-chan_Takao_Base/blob/main/Stack-chan_Takao_Base_v12.kicad_pcb)。写真・リンクとの照合は静的確認であり、実物の導通・過渡波形測定ではない。

## 次に取り組む項目

- [ ] 429の詳細原因・クォータ種別を秘密情報なしで確認。503/429に対する要求間隔・上限付き再試行を検討。
- [ ] VADの発話開始判定と再要求条件を確認。音を検知しないままタップ後に固定文送信する経路がある。
- [ ] 実録音の蓄積・音声認識/音声入力送信を実装。現在はマイクRMS/VADのみで、Gemini入力は固定文。
- [ ] TTSまたは音声応答を実装。GeminiClientはテキストしか処理せず、onAudioを呼ばない。
- [ ] 音声再生・録音の競合、長時間安定、カメラ動体復帰、サーボ負荷を実機検証。

待機時のまばたき・ランダムサーボ動作停止とタップ復帰はコードと整合する。これはESPの電源断やdeep sleepではなくアプリ状態の待機。サーボPWMは待機中も継続。約30秒の観察時間を固定タイムアウト値とは扱わない。

## 再開手順・依存関係

1. 上記USB監視用の接続にする。IDEのシリアルモニターとCodexの監視を同時に開かない。
2. `firmware/tools/observe_disconnect.py --seconds 60` または `observe_gemini.py --port COM10 --seconds 60` で読み取り監視する。キー、WiFi情報、応答本文は記録しない。
3. 通常のコード更新は `platformio run -e m5stack-cores3 -t upload --upload-port COM10`。`uploadfs` は設定全体を書き換えるため、コード変更だけなら実行しない。
4. 配布ZIP用 `lib/firmware-sources.ts` と実ファイルを同期する。Web検査は `node node_modules/typescript/bin/tsc --noEmit --incremental false`。

実ビルド依存: espressif32 6.5.0、Arduino-ESP32 2.0.14、M5Unified 0.1.17、M5GFX 0.2.32、ArduinoJson 7.4.3、WiFiManager 2.0.17。設定上の一部ライブラリは範囲指定なので再解決時には版を確認する。Windowsの長いパス回避にPlatformIO coreを一時フォルダ `stackchan-pio-r01` へ置いている。

詳細記録: `firmware/WIFI_SETUP_VALIDATION.md`、`TLS_MEMORY_DIAGNOSTIC.md`、`GEMINI_RESPONSE_VALIDATION.md`、`CAMERA_VALIDATION.md`、`POWER_VALIDATION.md`。R07ビルド・書込み・フラッシュハッシュ検証は成功。最新Web型検査と配布ソース内容一致も成功。R08ではコード更新・再書込みなし。
