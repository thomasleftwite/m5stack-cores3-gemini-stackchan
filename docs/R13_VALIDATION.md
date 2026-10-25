# R13 検証記録

日付: 2026-10-25。対象: CoreS3/CoreS3 Lite、仕様: [VOICE_PIPELINE_R13.md](VOICE_PIPELINE_R13.md)。
正本は `firmware/`。ルートのsrc/platformio.iniとWeb配布は `tools/sync_firmware_sources.mjs` で同期する。

## 許可範囲

ソース実装、オフラインテスト、ビルド、型検査、資料整備。
GeminiへのAPI呼出、マイクによる実録音、実機書込みは行っていない。

## 実装

- LocalWake: 16kHzの20msフレームから12帯域の対数スペクトル特徴量を作り、DTWで登録3回のうち2回以上と一致した場合に検出。ニューラルSTTではなく登録者依存のプロトタイプ。閾値は未校正。
- 更新: CRC付き2スロット。非アクティブ側へ一時保存・読戻し検証して公開する。少なくとも1つの旧確定データを残す。両スロットが不正なら未登録表示。
- I2S: メインループがマイク・スピーカーを排他使用。非同期録音完了を確認してからフレームを参照。スピーカーの非同期キューには寿命が継続する4面バッファを使う。
- Gemini: 無料枠の3モデルへHTTPSで別々に送信。成功済みSTT/回答はRAMへ保持し、エラー画面から失敗段階のみ手動再試行。30秒でエラー画面を閉じて一時データを破棄。
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
- [x] 実LocalWake.cppをメモリ上のFSに接続したオフラインテスト: 3回確定、異なる登録の拒否と同じ回の再試行、更新中止、再読込、部分書込み、rename失敗、CRC破損時の旧スロット復帰、全スロット不正。
- [ ] 最終変更後のPlatformIOビルド・配布整合・Web型検査。
- [ ] APIの受理、無料枠の実上限、STT精度、TTSスキーマ/声質/日本語、ネットワーク取消・再試行（未実施）。
- [ ] 実機マイク・照合閾値・誤検出・3回登録UI・再起動/電源断・sleep/touch・音切れ・口・サーボ（未実施）。
- [ ] 総待ち時間10秒以内の目標（未測定、無料枠では保証なし）。

合成波形とメモリFSでの成功は、実マイクや実フラッシュの品質・永続化の証明ではない。
Googleルート証明書は https://pki.goog/roots.pem から2026-10-25取得。SHA256: EC989DF46C8F4419EF2EE2517CAD7619D555E4973F3307BE697662AA2497E480。
