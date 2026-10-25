# 3.5 Flash-Lite採用 R13.2（2026-10-25）

目的: 利用者が選定した3.5 Flash-Liteを回答生成へ反映し、3.8 Flashの503を避ける構成を用意する。
対象: M5Stack CoreS3/CoreS3 Lite、ESP32-S3、Arduino / PlatformIO。起点0128f84。
承認済みR13仕様とR13.1再試行を維持し、回答モデルだけを変更する。PC側Webの回答モデル・初期値・配布サンプルも一致させる。

## 変更と設定

- firmware/src/GeminiModels.hにSTT/回答/TTSモデルを集約。3.1 Flash-Lite / **3.5 Flash-Lite** / 3.8 Flash-Lite TTS。
- GeminiClientは上記モデルを使用する。使用されていなかったモデル引数を削除し、起動表示をR13.2へ変更。
- ConfigManagerはgemini_modelを互換用として残すが、読込み時は採用モデルをRAM上で使用する。旧設定が3.8等でも3.5を使い、設定ファイルを自動更新しない。
- Webはlib/gemini-models.tsの回答モデルを初期値・初期値復帰・設定表示・配布サンプル・chat/stream APIから使用。根拠のない固定日次上限と旧モデル選択肢を除去。
- tools/validate_r13_api.pyの既定の回答モデルも3.5に変更。旧モデルの診断・比較ツールと過去の失敗記録は保持。
- APIキー入りローカル設定、NVS、登録パターン、課金プラン、パーティション表は変更しない。共有配布はキーなし。

## 依存関係と確認手順

firmware/platformio.ini: espressif32@6.5.0 / board esp32s3box、M5Unified0.1.17、M5GFX0.2.32、ArduinoJson7.4.3、WiFiManager2.0.17、esp32-camera2.0.4。
変更前と同じ依存・電源条件を使用する。PC USBのみ、Grove/外部電源を外した状態の確認は同日のAPI検証記録を参照。
実API確認は固定の合成質問を再使用し、新たな実マイク録音を行わない。

```powershell
./firmware/tools/run_offline_tests.ps1
node tools/sync_firmware_sources.mjs
node node_modules/typescript/bin/tsc --noEmit --incremental false
node tools/test_live_config_errors.mjs
# firmware/で実行。uploadは付けない
platformio run -e m5stack-cores3
# 案件ルートで実行。既定モデル3.5、各段階の待機15秒
python -B tools/validate_r13_api.py --run --resume docs/validation/r13-api-20261025-210943/report.json --timeout 15
```

## 実機導入の条件

後続の調査・設計決定（2026-10-25）: 本体表は標準app3M_fat9M_16MB.csvと完全一致。アプリ3MiB×2/FAT9.875MiBの既存配置を選定した。成果物はdocs/STORAGE_LAYOUT_DIAGNOSIS_2026-10-25.mdとdesign/cores3_16mb_app3m_fat9m.csv。FAT互換実装と実機検証は引き続き未実施。

COM10の公開パーティション表はapp0=0x10000/0x300000、ffat=0x610000/0x9e0000であり、ビルドのdefault_16MB.csvと一致しない。
現行はLittleFSのみを使用する。ラベルだけでは既存データのファイルシステムを確定できないため、無条件のFFatマウント/フォーマットへの置換は行わない。
既存の設定・登録データを保持する互換対応と書込み範囲を用意してから実機へ導入する。通常uploadはパーティション表を含むため、現時点では実施しない。
公開パーティション表以外のユーザーデータ読出しや保存も行っていない。

## ToDoと検証結果

- [x] モデル選定承認とソース反映（2026-10-25）。成果物: firmware/src/GeminiModels.h、GeminiClient.cpp、ConfigManager.h、lib/gemini-models.ts等。
- [x] オフラインテスト・型検査・PlatformIOビルド成功（2026-10-25）。録音/WAV/照合・3回登録保存・503上限/取消の既存全テスト成功。配布40ファイルと正本一致。
- [x] 採用後の既定値によるSTT→回答→TTS実API確認（2026-10-25）。成果物: docs/validation/r13-api-20261025-225254/report.jsonとsynthetic-reply.wav。
- [x] 差分の仕様・規約レビュー（2026-10-25）。表示1件・検証ツールの例外処理1件を修正、両側で再確認済み。以下に記録。
- [ ] 既存保存領域を保持した実機導入、実機会話・ウェイク・sleep/touch確認。

本記録: C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/docs/R13_2_MODEL_ADOPTION_2026-10-25.md。
先行比較と実測値はR13_MODEL_COMPARISON_2026-10-25.md。Google一次資料: https://ai.google.dev/gemini-api/docs/models/gemini-3.5-flash-lite 、 https://ai.google.dev/gemini-api/docs/pricing 。

### 採用後のPC API再確認

既定値で3.5を使用。合成入力の出所はSHA256で確認し、試験入力TTSの再生成なしで3要求を送信した。

| 段階 | 実応答 | 経過時間 |
|---|---|---:|
| STT / 3.1 Flash-Lite | HTTP200、質問の数値2/3を認識 | 9.852秒 |
| 回答 / 3.5 Flash-Lite | HTTP200、非空JSON、日本語18文字、正答5 | 1.011秒 |
| TTS / 3.8 Flash-Lite TTS | HTTP200、WAV24kHz/mono/16bit、3.44秒 | 2.521秒 |
| 合計 | pipeline_pass=true | **13.399秒** |

返信WAV171236 bytes、PCM開始44/165120 bytesで、実際のConversationRules.hを使うホスト検査も受理した。
先行比較の5.582秒からSTT待ちが増加しており、10秒以内を保証できない。録音時間・実機転送/再生準備を含まない。
実機への書込み・マイク録音・音出し・口/サーボ動作は未実施。Web側は型検査までで、ブラウザ実操作・Web経由のAPI呼出は未検証。

### ビルド成果物と監査

成果物: C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/.pio/build/m5stack-cores3/firmware.bin。
最終ビルド成功、2119424 bytes、SHA256 `4f35d8c11629eb82d1e4d932cbff9eaf333b78910bb918c91be95120dd2b21e6`。
リンク時静的RAM90272 / 327680 bytes（27.5%）。PSRAM実機ピークは未測定。
依存ライブラリ内の既存警告が出るが、今回のソース変更にコンパイルエラーはない。
Python2ツール・Liveツールの構文検査、差分形式、追跡/未除外成果物148ファイルのローカルキー一致0件、実設定のGit除外・未追跡を確認。
既存ユーザー変更（READMEのNode.js要件、package.json、bun.lock、.nvmrc）は保持し、今回のコミットに含めない。

## Spec

レビュー範囲: `git diff 0128f84...3ab0b60` と後続修正。
Web上部の回答モデル表示が旧3.8のままというP3指摘1件。app/page.tsxの表示をREPLY_MODELへ変更し、型検査・仕様側再確認で解消。
STT/回答/TTS、設定非書換え、503制御と実機未検証の区別に機能上の指摘なし。

## Standards

Live検証ツールの初期化時に、不正な設定JSONの入力断片を未捕捉例外が出す可能性があるP2指摘1件。
ローカル初期化を固定エラーのみを出す例外境界に含め、キー欠落・型不正も接続前に拒否した。実際のキーの漏えいを検出したものではない。
ダミー値だけで変更前の漏えい経路を再現し、変更後は不正JSON/読取り失敗/キー欠落/型不正の4件で、stderr空・固定JSONのみ・終了1を確認。
回帰検査tools/test_live_config_errors.mjsは設定読取りをメモリ内で差し替え、ネットワーク/書込みを禁止する。実configやAPIへアクセスしない。
品質側再確認で指摘解消、新たな具体的不具合なし。規約側1件・仕様側1件は解消済みで、両側とも未解決0件。
