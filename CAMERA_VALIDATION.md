# カメラI2C初期化失敗の修正 R04

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

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path $env:TEMP 'stackchan-pio-r01'
& 'C:\Users\thoma\.platformio\penv\Scripts\platformio.exe' run -e m5stack-cores3 -t upload --upload-port COM10
if ($LASTEXITCODE -eq 0) { & 'C:\Users\thoma\.platformio\penv\Scripts\python.exe' tools\observe_camera.py --port COM10 --seconds 25 }
```

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
