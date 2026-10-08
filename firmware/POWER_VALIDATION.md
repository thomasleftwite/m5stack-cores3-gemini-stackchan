# タッチ後の電源断の切り分けと暫定サーボ停止 R05

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

```powershell
& 'C:\Users\thoma\.platformio\penv\Scripts\python.exe' tools\observe_disconnect.py --seconds 60
```

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
