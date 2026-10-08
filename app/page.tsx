"use client";

import React, { useState, useRef, useEffect } from "react";
import { EmotionType } from "@/components/StackChanFace";
import { CoreS3HardwareFrame } from "@/components/CoreS3HardwareFrame";
import { VoiceCameraSimulator } from "@/components/VoiceCameraSimulator";
import { FirmwareCodeStudio } from "@/components/FirmwareCodeStudio";
import { SettingsPanel } from "@/components/SettingsPanel";
import { TechnicalQuestionsAndArchitecture } from "@/components/TechnicalQuestionsAndArchitecture";
import {
  Sparkles,
  Bot,
  Cpu,
  Layers,
  Settings,
  Download,
  Terminal,
  Activity,
  Volume2,
  HelpCircle,
  ExternalLink,
} from "lucide-react";

interface ChatMessage {
  role: "user" | "assistant";
  text: string;
  emotion?: EmotionType;
}

export default function HomePage() {
  // App state machine: SLEEP -> STANDBY_WAIT_KEYWORD -> LISTENING -> THINKING -> SPEAKING -> WAIT_FOLLOWUP
  const [appState, setAppState] = useState<string>("SLEEP");
  const [emotion, setEmotion] = useState<EmotionType>("SLEEP");
  const [lipSyncLevel, setLipSyncLevel] = useState<number>(0);
  const [isSpeaking, setIsSpeaking] = useState<boolean>(false);
  const [audioInputLevel, setAudioInputLevel] = useState<number>(0);

  // Settings
  const [wifiSsid, setWifiSsid] = useState("MyHome-WiFi_2.4G");
  const [wifiPass, setWifiPass] = useState("");
  const [geminiApiKey, setGeminiApiKey] = useState("");
  const [geminiModel, setGeminiModel] = useState("gemini-3.8-flash");
  const [ttsVoice, setTtsVoice] = useState("Kore");
  const [wakeWord, setWakeWord] = useState("スタックちゃん");
  const [silenceTimeoutSec, setSilenceTimeoutSec] = useState(6);
  const [motionThreshold, setMotionThreshold] = useState(25);

  // Port A SG90 2-axis servo settings
  const [servoEnabled, setServoEnabled] = useState(true);
  const [servoPanPin, setServoPanPin] = useState(2);
  const [servoTiltPin, setServoTiltPin] = useState(1);
  const [servoPanCenter, setServoPanCenter] = useState(90);
  const [servoTiltCenter, setServoTiltCenter] = useState(90);

  // Chat conversation history
  const [chatLog, setChatLog] = useState<ChatMessage[]>([
    {
      role: "assistant",
      text: "（すやすや眠っています... カメラで動体を検知するか、画面をタップして起こしてください）",
      emotion: "SLEEP",
    },
  ]);

  // Audio player references
  const audioContextRef = useRef<AudioContext | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const animFrameRef = useRef<number | null>(null);
  const currentAudioRef = useRef<HTMLAudioElement | null>(null);

  // Reset defaults handler
  const handleResetDefaults = () => {
    setWifiSsid("MyHome-WiFi_2.4G");
    setWifiPass("");
    setGeminiApiKey("");
    setGeminiModel("gemini-3.8-flash");
    setTtsVoice("Kore");
    setWakeWord("スタックちゃん");
    setSilenceTimeoutSec(6);
    setMotionThreshold(25);
    setServoEnabled(true);
    setServoPanPin(2);
    setServoTiltPin(1);
    setServoPanCenter(90);
    setServoTiltCenter(90);
  };

  // Motion detected trigger
  const handleMotionDetected = () => {
    if (appState === "SLEEP") {
      setAppState("STANDBY_WAIT_KEYWORD");
      setEmotion("NORMAL");
      setChatLog((prev) => [
        ...prev,
        {
          role: "assistant",
          text: "（カメラが動体を検知しました！「スタックちゃん」と声をかけるか、テスト発話してください）",
          emotion: "NORMAL",
        },
      ]);
    }
  };

  // Silence timeout trigger
  const handleSilenceTimeout = () => {
    if (appState === "WAIT_FOLLOWUP" || appState === "STANDBY_WAIT_KEYWORD") {
      setAppState("SLEEP");
      setEmotion("SLEEP");
      setChatLog((prev) => [
        ...prev,
        {
          role: "assistant",
          text: "またね、おやすみなさい...（一定時間無音が続いたためスリープ状態に移行しました）",
          emotion: "SLEEP",
        },
      ]);
    }
  };

  // Screen click / wake trigger
  const handleScreenClick = () => {
    if (appState === "SLEEP") {
      setAppState("LISTENING");
      setEmotion("HAPPY");
      setChatLog((prev) => [
        ...prev,
        {
          role: "assistant",
          text: "タッチ検知！起きたよ！何をお話しする？",
          emotion: "HAPPY",
        },
      ]);
    } else {
      setEmotion("HAPPY");
      setTimeout(() => {
        if (!isSpeaking) setEmotion("NORMAL");
      }, 1500);
    }
  };

  // Send message to Gemini API with TTS streaming playback & lip sync
  const handleSendMessage = async (msgText: string) => {
    if (!msgText.trim()) return;

    // 1. Update UI state
    setChatLog((prev) => [...prev, { role: "user", text: msgText }]);
    setAppState("THINKING");
    setEmotion("THINKING");

    try {
      // 2. Call server API safely
      let data: any = null;
      try {
        const res = await fetch("/api/gemini/chat", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            message: msgText,
            history: chatLog.slice(-4),
            voice: ttsVoice,
            requestTts: true,
          }),
        });

        const contentType = res.headers.get("content-type") || "";
        if (contentType.includes("application/json")) {
          data = await res.json();
        } else {
          const text = await res.text();
          console.warn("Received non-JSON response from API:", text.slice(0, 100));
        }
      } catch (fetchErr) {
        console.warn("Network fetch error, proceeding to client fallback:", fetchErr);
      }

      // Safe defaults if API was unavailable or returned non-JSON
      const replyEmotion = (data?.emotion as EmotionType) || "HAPPY";
      const speechText =
        data?.speechText ||
        data?.rawText ||
        "こんにちは！スタックちゃんですよ。何でも話しかけてね！";

      setEmotion(replyEmotion);
      setAppState("SPEAKING");
      setIsSpeaking(true);

      setChatLog((prev) => [
        ...prev,
        {
          role: "assistant",
          text: speechText,
          emotion: replyEmotion,
        },
      ]);

      // 3. Playback TTS Audio & drive lip-sync
      if (data?.audioBase64) {
        if (currentAudioRef.current) {
          try {
            currentAudioRef.current.pause();
          } catch {}
        }
        const audioUrl = `data:audio/wav;base64,${data.audioBase64}`;
        const audio = new Audio(audioUrl);
        currentAudioRef.current = audio;

        // Setup Web Audio Analyser for realistic lip-syncing
        try {
          const audioCtx = new (window.AudioContext || (window as any).webkitAudioContext)();
          audioContextRef.current = audioCtx;
          const source = audioCtx.createMediaElementSource(audio);
          const analyser = audioCtx.createAnalyser();
          analyser.fftSize = 256;
          source.connect(analyser);
          analyser.connect(audioCtx.destination);
          analyserRef.current = analyser;

          const dataArray = new Uint8Array(analyser.frequencyBinCount);

          const trackLipSync = () => {
            if (!audio.paused && !audio.ended && analyserRef.current) {
              analyserRef.current.getByteFrequencyData(dataArray);
              let sum = 0;
              for (let i = 0; i < dataArray.length; i++) sum += dataArray[i];
              const avg = sum / dataArray.length;
              const level = Math.min(1.0, avg / 70); // normalization for mouth opening
              setLipSyncLevel(level);
              animFrameRef.current = requestAnimationFrame(trackLipSync);
            } else {
              setLipSyncLevel(0);
            }
          };

          audio.onplay = () => {
            trackLipSync();
          };
        } catch (ctxErr) {
          console.warn("Analyser fallback to simulated lip sync:", ctxErr);
        }

        audio.onended = () => {
          setIsSpeaking(false);
          setLipSyncLevel(0);
          setAppState("WAIT_FOLLOWUP");
        };

        audio.onerror = () => {
          setIsSpeaking(false);
          setLipSyncLevel(0);
          setAppState("WAIT_FOLLOWUP");
        };

        await audio.play().catch(() => {
          setIsSpeaking(false);
          setAppState("WAIT_FOLLOWUP");
        });
      } else {
        // Fallback: Web Speech API synthesis
        if ("speechSynthesis" in window) {
          window.speechSynthesis.cancel();
          const utter = new SpeechSynthesisUtterance(speechText);
          utter.lang = "ja-JP";
          utter.rate = 1.05;

          utter.onboundary = () => {
            setLipSyncLevel(0.4 + Math.random() * 0.4);
          };
          utter.onend = () => {
            setIsSpeaking(false);
            setLipSyncLevel(0);
            setAppState("WAIT_FOLLOWUP");
          };
          utter.onerror = () => {
            setIsSpeaking(false);
            setLipSyncLevel(0);
            setAppState("WAIT_FOLLOWUP");
          };
          window.speechSynthesis.speak(utter);
        } else {
          setTimeout(() => {
            setIsSpeaking(false);
            setAppState("WAIT_FOLLOWUP");
          }, 2500);
        }
      }
    } catch (err: any) {
      console.warn("Handled interaction error:", err);
      setEmotion("HAPPY");
      setAppState("WAIT_FOLLOWUP");
      setIsSpeaking(false);
    }
  };

  return (
    <div className="min-h-screen bg-[#0a0d14] text-neutral-100 flex flex-col font-sans selection:bg-cyan-500 selection:text-black">
      {/* Top Banner / Navbar */}
      <header className="border-b border-neutral-800 bg-[#0d121d]/80 backdrop-blur-md sticky top-0 z-50 px-6 py-3.5 flex items-center justify-between">
        <div className="flex items-center gap-3">
          <div className="w-8 h-8 rounded-xl bg-gradient-to-tr from-cyan-600 to-emerald-500 flex items-center justify-center shadow-lg shadow-cyan-500/20">
            <Bot className="w-5 h-5 text-white" />
          </div>
          <div>
            <div className="flex items-center gap-2">
              <h1 className="font-bold text-sm tracking-tight text-white">
                M5Stack CoreS3 Lite × Gemini AI Voice Stack-chan
              </h1>
              <span className="bg-cyan-500/10 text-cyan-400 border border-cyan-500/30 text-[10px] font-mono px-2 py-0.5 rounded-full font-semibold">
                v1.0.0
              </span>
            </div>
            <p className="text-[11px] text-neutral-400">
              低遅延ストリーミング • 感情アバター • カメラ動体検知 • FreeRTOS最適化ファームウェア
            </p>
          </div>
        </div>

        <div className="flex items-center gap-2">
          <a
            href="#code-studio"
            className="hidden sm:flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-neutral-800 hover:bg-neutral-700 text-neutral-200 text-xs font-medium border border-neutral-700 transition"
          >
            <Terminal className="w-3.5 h-3.5 text-cyan-400" />
            ファームウェアコード
          </a>
          <a
            href="#technical-guide"
            className="hidden sm:flex items-center gap-1.5 px-3 py-1.5 rounded-lg bg-neutral-800 hover:bg-neutral-700 text-neutral-200 text-xs font-medium border border-neutral-700 transition"
          >
            <HelpCircle className="w-3.5 h-3.5 text-amber-400" />
            要確認事項・Q&A
          </a>
        </div>
      </header>

      {/* Main Container */}
      <main className="flex-1 max-w-7xl w-full mx-auto p-4 sm:p-6 lg:p-8 flex flex-col gap-8">
        
        {/* Hero Section */}
        <div className="flex flex-col gap-2">
          <div className="flex flex-wrap gap-2 items-center text-xs">
            <span className="px-2.5 py-1 rounded-full bg-cyan-950 text-cyan-300 border border-cyan-800 font-mono font-medium flex items-center gap-1">
              <Sparkles className="w-3 h-3 text-cyan-400" />
              Gemini 3.8 Flash & Gemini 3.8 Flash Lite TTS
            </span>
            <span className="px-2.5 py-1 rounded-full bg-emerald-950 text-emerald-300 border border-emerald-800 font-mono font-medium flex items-center gap-1">
              <Cpu className="w-3 h-3 text-emerald-400" />
              ESP32-S3FN8 (Dual-Core 240MHz, 8MB PSRAM)
            </span>
            <span className="px-2.5 py-1 rounded-full bg-amber-950 text-amber-300 border border-amber-800 font-mono font-medium flex items-center gap-1">
              <Activity className="w-3 h-3 text-amber-400" />
              GC0308 カメラ動体検知 & VAD無音自動終了
            </span>
          </div>
          <h2 className="text-xl sm:text-2xl font-black text-neutral-100 tracking-tight">
            M5Stack CoreS3 Lite専用 音声対話スタックチャン統合ワークベンチ
          </h2>
          <p className="text-xs sm:text-sm text-neutral-400 max-w-3xl leading-relaxed">
            M5Stack CoreS3 Liteの実機をリアルタイムにエミュレートするWebシミュレータと、そのままPlatformIO / Arduino IDEでビルドできる最適化済みC++ファームウェアコードを完全完備。会話内容に応じた感情表現・瞬き・リップシンク、低遅延ストリーミング、動体検知ウェイクアップをお試しいただけます。
          </p>
        </div>

        {/* Live Simulator Area (2 Columns) */}
        <div className="grid grid-cols-1 lg:grid-cols-12 gap-6 items-start">
          {/* Left: CoreS3 Device Frame (5 cols) */}
          <div className="lg:col-span-5 flex flex-col items-center gap-4 bg-gradient-to-b from-neutral-900/60 to-neutral-950/60 p-6 rounded-3xl border border-neutral-800/80 shadow-2xl">
            <div className="w-full flex items-center justify-between text-xs text-neutral-400 font-mono px-2">
              <span className="flex items-center gap-1.5">
                <span className="w-2 h-2 rounded-full bg-emerald-400 animate-ping" />
                VIRTUAL M5STACK HARDWARE
              </span>
              <span>IPS 320x240 LCD</span>
            </div>

            <CoreS3HardwareFrame
              emotion={emotion}
              lipSyncLevel={lipSyncLevel}
              isSpeaking={isSpeaking}
              audioInputLevel={audioInputLevel}
              isCameraActive={true}
              appState={appState}
              servoEnabled={servoEnabled}
              onScreenClick={handleScreenClick}
              onPowerClick={handleScreenClick}
            />

            {/* Recent Dialogue Bubble */}
            <div className="w-full bg-neutral-950 rounded-2xl p-4 border border-neutral-800 text-xs flex flex-col gap-2 max-h-48 overflow-y-auto">
              <span className="text-[11px] font-mono text-neutral-500 uppercase">
                リアルタイム対話ログ
              </span>
              {chatLog.slice(-3).map((item, i) => (
                <div
                  key={i}
                  className={`p-2.5 rounded-xl ${
                    item.role === "user"
                      ? "bg-cyan-950/60 text-cyan-200 border border-cyan-800/50 ml-4"
                      : "bg-neutral-900 text-neutral-200 border border-neutral-800 mr-4"
                  }`}
                >
                  <div className="flex items-center justify-between text-[10px] text-neutral-400 mb-1">
                    <span>{item.role === "user" ? "👤 ユーザー" : "🤖 スタックチャン"}</span>
                    {item.emotion && (
                      <span className="font-mono text-cyan-300 font-bold">
                        [{item.emotion}]
                      </span>
                    )}
                  </div>
                  <p className="leading-relaxed">{item.text}</p>
                </div>
              ))}
            </div>
          </div>

          {/* Right: Camera Motion & Voice STT Simulator (7 cols) */}
          <div className="lg:col-span-7 flex flex-col gap-6">
            <VoiceCameraSimulator
              appState={appState}
              wakeWord={wakeWord}
              silenceTimeoutSec={silenceTimeoutSec}
              motionThreshold={motionThreshold}
              onStateChange={(st) => setAppState(st)}
              onSendMessage={handleSendMessage}
              onMotionDetected={handleMotionDetected}
              onSilenceTimeout={handleSilenceTimeout}
              audioInputLevel={audioInputLevel}
              setAudioInputLevel={setAudioInputLevel}
            />
          </div>
        </div>

        {/* Configuration Parameter Editor (config.json) */}
        <div id="settings-panel">
          <SettingsPanel
            wifiSsid={wifiSsid}
            setWifiSsid={setWifiSsid}
            wifiPass={wifiPass}
            setWifiPass={setWifiPass}
            geminiApiKey={geminiApiKey}
            setGeminiApiKey={setGeminiApiKey}
            geminiModel={geminiModel}
            setGeminiModel={setGeminiModel}
            ttsVoice={ttsVoice}
            setTtsVoice={setTtsVoice}
            wakeWord={wakeWord}
            setWakeWord={setWakeWord}
            silenceTimeoutSec={silenceTimeoutSec}
            setSilenceTimeoutSec={setSilenceTimeoutSec}
            motionThreshold={motionThreshold}
            setMotionThreshold={setMotionThreshold}
            servoEnabled={servoEnabled}
            setServoEnabled={setServoEnabled}
            servoPanPin={servoPanPin}
            setServoPanPin={setServoPanPin}
            servoTiltPin={servoTiltPin}
            setServoTiltPin={setServoTiltPin}
            servoPanCenter={servoPanCenter}
            setServoPanCenter={setServoPanCenter}
            servoTiltCenter={servoTiltCenter}
            setServoTiltCenter={setServoTiltCenter}
            onResetDefaults={handleResetDefaults}
          />
        </div>

        {/* Firmware Code Studio Section */}
        <div id="code-studio">
          <FirmwareCodeStudio
            apiKeyParam={geminiApiKey}
            wifiSsidParam={wifiSsid}
            wifiPassParam={wifiPass}
            ttsVoiceParam={ttsVoice}
            wakeWordParam={wakeWord}
            silenceSecParam={silenceTimeoutSec}
            motionThreshParam={motionThreshold}
            servoEnabledParam={servoEnabled}
            servoPanPinParam={servoPanPin}
            servoTiltPinParam={servoTiltPin}
            servoPanCenterParam={servoPanCenter}
            servoTiltCenterParam={servoTiltCenter}
          />
        </div>

        {/* Architecture & Q&A Clarification Section */}
        <div id="technical-guide">
          <TechnicalQuestionsAndArchitecture />
        </div>

      </main>

      {/* Footer */}
      <footer className="border-t border-neutral-800 bg-[#0d121d] py-6 px-6 text-center text-xs text-neutral-500 flex flex-col sm:flex-row items-center justify-between gap-3 max-w-7xl mx-auto w-full">
        <div className="flex items-center gap-2">
          <span>M5Stack CoreS3 Lite Gemini AI Voice Stack-chan</span>
          <span>•</span>
          <span className="text-neutral-400">FreeRTOS Dual-Core Architecture</span>
        </div>
        <div className="flex items-center gap-4 text-neutral-400">
          <span>GC0308 Camera (160x120)</span>
          <span>ES7210 Mic (16kHz)</span>
          <span>ES8311 Spk (24kHz DMA)</span>
        </div>
      </footer>
    </div>
  );
}
