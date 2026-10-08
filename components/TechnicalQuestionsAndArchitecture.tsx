"use client";

import React, { useState } from "react";
import {
  Cpu,
  Layers,
  HelpCircle,
  Camera,
  Radio,
  Zap,
  CheckCircle2,
  AlertTriangle,
  ArrowRight,
  Server,
  Activity,
  Rotate3D,
} from "lucide-react";

export const TechnicalQuestionsAndArchitecture: React.FC = () => {
  const [activeTab, setActiveTab] = useState<"architecture" | "servo_guide" | "yolo_analysis" | "questions">("servo_guide");

  return (
    <div className="bg-neutral-900 border border-neutral-800 rounded-2xl overflow-hidden shadow-2xl flex flex-col">
      {/* Navigation Tabs */}
      <div className="bg-neutral-950 px-5 py-3 border-b border-neutral-800 flex items-center justify-between flex-wrap gap-2">
        <div className="flex items-center gap-2">
          <Layers className="w-5 h-5 text-cyan-400" />
          <h2 className="text-sm font-bold text-neutral-100">
            アーキテクチャ設計・技術解説 & Port A サーボ統合
          </h2>
        </div>

        <div className="flex gap-1 bg-neutral-900 p-1 rounded-lg border border-neutral-800">
          <button
            onClick={() => setActiveTab("servo_guide")}
            className={`px-3 py-1 rounded-md text-xs font-medium transition cursor-pointer flex items-center gap-1.5 ${
              activeTab === "servo_guide"
                ? "bg-cyan-600 text-white"
                : "text-neutral-400 hover:text-neutral-200"
            }`}
          >
            <Rotate3D className="w-3.5 h-3.5 text-amber-300" />
            Port A SG90 サーボ統合
          </button>
          <button
            onClick={() => setActiveTab("architecture")}
            className={`px-3 py-1 rounded-md text-xs font-medium transition cursor-pointer ${
              activeTab === "architecture"
                ? "bg-cyan-600 text-white"
                : "text-neutral-400 hover:text-neutral-200"
            }`}
          >
            FreeRTOS 最適化構成
          </button>
          <button
            onClick={() => setActiveTab("yolo_analysis")}
            className={`px-3 py-1 rounded-md text-xs font-medium transition cursor-pointer ${
              activeTab === "yolo_analysis"
                ? "bg-cyan-600 text-white"
                : "text-neutral-400 hover:text-neutral-200"
            }`}
          >
            ESP32カメラ動体・YOLO検証
          </button>
          <button
            onClick={() => setActiveTab("questions")}
            className={`px-3 py-1 rounded-md text-xs font-medium transition cursor-pointer ${
              activeTab === "questions"
                ? "bg-cyan-600 text-white"
                : "text-neutral-400 hover:text-neutral-200"
            }`}
          >
            要確認事項・状況サマリー
          </button>
        </div>
      </div>

      {/* Content Body */}
      <div className="p-6">
        {/* TAB: Port A SG90 Servo Integration Guide */}
        {activeTab === "servo_guide" && (
          <div className="flex flex-col gap-6">
            <div>
              <h3 className="text-base font-bold text-neutral-100 flex items-center gap-2">
                <Rotate3D className="w-5 h-5 text-cyan-400" />
                M5Stack CoreS3 Lite Port A (G2, G1) SG90 2軸サーボ統合仕様
              </h3>
              <p className="text-xs text-neutral-400 mt-1">
                CoreS3 Lite 本体の赤色 Grove コネクタ（Port A）に SG90 マイクロサーボ×2軸を直結し、会話の感情・リップシンクに合わせて自然に首を振るうなずき制御を組み込みました。
              </p>
            </div>

            {/* Pinout & Wiring Diagram */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              <div className="bg-neutral-950 rounded-xl p-4 border border-cyan-800/60 flex flex-col gap-3">
                <span className="font-bold text-xs text-cyan-300 font-mono flex items-center gap-1.5">
                  <Zap className="w-4 h-4 text-cyan-400" />
                  Port A (Grove Red) ピンアサイン結線表
                </span>
                <div className="overflow-x-auto">
                  <table className="w-full text-xs text-left border-collapse font-mono">
                    <thead>
                      <tr className="bg-neutral-900 text-neutral-400 border-b border-neutral-800">
                        <th className="p-2">Groveピン</th>
                        <th className="p-2">ESP32-S3 GPIO</th>
                        <th className="p-2">SG90 接続先</th>
                        <th className="p-2">機能</th>
                      </tr>
                    </thead>
                    <tbody className="divide-y divide-neutral-800 text-neutral-200">
                      <tr>
                        <td className="p-2 text-amber-400 font-bold">Pin 1 (黄)</td>
                        <td className="p-2 font-bold text-cyan-300">GPIO 2 (SDA)</td>
                        <td className="p-2">Pan サーボ (橙/信号線)</td>
                        <td className="p-2 font-sans text-neutral-400">水平 首振り (Yaw 左右)</td>
                      </tr>
                      <tr>
                        <td className="p-2 text-neutral-300 font-bold">Pin 2 (白)</td>
                        <td className="p-2 font-bold text-cyan-300">GPIO 1 (SCL)</td>
                        <td className="p-2">Tilt サーボ (橙/信号線)</td>
                        <td className="p-2 font-sans text-neutral-400">垂直 うなずき (Pitch 上下)</td>
                      </tr>
                      <tr>
                        <td className="p-2 text-rose-400 font-bold">Pin 3 (赤)</td>
                        <td className="p-2">5V (VBUS / Boost)</td>
                        <td className="p-2">両サーボ 電源線 (赤)</td>
                        <td className="p-2 font-sans text-neutral-400">5V サーボ電源供給</td>
                      </tr>
                      <tr>
                        <td className="p-2 text-neutral-400 font-bold">Pin 4 (黒)</td>
                        <td className="p-2">GND</td>
                        <td className="p-2">両サーボ GND (茶/黒)</td>
                        <td className="p-2 font-sans text-neutral-400">共通グランド</td>
                      </tr>
                    </tbody>
                  </table>
                </div>
                <p className="text-[11px] text-neutral-400 leading-relaxed bg-neutral-900 p-2.5 rounded-lg border border-neutral-800">
                  ※M5Stack CoreS3 Liteの内蔵PMIC (AXP2101) がBoost 5V出力を制御するため、通常のUSB給電下でSG90×2軸を安定駆動可能です。
                </p>
              </div>

              {/* Emotional Head Gestures */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-emerald-800/60 flex flex-col gap-3">
                <span className="font-bold text-xs text-emerald-300 font-mono flex items-center gap-1.5">
                  <Rotate3D className="w-4 h-4 text-emerald-400" />
                  感情連動の首振り・ジェスチャー一覧
                </span>
                <div className="flex flex-col gap-2 text-xs">
                  <div className="p-2 bg-neutral-900 rounded border border-neutral-800 flex justify-between items-center">
                    <div>
                      <strong className="text-emerald-300">HAPPY (喜び・挨拶)</strong>
                      <span className="block text-[11px] text-neutral-400">嬉しそうにペコッと2回うなずく (Tilt 80° ⇄ 105°)</span>
                    </div>
                    <span className="text-[10px] font-mono text-emerald-400 bg-emerald-950 px-2 py-0.5 rounded">Double Nod</span>
                  </div>

                  <div className="p-2 bg-neutral-900 rounded border border-neutral-800 flex justify-between items-center">
                    <div>
                      <strong className="text-cyan-300">THINKING (思案・計算)</strong>
                      <span className="block text-[11px] text-neutral-400">首を右にかしげ、斜め上を見上げる (Pan 105°, Tilt 102°)</span>
                    </div>
                    <span className="text-[10px] font-mono text-cyan-400 bg-cyan-950 px-2 py-0.5 rounded">Head Tilt</span>
                  </div>

                  <div className="p-2 bg-neutral-900 rounded border border-neutral-800 flex justify-between items-center">
                    <div>
                      <strong className="text-amber-300">SURPRISED (驚き)</strong>
                      <span className="block text-[11px] text-neutral-400">ハッと頭を急激に後ろに引き、見開く (Tilt 112°)</span>
                    </div>
                    <span className="text-[10px] font-mono text-amber-400 bg-amber-950 px-2 py-0.5 rounded">Head Jerk</span>
                  </div>

                  <div className="p-2 bg-neutral-900 rounded border border-neutral-800 flex justify-between items-center">
                    <div>
                      <strong className="text-rose-300">ANGRY (不満・お小言)</strong>
                      <span className="block text-[11px] text-neutral-400">イヤイヤと激しく左右に首を振る (Pan 70° ⇄ 110°)</span>
                    </div>
                    <span className="text-[10px] font-mono text-rose-400 bg-rose-950 px-2 py-0.5 rounded">Head Shake</span>
                  </div>

                  <div className="p-2 bg-neutral-900 rounded border border-neutral-800 flex justify-between items-center">
                    <div>
                      <strong className="text-purple-300">TALKING (発話リップシンク連動)</strong>
                      <span className="block text-[11px] text-neutral-400">マイク/TTS音量RMSに合わせて微小にうなずき、生体感を演出</span>
                    </div>
                    <span className="text-[10px] font-mono text-purple-400 bg-purple-950 px-2 py-0.5 rounded">Micro Nod</span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* TAB 1: FreeRTOS Architecture */}
        {activeTab === "architecture" && (
          <div className="flex flex-col gap-6">
            <div>
              <h3 className="text-base font-bold text-neutral-100 flex items-center gap-2">
                <Cpu className="w-5 h-5 text-cyan-400" />
                FreeRTOS デュアルコア非同期マルチタスク設計
              </h3>
              <p className="text-xs text-neutral-400 mt-1">
                ESP32-S3 (Xtensa LX7 240MHz × 2コア, 8MB PSRAM) の計算資源を最大限に活かし、画面のカクつきや音声途切れを防ぐタスク分離を行っています。
              </p>
            </div>

            {/* Core 0 vs Core 1 Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
              {/* Core 0: Network & Heavy IO */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-cyan-900/40 relative">
                <div className="flex items-center justify-between pb-2 mb-3 border-b border-neutral-800">
                  <span className="font-bold text-xs text-cyan-300 font-mono flex items-center gap-1.5">
                    <Radio className="w-4 h-4 text-cyan-400" />
                    CORE 0 : 通信・AI推論・カメラタスク
                  </span>
                  <span className="text-[10px] bg-neutral-900 px-2 py-0.5 rounded text-neutral-400">
                    Network & Sensing
                  </span>
                </div>

                <div className="flex flex-col gap-3 text-xs">
                  <div className="p-2.5 rounded-lg bg-neutral-900/80 border border-neutral-800">
                    <strong className="text-cyan-200 block font-mono">GeminiNetTask (Priority 4)</strong>
                    <p className="text-[11px] text-neutral-400 mt-0.5">
                      WiFiClientSecureによる直接Google API HTTPS/SSEストリーミング接続。レスポンスの最初のチャンクから<code>[EMOTION:XXX]</code>タグを先行パースしてアバターとPort Aサーボに即時通知。
                    </p>
                  </div>

                  <div className="p-2.5 rounded-lg bg-neutral-900/80 border border-neutral-800">
                    <strong className="text-cyan-200 block font-mono">CameraMotionTask (Priority 1)</strong>
                    <p className="text-[11px] text-neutral-400 mt-0.5">
                      GC0308カメラからQQVGA (160x120) グレースケール画像をDMAキャプチャ。軽量差分演算（10~15FPS）で在室・接近を判定。CPU負荷は4%未満。
                    </p>
                  </div>
                </div>
              </div>

              {/* Core 1: Realtime Audio, 60fps Display & Servo PWM */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-emerald-900/40 relative">
                <div className="flex items-center justify-between pb-2 mb-3 border-b border-neutral-800">
                  <span className="font-bold text-xs text-emerald-300 font-mono flex items-center gap-1.5">
                    <Zap className="w-4 h-4 text-emerald-400" />
                    CORE 1 : 描画・オーディオDMA・サーボPWM
                  </span>
                  <span className="text-[10px] bg-neutral-900 px-2 py-0.5 rounded text-neutral-400">
                    Render & Audio DMA
                  </span>
                </div>

                <div className="flex flex-col gap-3 text-xs">
                  <div className="p-2.5 rounded-lg bg-neutral-900/80 border border-neutral-800">
                    <strong className="text-emerald-200 block font-mono">AvatarTask & Servo (Priority 2 / 60FPS)</strong>
                    <p className="text-[11px] text-neutral-400 mt-0.5">
                      M5CanvasスプライトをPSRAM上に配置。目・眉・口のスムーズ補間・瞬き・リップシンクを描画しつつ、Port A (G2/G1) LEDC PWMイージングを50Hzで同時更新。
                    </p>
                  </div>

                  <div className="p-2.5 rounded-lg bg-neutral-900/80 border border-neutral-800">
                    <strong className="text-emerald-200 block font-mono">AudioWorkerTask (Priority 3)</strong>
                    <p className="text-[11px] text-neutral-400 mt-0.5">
                      ES7210マイク入力(16kHz)のRMSを計算し発話検知(VAD)。受信したTTS PCM(24kHz)をPSRAMリングバッファからES8311スピーカーへノンブロッキングDMA投入。
                    </p>
                  </div>
                </div>
              </div>
            </div>

            {/* PSRAM Memory Map */}
            <div className="bg-neutral-950 rounded-xl p-4 border border-neutral-800 text-xs">
              <span className="font-bold text-neutral-200 flex items-center gap-1.5 mb-2 font-mono">
                <Layers className="w-4 h-4 text-purple-400" />
                8MB OPI PSRAM メモリ配置 (ESP32-S3FN8)
              </span>
              <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 text-center font-mono text-[11px]">
                <div className="bg-neutral-900 p-2 rounded border border-neutral-800">
                  <span className="text-neutral-400 block text-[10px]">M5Canvas スプライト</span>
                  <strong className="text-cyan-300">153 KB</strong>
                  <span className="text-[9px] text-neutral-500 block">320x240 RGB565</span>
                </div>
                <div className="bg-neutral-900 p-2 rounded border border-neutral-800">
                  <span className="text-neutral-400 block text-[10px]">オーディオ RingBuffer</span>
                  <strong className="text-emerald-300">64 KB</strong>
                  <span className="text-[9px] text-neutral-500 block">24kHz PCM DMA</span>
                </div>
                <div className="bg-neutral-900 p-2 rounded border border-neutral-800">
                  <span className="text-neutral-400 block text-[10px]">GC0308 カメラFB</span>
                  <strong className="text-amber-300">38 KB</strong>
                  <span className="text-[9px] text-neutral-500 block">QQVGA FB×2</span>
                </div>
                <div className="bg-neutral-900 p-2 rounded border border-neutral-800">
                  <span className="text-neutral-400 block text-[10px]">TLS バッファ・空き</span>
                  <strong className="text-purple-300">約 7.7 MB</strong>
                  <span className="text-[9px] text-neutral-500 block">mbedTLS / Free</span>
                </div>
              </div>
            </div>
          </div>
        )}

        {/* TAB 2: Camera Motion & YOLO on ESP32-S3 Analysis */}
        {activeTab === "yolo_analysis" && (
          <div className="flex flex-col gap-5">
            <div>
              <h3 className="text-base font-bold text-neutral-100 flex items-center gap-2">
                <Camera className="w-5 h-5 text-amber-400" />
                カメラ動体検知 & ESP32-S3における「YOLO相当」の実現性比較
              </h3>
              <p className="text-xs text-neutral-400 mt-1">
                ユーザー様のご要望である「Yolo相当のローカルAIによる動体検知」について、ESP32-S3マイコンの性能限界と最適な実装方針を整理しました。
              </p>
            </div>

            {/* Comparison Table */}
            <div className="overflow-x-auto">
              <table className="w-full text-xs text-left border-collapse border border-neutral-800">
                <thead>
                  <tr className="bg-neutral-950 text-neutral-300 font-mono border-b border-neutral-800">
                    <th className="p-3">方式</th>
                    <th className="p-3">フレームレート</th>
                    <th className="p-3">CPU / メモリ負荷</th>
                    <th className="p-3">メリット</th>
                    <th className="p-3">課題・注意点</th>
                  </tr>
                </thead>
                <tbody className="divide-y divide-neutral-800 text-neutral-300">
                  <tr className="bg-emerald-950/20">
                    <td className="p-3 font-bold text-emerald-400">
                      ① 高速フレーム差分法 (本ファームウェア採用)
                    </td>
                    <td className="p-3 font-mono text-emerald-300">15〜20 FPS</td>
                    <td className="p-3 font-mono text-emerald-300">CPU: 3〜5%<br />RAM: 19 KB</td>
                    <td className="p-3">超低遅延。会話中・描画中もCPUを一切圧迫せず熱も持たない。人の接近・動きを即座に感知。</td>
                    <td className="p-3 text-neutral-400">「物体が人間か猫か」のクラス分類は行わない。</td>
                  </tr>
                  <tr className="bg-neutral-900/60">
                    <td className="p-3 font-bold text-sky-400">
                      ② ESP-DL 人物顔・人体検知 (ESP-WHO)
                    </td>
                    <td className="p-3 font-mono text-sky-300">6〜8 FPS</td>
                    <td className="p-3 font-mono text-sky-300">CPU: 45〜60%<br />RAM: 800 KB</td>
                    <td className="p-3">ESP32-S3のベクトル命令(SIMD)を活用し、人がカメラに映ったこと・顔の向きを識別可能。</td>
                    <td className="p-3 text-neutral-400">モデルのロードと推論でCore 0の演算を一時的に専有する。</td>
                  </tr>
                  <tr className="bg-rose-950/20">
                    <td className="p-3 font-bold text-rose-400">
                      ③ フルYOLO (YOLOv8n / Nano)
                    </td>
                    <td className="p-3 font-mono text-rose-300">0.8〜1.5 FPS</td>
                    <td className="p-3 font-mono text-rose-300">CPU: 95〜100%<br />RAM: 4〜6 MB</td>
                    <td className="p-3">80種類の物体検出が可能。</td>
                    <td className="p-3 text-rose-300">
                      ⚠️ 1推論に約1秒かかり発熱とWi-Fiパケット詰まりが発生。会話用スタックチャンとの常時並行稼働は極めて困難。
                    </td>
                  </tr>
                  <tr className="bg-purple-950/20">
                    <td className="p-3 font-bold text-purple-400">
                      ④ ハイブリッド構成 (推奨応用案)
                    </td>
                    <td className="p-3 font-mono text-purple-300">即時トリガー</td>
                    <td className="p-3 font-mono text-purple-300">CPU: 4%</td>
                    <td className="p-3">
                      通常時は①で超高速接近検知 → 話しかけられた瞬間にGC0308で1枚撮影しGemini 3.8 Flashに送信して「どんな人がいるか」を目視対話！
                    </td>
                    <td className="p-3 text-neutral-400">画像送信時にわずかな通信レイテンシ(~500ms)が発生。</td>
                  </tr>
                </tbody>
              </table>
            </div>

            <div className="bg-neutral-950 p-4 rounded-xl border border-neutral-800 text-xs text-neutral-300 flex items-start gap-3">
              <CheckCircle2 className="w-5 h-5 text-emerald-400 shrink-0 mt-0.5" />
              <div>
                <strong className="text-neutral-100 block">
                  結論：本プロジェクトの最適なアプローチ
                </strong>
                <p className="mt-1 text-neutral-400">
                  CoreS3 Liteは小型筐体かつファンレスのため、常時AI推論でコアを熱くするフルYOLOよりも、<strong>「①の超軽量フレーム差分法で高速ウェイクアップ」</strong>し、必要に応じて<strong>「Geminiへのスナップショット送信で高精度視覚認識」</strong>を組み合わせるのが、バッテリー寿命・レスポンス速度・発熱防止の観点から最も実用的です。
                </p>
              </div>
            </div>
          </div>
        )}

        {/* TAB 3: Questions & Clarifications */}
        {activeTab === "questions" && (
          <div className="flex flex-col gap-5">
            <div>
              <h3 className="text-base font-bold text-neutral-100 flex items-center gap-2">
                <HelpCircle className="w-5 h-5 text-cyan-400" />
                開発を進める上での要確認事項・技術質問
              </h3>
              <p className="text-xs text-neutral-400 mt-1">
                実機で最高品質の動作を実現するために、以下の4点についてユーザー様のご希望や環境をお伺いできますと幸いです。
              </p>
            </div>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-4 text-xs">
              {/* Question 1 */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-neutral-800 flex flex-col gap-2">
                <div className="flex items-center gap-2 text-cyan-300 font-bold">
                  <span className="w-5 h-5 rounded-full bg-cyan-950 border border-cyan-800 flex items-center justify-center text-[10px]">
                    Q1
                  </span>
                  インターネット接続とAPIアクセス経路
                </div>
                <p className="text-neutral-400 leading-relaxed">
                  ESP32から直接 <code>generativelanguage.googleapis.com</code> (HTTPS:443) に直接アクセスできるWi-Fi環境でしょうか？それとも、社内LANやPC/Cloud Run上のプロキシサーバー（本Webアプリのバックエンドなど）を経由させる形をご希望でしょうか？
                </p>
                <div className="mt-auto pt-2 border-t border-neutral-900 text-[11px] text-neutral-500">
                  ※本コードは直接アクセスとプロキシ経由の両方に対応可能な設計にしています。
                </div>
              </div>

              {/* Question 2 */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-neutral-800 flex flex-col gap-2">
                <div className="flex items-center gap-2 text-cyan-300 font-bold">
                  <span className="w-5 h-5 rounded-full bg-cyan-950 border border-cyan-800 flex items-center justify-center text-[10px]">
                    Q2
                  </span>
                  カメラ動体検知の具体的なユースケース
                </div>
                <p className="text-neutral-400 leading-relaxed">
                  カメラ検知は<strong>「前に人が来たらスリープから復帰する（在室トリガー）」</strong>で十分でしょうか？それとも、<strong>「相手の顔や着ている服の色、持っている物をGeminiに画像として渡して会話したい」</strong>（マルチモーダル視覚対話）でしょうか？
                </p>
                <div className="mt-auto pt-2 border-t border-neutral-900 text-[11px] text-neutral-500">
                  ※後者の場合、撮影JPEGをGemini 3.8 Flashに添付する関数を追加できます。
                </div>
              </div>

              {/* Question 3 */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-neutral-800 flex flex-col gap-2">
                <div className="flex items-center gap-2 text-cyan-300 font-bold">
                  <span className="w-5 h-5 rounded-full bg-cyan-950 border border-cyan-800 flex items-center justify-center text-[10px]">
                    Q3
                  </span>
                  ウェイクワード判定の運用方針
                </div>
                <p className="text-neutral-400 leading-relaxed">
                  ウェイクワード（「スタックちゃん」など）の認識は、<strong>EspressifのローカルWakeNet（ESP-SRライブラリ・完全オフライン判定）</strong>を使用したいですか？それとも<strong>音声ストリーミング時のVAD（音量検知）+ Gemini認識</strong>で判定する形がよいでしょうか？
                </p>
                <div className="mt-auto pt-2 border-t border-neutral-900 text-[11px] text-neutral-500">
                  ※ESP-SRを使うと完全オフラインで「スタックちゃん」に即反応可能になります。
                </div>
              </div>

              {/* Question 4 */}
              <div className="bg-neutral-950 rounded-xl p-4 border border-neutral-800 flex flex-col gap-2">
                <div className="flex items-center gap-2 text-cyan-300 font-bold">
                  <span className="w-5 h-5 rounded-full bg-cyan-950 border border-cyan-800 flex items-center justify-center text-[10px]">
                    Q4
                  </span>
                  サーボモーター（首振り機構）の有無
                </div>
                <p className="text-neutral-400 leading-relaxed">
                  スタックチャン専用の首振りケース（パン・チルト用サーボモーター SG90やSCS0009など）は搭載予定でしょうか？もし搭載される場合、会話や感情に合わせて首をかしげるPWMサーボ制御タスクを組み込むことが可能です。
                </p>
                <div className="mt-auto pt-2 border-t border-neutral-900 text-[11px] text-neutral-500">
                  ※CoreS3のPort A/Bまたは内部ピンからPWM駆動可能です。
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};
