# TLSメモリ確保失敗の修正 R02

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

```powershell
& 'C:\Users\thoma\.platformio\penv\Scripts\python.exe' firmware\tools\observe_tls.py --port COM10 --seconds 45
```

TLS_ALLOCATION_FAILEDは失敗、TLS_CONNECTEDはTLS成功マーカーまたはStream completeを検出した状態。NO_TLS_ATTEMPT_OBSERVEDは判定不能、PORT_UNAVAILABLEなら別のシリアルモニターを閉じる。TLS成功はGemini応答の内容検証を意味しない。

## 作業状態

- [x] 再現用観測ツール作成・同じ実機TLS失敗を再現（2026-10-09）。
- [x] 内部RAM・最大連続空き領域・PSRAMを実測（2026-10-09）。
- [x] メモリモード修正、コンパイル・フラッシュ・ハッシュ検証成功（2026-10-09）。
- [x] 診断版で同じ操作のTLS接続成功を確認（2026-10-09）。
- [x] 一時測定ログとMemoryDiagnostics.hを除去（2026-10-09）。通常の[Gemini] TLS connectedマーカーのみ維持。
- [x] 測定用コードを除いた最終版のコンパイル・フラッシュ・ハッシュ検証成功（2026-10-09）。同じ45秒観測ループでTLS_CONNECTED（終了コード0）。TLS接続成功を2回観測し、TLS_ALLOCATION_FAILEDは観測しなかった。

成果物の絶対パス:

- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/platformio.ini`
- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/src/GeminiClient.cpp`
- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/tools/observe_tls.py`
- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/.pio/build/m5stack-cores3/firmware.bin`

再発予防: ボードやメモリモード変更時は、マクロ定義だけでPSRAM有効と扱わず、実機のPSRAM容量とTLS接続を確認する。初期化後のRAM不足はホスト上のコンパイルだけでは判定できない。
