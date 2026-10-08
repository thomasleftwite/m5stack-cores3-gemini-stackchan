# WiFiManager移行 R01 検証記録

日付: 2026-10-09（日本時間）
目的: SSID・パスワードをconfig.jsonで配布する方式から、CoreS3 Liteの設定APへスマホで接続して登録する方式へ移行する。
対象: M5Stack CoreS3 Lite / ESP32-S3 / Arduino / PlatformIO m5stack-cores3。
依存関係: espressif32@6.5.0、tzapu/WiFiManager@2.0.17。その他はplatformio.ini参照。

## 作業状態

- [x] 実装・手順更新（2026-10-09）: src/main.cpp、platformio.ini、README.md、data/config.example.json。Web側はapp/page.tsx、components/SettingsPanel.tsx、components/FirmwareCodeStudio.tsx、lib/firmware-sources.ts。
- [x] 静的確認（2026-10-09）: ZIP用README・main.cpp・ConfigManager.h・platformio.iniの実機側ファイルとの一致、設定サンプルと生成JSONの検査。WebのWiFi入力・設定JSON内WiFi項目の除去。WiFi設定が周辺機器初期化に先行すること。ファイル操作診断物残存0件。
- [x] ファームウェアのコンパイル成功（2026-10-09）: PlatformIO Core 6.2.0、Arduino-ESP32 2.0.14、WiFiManager 2.0.17で終了コード0。RAM 53,904 / 327,680 bytes、Flash 1,308,853 / 6,553,600 bytes。成果物: .pio/build/m5stack-cores3/firmware.bin、firmware.elf。初回の取得エラーと案件内キャッシュのパス長制限は、ネットワーク取得の再実行と短い一時キャッシュへ切替えて解消した。
- [x] Webの型検査・ビルド成功（2026-10-09）: TypeScript 5.9.3で変更4ファイルの構文検査と全体のtsc --noEmit --incremental falseが成功。Next.js 15.5.27のnpm run buildも終了コード0。lintは既存設定でスキップされ、Gemini API呼出しは未検証。ビルド時のAPIキー未設定警告は想定どおり。
- [x] ターゲットへのフラッシュ・AP起動確認（2026-10-09）: COM10のESP32-S3 revision v0.2へPlatformIO uploadを実行。書込み・ハッシュ検証・RTSによる再起動が成功（終了コード0）。uploadfsは実行していない。再起動後のシリアル読取り8秒間で起動バナーと `[WiFi] Setup AP active.` を確認し、panicやboot-loopマーカーは観測しなかった。生ログや接続情報は保存していない。
- [ ] スマホ設定・接続後の実機検証: スマホの設定画面操作、接続先変更、サーボ等は未確認。R02で再起動後に保存済みWiFiへ接続し、Gemini向けTLS接続に至ることを確認。

ビルド時の解決済みライブラリ: M5Unified 0.1.17、M5GFX 0.2.32、ArduinoJson 7.4.3、WiFiManager 2.0.17（esp32-camera 2.0.4も取得）。既存依存のバージョン範囲は維持しているため、後日のビルドではWiFiManager以外の解決版が変わり得る。今回のツールキャッシュは `$env:TEMP\stackchan-pio-r01`。ライブラリ/Arduino本体の非推奨・戻り値警告は残るが、アプリのコンパイルとリンクは成功した。

## 再現手順

実機用firmwareフォルダ（ZIPでは展開先ルート）で、PlatformIOを利用して以下を実行する。

```powershell
pio run -e m5stack-cores3
```

Webの依存導入・検査はリポジトリルートで実行する。

```powershell
npm install
npx tsc --noEmit
npm run build
```

成果物の絶対パス:

- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/.pio/build/m5stack-cores3/firmware.bin`
- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/src/main.cpp`
- `C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/lib/firmware-sources.ts`

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

TLSメモリ確保失敗の修正でPSRAMモードを `qio_qspi` へ変更し、PSRAM約8MBの認識と実機TLS接続成功を確認した。最新の原因・測定・最終フラッシュ結果は `TLS_MEMORY_DIAGNOSTIC.md` を参照する。R01のサイズ値はその時点の値。
