# R13 無料枠候補モデル比較 v1（2026-10-25）

目的: 回答生成503を回避できる候補とGemini 3.8 Liveを、準備済みの同じキーで比較する。
対象: R13.1（0128f84）に対するPC側の追加検証。ファームウェア、ローカル設定、課金プランは変更していない。
後続の採用判断: 同日、利用者が3.5 Flash-Liteを選択。ソースへの反映は[R13.2採用記録](R13_2_MODEL_ADOPTION_2026-10-25.md)を参照。以下の表は採用前に測った比較結果である。
ユーザーが別モデル試験と3.8 Live追加を承認。固定の合成音声だけを使用し、実マイクや会話履歴は送信していない。

## 判断

現行の「録音→STT→回答文→TTS→音声全体受信後に再生」を維持する場合、回答生成を `gemini-3.5-flash-lite` にする案を第一候補とする。
STTは3.1 Flash-Lite、TTSは3.8 Flash-Lite TTSを維持する。3.1 Flash-Liteによる回答生成も成功しており、代替候補となる。
これは短い算数質問1種での機能確認に基づく暫定判断であり、一般的な回答品質・速度順位・連続運転の安定性を証明していない。

3.8 Liveも同じキーでテキスト入力・音声入力とも成功した。ただしLiveはWebSocketで音声を分割受信する方式で、既存RESTクライアントのモデル名だけを置換できない。
音声入力から回答音声を直接生成するため、承認済みの独立したSTT・回答生成・TTSとは処理の区切りが異なる。
音声を全部ためて再生する条件では今回Liveが最速とはならなかった。受信途中から再生する設計へ変更する場合には有力な比較対象である。
公式モデル資料では構造化出力も非対応であり、既存のemotion/text JSON契約はそのまま使えない。
入出力の文字起こしは取得できるが、後から出力される文字起こしを独立した回答文生成・TTS入力の代用とは扱わない。

公式料金表では3.5/3.1 Flash-Liteおよび3.8 Liveの入出力に無料枠がある。
今回の生成成功だけでは、このプロジェクトの実際の課金階層・無料枠残量・継続利用上限は確定できない。課金設定は操作していない。

## 共通の回答生成要求

固定質問「二足す三はいくつですか。」、日本語60文字以内、emotion/text JSON、出力上限2048、HTTP待機15秒。
世代をまたいで要求をそろえるため、この比較ではthinkingConfigを省略した。後述のSTT→TTS試験は本番同様thinking lowである。

| モデル | 実応答 | 経過時間 | 判定 |
|---|---|---|---|
| gemini-3.5-flash-lite | HTTP200、JSON・感情値・正答5を確認 | 再確認1.344秒 | 成功 |
| gemini-3.1-flash-lite | HTTP200、JSON・感情値・正答5を確認 | 0.928秒 | 成功 |
| gemini-3.8-flash | 15秒待機でTimeoutError | 15.155秒 | 失敗。先行診断の503を解消したとは扱わない |
| gemini-2.5-flash | HTTP404 / NOT_FOUND | 0.400秒 | このキーでは生成不可 |
| gemini-2.5-flash-lite | HTTP404 / NOT_FOUND | 0.327秒 | このキーでは生成不可 |

モデル一覧には5候補とも掲載されていた。一覧掲載は生成利用権限の証明にはならない。
2.5系が全利用者に利用不可という結論ではない。公式のアクセス制約と、このキーの404を区別する。
3.5の初回はHTTP200/1.668秒、非空JSONと感情値は確認したが正答判定falseだった。
回答本文を保存していないため、その初回の正誤は後から確定できない。数字・漢数字に加え仮名表記を許容する判定へ修正し、別の1要求で再確認した。初回JSONは書き換えていない。

## 音声入力から全音声受信まで

同じ2.8秒の検証済み合成質問を使用。STT/Live入力は16kHz、出力は24kHz/mono/16bit。
録音時間と実機転送・再生準備時間を含まないPC測定である。各経路1試行、回答長も異なるため時間差をモデルだけの差と解釈しない。

| 構成 | STT | 回答生成 | TTS | 接続等を含む合計 | 返信音声 |
|---|---:|---:|---:|---:|---|
| STT3.1 → 回答3.5 → TTS3.8 Lite | 1.602秒 | 1.868秒 | 2.099秒 | **5.582秒** | 4.48秒、221156 bytes |
| STT3.1 → 回答3.1 → TTS3.8 Lite | 1.855秒 | 2.477秒 | 2.895秒 | **7.242秒** | 5.12秒、251876 bytes |
| 3.8 Live 音声→音声 | 一体処理 | 一体処理 | 一体処理 | **7.314秒** | 5.00秒、240044 bytes（PCでWAV化） |

RESTの両経路は各段階HTTP200、入力の数値2/3、回答5、WAVを確認しpipeline_pass=true。
Live音声入力も入力文字起こしの2/3と出力文字起こしの5を確認した。音声内容を人が聴いて評価した結果ではない。

## 3.8 Liveの追加確認

| 入力 | 接続・setup完了 | 入力送信から最初の音声 | 入力送信からturnComplete | 接続を含む全体 | 判定 |
|---|---:|---:|---:|---:|---|
| 固定テキスト | 0.579秒 | 0.812秒 | 5.745秒 | 6.330秒 | 正答5、音声完了、割込みなし |
| 固定合成音声 | 0.582秒 | 1.734秒 | 6.730秒 | 7.314秒 | 入力認識・正答5、音声完了、割込みなし |

新規接続を各1回使用。常時接続・再接続・複数ターン・無音・取消・429/503時のLive回復は未検証。
録音済み音声の送信を模して、2.8秒分のPCMをまとめて送り、手動の発話開始/終了を通知した。実時間に合わせて少量ずつ送る試験ではない。
初回音声到着時間は再生開始時間ではない。今回の仕様ではturnCompleteまで全音声を保持する必要がある。
LiveはPCMを返すためPC側でWAVヘッダを付加した。REST TTSから返るWAVとは生成経路が異なる。
Liveの2ファイル（237164/240044 bytes）とRESTの2ファイルは、実際のfirmware/src/ConversationRules.hを呼ぶホスト検査で全件受理された。
これはCoreS3での音出し・RAM使用量・Wi-Fi挙動を確認したものではない。

## 成果物と再現

本記録の絶対パス: C:/Users/thoma/SynologyDrive/H/Doc/Src/Codex/projects/ai-stackchan/m5stack-cores3-gemini-stackchan/docs/R13_MODEL_COMPARISON_2026-10-25.md。
以下のパスは同案件ルートからの相対パス。JSONは状態・時間・件数などの許可した指標のみを保存し、キーやAPI本文は保存しない。

- 共通比較: docs/validation/model-comparison-20261025-222747/report.json
- 3.5正答再確認: docs/validation/model-comparison-20261025-223218/report.json
- 回答3.1の全経路: docs/validation/r13-api-20261025-223045/report.json
- 回答3.5の全経路: docs/validation/r13-api-20261025-223310/report.json
- Liveテキスト: docs/validation/live-20261025223946-702-text/report.json
- Live音声: docs/validation/live-20261025224157-326-audio/report.json
- 音声成果物: 各全経路ディレクトリのsynthetic-reply.wav、Liveディレクトリのsynthetic-live-reply.wav。

依存: REST試験はPython標準ライブラリ。Live試験はNode.jsと既存node_modulesのws 8.22.0を使用し、新規インストールなし。
再現時は実API要求と枠消費が発生する。キーはGit除外されたfirmware/data/config.jsonから読む。

```powershell
python -B tools/compare_reply_models.py
python -B tools/validate_r13_api.py --run --resume docs/validation/r13-api-20261025-210943/report.json --reply-model gemini-3.5-flash-lite --timeout 15
node tools/validate_gemini_live.mjs text
node tools/validate_gemini_live.mjs audio
./tools/check_api_wav.ps1 -WaveFile docs/validation/live-20261025224157-326-audio/synthetic-live-reply.wav
```

## ToDo

- [x] 5候補の同一キー・共通要求比較（2026-10-25）。上記比較JSONに実応答を保存。
- [x] 回答3.5/3.1のSTT→回答→TTS成功確認（2026-10-25）。上記全経路JSON、WAV、ファームウェアWAV規則で検証。
- [x] 3.8 Liveのテキスト/音声入力・回答音声・文字起こし・完了確認（2026-10-25）。上記Live JSON/WAVが成果物。
- [x] Python2ツール/Liveツールの構文、差分形式、追加成果物19ファイルのローカルキー非混入・JSON構文を確認（2026-10-25）。実キー入りconfig.jsonはGit除外・未追跡を確認。
- [x] 回答3.5 Flash-Liteの採用承認（2026-10-25）。ソース反映・検証はdocs/R13_2_MODEL_ADOPTION_2026-10-25.md。実機未導入。
- [ ] 実プロジェクトの無料枠残量・連続利用上限、代表的な日本語質問での品質・遅延分布。
- [ ] CoreS3保存領域互換と実機検証。従来のffat/ビルド領域不一致は未解決。

## 一次資料（2026-10-25確認）

- [料金と無料枠](https://ai.google.dev/gemini-api/docs/pricing)
- [モデルの終了予定・アクセス制約](https://ai.google.dev/gemini-api/docs/deprecations)
- [Gemini 3.8 Liveモデル仕様](https://ai.google.dev/gemini-api/docs/models/gemini-3.8-live)
- [Liveの音声形式・文字起こし・発話区間制御](https://ai.google.dev/gemini-api/docs/live-api/capabilities)
- [Live WebSocket API契約](https://ai.google.dev/api/live)
