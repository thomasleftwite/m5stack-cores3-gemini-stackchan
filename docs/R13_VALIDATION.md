# R13 検証記録

日付: 2026-10-25。対象: CoreS3/CoreS3 Lite、仕様: [VOICE_PIPELINE_R13.md](VOICE_PIPELINE_R13.md)。
正本は `firmware/`。ルートのsrc/platformio.iniとWeb配布は `tools/sync_firmware_sources.mjs` で同期する。

## 許可範囲

ソース実装、オフラインテスト、ビルド、型検査、資料整備。
初期実装時はAPI呼出・マイク実録音・実機書込みを行っていない。
その後、2026-10-25にホストAPI検証とUSB調査の追加承認を得た。経過は案件ルートのdocs/R13_API_VALIDATION_2026-10-25.mdに記録。実機書込みは未実施。

## 実装

- LocalWake: 16kHzの20msフレームから12帯域の対数スペクトル特徴量を作り、DTWで登録3回のうち2回以上と一致した場合に検出。ニューラルSTTではなく登録者依存のプロトタイプ。閾値は未校正。
- 更新: CRC付き2スロット。非アクティブ側へ一時保存・読戻し検証して公開する。少なくとも1つの旧確定データを残す。両スロットが不正なら未登録表示。
- I2S: メインループがマイク・スピーカーを排他使用。M5Unified 0.1.17の2要求キューに3つの寿命が継続する録音バッファを順に渡し、3件目の投入が待つことを完了バリアとして最古フレームを読む。isRecording()の起動前競合を避ける。スピーカーの非同期キューには4面バッファを使う。
- Gemini: 無料枠の3モデルへHTTPSで別々に送信。成功済みSTT/回答はRAMへ保持し、エラー画面から失敗段階のみ手動再試行。Retry-After等の待機時間+30秒まで保持して終了する。通常エラーは約30秒、429は最低60秒の待機+30秒。認識不能は聞き直しを表示しreadyへ戻る。
- HTTP: 証明書・時刻・HTTP状態・Content-Type・受信長・JSON・finishReason・Base64・WAVチャンク・音声長を検証。チャンク受信中に再生しない。
- キャンセル: UIはネットワークとは別タスク。進行中HTTPの待機終了後に中止が完了する。既に送ったリクエストの取消や課金取消は保証しない。後段API/再生は停止。
- 返信上限20秒。無音・認識不能はクラウド回答へ進めない。会話履歴は直近3往復、sleepで消去。
- WiFi/NVS、ローカルのAPIキー入りconfig.jsonは書換えない。LittleFSのマウント失敗時に自動フォーマットしない。
- 既存ブラウザの対話・カメラ・設定シミュレーターは旧方式の参考デモと明示。配布ZIPのソースはR13。キー入りconfig.jsonは配布せず、空欄のconfig.example.jsonのみ。

## 再現手順

```powershell
# リポジトリルート、Visual C++ Build Tools使用
./firmware/tools/run_offline_tests.ps1
node tools/sync_firmware_sources.mjs
node node_modules/typescript/bin/tsc --noEmit --incremental false
# firmware/で実行。実機へは書き込まない
pio run -e m5stack-cores3
```

## 検証結果

- [x] オフラインC++テスト: 録音の開始待ち/無音終了/15秒/時計ラップ、WAV切断・フォーマット・追加チャンク・過大長、CRC、DTW、無音・短音・3秒超の登録拒否。
- [x] 遅延開始するマイクキューのスタブ: 最古バッファの完了前に参照しないこと、連続12フレームの順序、停止後リセット、投入失敗を確認。
- [x] 実LocalWake.cppをメモリ上のFSに接続したオフラインテスト: 3回確定、異なる登録の拒否と同じ回の再試行、更新中止、再読込、部分書込み、rename失敗、CRC破損時の旧スロット復帰、全スロット不正。
- [x] 最終変更後のPlatformIOビルド成功（2026-10-25）。Arduino-ESP32 2.0.14、M5Unified 0.1.17、M5GFX 0.2.32、ArduinoJson 7.4.3、WiFiManager 2.0.17。
- [x] 配布37ファイルと正本の一致、ルートソースの同期、Web TypeScript型検査（tsc --noEmit --incremental false）成功（2026-10-25）。
- [x] 実際のローカルAPIキーとの一致検査で、変更した公開成果物内の一致0件。firmware/data/config.jsonはGit除外・未追跡を維持。既存ルートdata/config.jsonは追跡済みのプレースホルダーで、変更していない。
- [ ] API統合検証。2026-10-25にTTS日本語WAVとSTTをホストで個別確認。回答生成は503/タイムアウトで未完了。実際の無料枠上限・実機認識精度・ネットワーク取消は未確認。
- [ ] 実機マイク・照合閾値・誤検出・3回登録UI・再起動/電源断・sleep/touch・音切れ・口・サーボ（未実施）。
- [ ] 総待ち時間10秒以内の目標（未測定、無料枠では保証なし）。
- [ ] マイクタスク自体が停止した故障からの回復（未対応・未検証）。M5Unified 0.1.17のrecord()内部FIFO待ちは期限がなく、この故障時はUIのタッチ処理へ戻れない可能性がある。通常のrecord()失敗はエラー表示・再準備で扱う。

## レビュー

2026-10-25、code-review Skillの仕様/コード品質の2軸でレビュー。
仕様側2件（タッチ復帰直後の時刻、429の待機より短いキャッシュ保持）、品質側2件（非同期録音の開始競合、マイク失敗の黙殺）を修正し、再確認で解消。
さらにマイクエラー時の質問録音破棄と、STT認識不能時の聞き直しを追加。依存マイクタスク停止からの回復は上記の制約として残す。

### Standards

- 非同期録音の開始前にisRecording()が0を返す競合: 3面バッファとFIFO完了バリアに変更し、遅延開始スタブとソース再確認で解消。
- ready/登録中に録音失敗を黙殺する経路: マイクエラー画面と明示的な再準備を実装し、再確認で解消。登録済みデータ・受理済み登録回を保持。
- 残る制約: 依存マイクタスク自体の停止からの復旧は未対応・未検証。最初の品質指摘2件は修正済み。

### Spec

- タッチ復帰直後の古いnowが符号なしアンダーフローし再sleepする問題: タッチ処理後に時刻を再取得し、再確認で解消。
- 429で再試行可能になる前に成功済み段階を破棄する問題: 保持期限を待機時間+30秒へ変更し、再確認で解消。期限のオフラインテストも追加。
- 追加の録音破棄漏れはマイクエラー進入時のclearRecordingで修正。最初の仕様指摘2件は修正済み。

## 成果物

- 作業ブランチ: codex/cores3-local-wake-file-conversation（ローカルコミットのみ、公開なし）。
- 正本: C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/src/
- ビルド: C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/firmware/.pio/build/m5stack-cores3/firmware.bin
- バイナリサイズ: 2118336 bytes。SHA256: C6AEE741FAD996289A45D582D898931A9320AFE39F36CF2A7BA10555A2A70202。
- リンク時静的RAM: 90272 / 327680 bytes（27.5%）。PSRAMの実行時ピークは未測定。
- Arduino-ESP32既存ライブラリのuartSetPins警告は残るが、ビルドは成功。
- 起点から存在するREADMEのNode追記、package.json、bun.lock、.nvmrcは今回のコミットに含めず保持。

合成波形とメモリFSでの成功は、実マイクや実フラッシュの品質・永続化の証明ではない。
Googleルート証明書は https://pki.goog/roots.pem から2026-10-25取得。SHA256: EC989DF46C8F4419EF2EE2517CAD7619D555E4973F3307BE697662AA2497E480。
