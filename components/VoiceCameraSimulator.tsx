"use client";

import React, { useState, useEffect, useRef } from "react";
import {
  Mic,
  MicOff,
  Video,
  VideoOff,
  Send,
  Sparkles,
  Volume2,
  Activity,
  Play,
  RotateCcw,
  Eye,
  Sliders,
} from "lucide-react";

interface VoiceCameraSimulatorProps {
  appState: string;
  wakeWord: string;
  silenceTimeoutSec: number;
  motionThreshold: number;
  onStateChange: (newState: string) => void;
  onSendMessage: (msg: string) => void;
  onMotionDetected: () => void;
  onSilenceTimeout: () => void;
  audioInputLevel: number;
  setAudioInputLevel: (lvl: number) => void;
}

export const VoiceCameraSimulator: React.FC<VoiceCameraSimulatorProps> = ({
  appState,
  wakeWord,
  silenceTimeoutSec,
  motionThreshold,
  onStateChange,
  onSendMessage,
  onMotionDetected,
  onSilenceTimeout,
  audioInputLevel,
  setAudioInputLevel,
}) => {
  // Camera state
  const [useWebcam, setUseWebcam] = useState(false);
  const [motionScore, setMotionScore] = useState(0);
  const [cameraActive, setCameraActive] = useState(true);
  const videoRef = useRef<HTMLVideoElement | null>(null);
  const camCanvasRef = useRef<HTMLCanvasElement | null>(null);
  const prevFrameRef = useRef<Uint8Array | null>(null);

  // Microphone state
  const [micActive, setMicActive] = useState(false);
  const [transcript, setTranscript] = useState("");
  const [customInput, setCustomInput] = useState("");
  const recognitionRef = useRef<any>(null);
  const audioContextRef = useRef<AudioContext | null>(null);
  const analyserRef = useRef<AnalyserNode | null>(null);
  const animFrameRef = useRef<number | null>(null);

  // Silence timer
  const [silenceCounter, setSilenceCounter] = useState(0);
  const lastVoiceTimeRef = useRef<number>(0);

  useEffect(() => {
    lastVoiceTimeRef.current = Date.now();
  }, []);

  // Synthetic camera motion generator
  const synthPosRef = useRef({ x: 20, vx: 1.2 });

  // 1. Microphone setup (Audio level + Speech Recognition)
  useEffect(() => {
    if (!micActive) {
      if (recognitionRef.current) {
        try {
          recognitionRef.current.stop();
        } catch {}
      }
      if (audioContextRef.current && audioContextRef.current.state !== "closed") {
        audioContextRef.current.close().catch(() => {});
        audioContextRef.current = null;
      }
      setAudioInputLevel(0);
      return;
    }

    let isCancelled = false;

    // A. Web Audio API for live VU meter
    navigator.mediaDevices
      .getUserMedia({ audio: true })
      .then((stream) => {
        if (isCancelled) return;
        const audioCtx = new (window.AudioContext || (window as any).webkitAudioContext)();
        audioContextRef.current = audioCtx;
        const source = audioCtx.createMediaStreamSource(stream);
        const analyser = audioCtx.createAnalyser();
        analyser.fftSize = 256;
        source.connect(analyser);
        analyserRef.current = analyser;

        const dataArray = new Uint8Array(analyser.frequencyBinCount);

        const checkAudio = () => {
          if (!micActive || !analyserRef.current) return;
          analyserRef.current.getByteFrequencyData(dataArray);
          let sum = 0;
          for (let i = 0; i < dataArray.length; i++) {
            sum += dataArray[i];
          }
          const avg = sum / dataArray.length;
          const level = Math.min(1.0, avg / 128);
          setAudioInputLevel(level);

          if (level > 0.1) {
            lastVoiceTimeRef.current = Date.now();
            setSilenceCounter(0);
          }

          animFrameRef.current = requestAnimationFrame(checkAudio);
        };
        checkAudio();
      })
      .catch((err) => {
        console.warn("Mic access error:", err);
      });

    // B. Web Speech Recognition
    const SpeechRecognition =
      (window as any).SpeechRecognition || (window as any).webkitSpeechRecognition;

    if (SpeechRecognition) {
      const recognition = new SpeechRecognition();
      recognition.continuous = true;
      recognition.interimResults = true;
      recognition.lang = "ja-JP";

      recognition.onresult = (event: any) => {
        let currentText = "";
        for (let i = event.resultIndex; i < event.results.length; ++i) {
          currentText += event.results[i][0].transcript;
        }
        setTranscript(currentText);
        lastVoiceTimeRef.current = Date.now();
        setSilenceCounter(0);

        // Wake word trigger
        if (
          appState === "SLEEP" ||
          appState === "STANDBY_WAIT_KEYWORD"
        ) {
          if (
            currentText.includes(wakeWord) ||
            currentText.includes("スタックちゃん") ||
            currentText.includes("こんにちは") ||
            currentText.includes("おはよう")
          ) {
            onStateChange("LISTENING");
          }
        }

        // Final speech detected
        if (event.results[event.results.length - 1].isFinal) {
          const finalPrompt = currentText.trim();
          if (finalPrompt.length > 0) {
            onSendMessage(finalPrompt);
            setTranscript("");
          }
        }
      };

      recognition.onerror = (e: any) => {
        console.warn("Speech recognition error:", e);
      };

      try {
        recognition.start();
        recognitionRef.current = recognition;
      } catch (e) {
        console.error(e);
      }
    }

    return () => {
      isCancelled = true;
      if (animFrameRef.current) cancelAnimationFrame(animFrameRef.current);
      if (recognitionRef.current) {
        try {
          recognitionRef.current.stop();
        } catch {}
      }
      if (audioContextRef.current && audioContextRef.current.state !== "closed") {
        audioContextRef.current.close().catch(() => {});
      }
    };
  }, [micActive, appState, wakeWord, onSendMessage, onStateChange, setAudioInputLevel]);

  // 2. Silence timeout interval watcher
  useEffect(() => {
    const timer = setInterval(() => {
      if (appState === "WAIT_FOLLOWUP" || appState === "STANDBY_WAIT_KEYWORD") {
        const elapsedSec = (Date.now() - lastVoiceTimeRef.current) / 1000;
        setSilenceCounter(Math.floor(elapsedSec));
        if (elapsedSec >= silenceTimeoutSec) {
          onSilenceTimeout();
          setSilenceCounter(0);
        }
      } else {
        setSilenceCounter(0);
      }
    }, 500);

    return () => clearInterval(timer);
  }, [appState, silenceTimeoutSec, onSilenceTimeout]);

  // 3. Camera Motion Engine (QQVGA 160x120 Grayscale Frame Differencing)
  useEffect(() => {
    if (!cameraActive) return;

    let videoStream: MediaStream | null = null;
    if (useWebcam) {
      navigator.mediaDevices
        .getUserMedia({ video: { width: 160, height: 120 } })
        .then((stream) => {
          videoStream = stream;
          if (videoRef.current) {
            videoRef.current.srcObject = stream;
            videoRef.current.play().catch(() => {});
          }
        })
        .catch(() => {
          setUseWebcam(false);
        });
    }

    const interval = setInterval(() => {
      const canvas = camCanvasRef.current;
      if (!canvas) return;
      const ctx = canvas.getContext("2d", { willReadFrequently: true });
      if (!ctx) return;

      const W = 160;
      const H = 120;

      if (useWebcam && videoRef.current && videoRef.current.readyState >= 2) {
        // Draw real webcam frame
        ctx.drawImage(videoRef.current, 0, 0, W, H);
      } else {
        // Draw synthetic room simulation
        ctx.fillStyle = "#1e293b";
        ctx.fillRect(0, 0, W, H);

        // Simulated background furniture
        ctx.fillStyle = "#334155";
        ctx.fillRect(20, 70, 40, 50); // desk
        ctx.fillRect(100, 50, 40, 70); // door

        // Moving person silhouette
        const p = synthPosRef.current;
        p.x += p.vx;
        if (p.x > 130 || p.x < 15) p.vx *= -1;

        ctx.fillStyle = "#64748b";
        // Head
        ctx.beginPath();
        ctx.arc(p.x, 40, 10, 0, Math.PI * 2);
        ctx.fill();
        // Body
        ctx.beginPath();
        ctx.roundRect(p.x - 12, 52, 24, 45, 6);
        ctx.fill();
      }

      // Read pixels for frame differencing
      const imgData = ctx.getImageData(0, 0, W, H);
      const data = imgData.data;
      const totalPixels = W * H;

      if (!prevFrameRef.current) {
        prevFrameRef.current = new Uint8Array(totalPixels);
        for (let i = 0; i < totalPixels; i++) {
          prevFrameRef.current[i] = data[i * 4];
        }
        return;
      }

      let diffCount = 0;
      const step = 4; // match ESP32 step=4 sub-sampling
      const prev = prevFrameRef.current;

      for (let y = 0; y < H; y += step) {
        const row = y * W;
        for (let x = 0; x < W; x += step) {
          const idx = row + x;
          const gray = data[idx * 4]; // R channel as luminance
          const diff = Math.abs(gray - prev[idx]);
          if (diff > 18) {
            diffCount++;
          }
          prev[idx] = Math.round((prev[idx] * 4 + gray) / 5);
        }
      }

      const sampledTotal = (W / step) * (H / step);
      const score = Math.min(100, Math.round((diffCount / sampledTotal) * 100));
      setMotionScore(score);

      // Trigger motion wakeup when score exceeds threshold
      if (score >= motionThreshold && appState === "SLEEP") {
        onMotionDetected();
      }
    }, 120);

    return () => {
      clearInterval(interval);
      if (videoStream) {
        videoStream.getTracks().forEach((track) => track.stop());
      }
    };
  }, [cameraActive, useWebcam, motionThreshold, appState, onMotionDetected]);

  return (
    <div className="bg-neutral-900/90 border border-neutral-800 rounded-2xl p-5 backdrop-blur-md shadow-xl flex flex-col gap-5">
      {/* Header */}
      <div className="flex items-center justify-between pb-3 border-b border-neutral-800">
        <div className="flex items-center gap-2">
          <Activity className="w-5 h-5 text-cyan-400" />
          <h3 className="font-semibold text-neutral-100 text-sm">
            CoreS3 センサー & インプット シミュレータ
          </h3>
        </div>
        <span className="text-xs px-2.5 py-0.5 rounded-full bg-cyan-950 text-cyan-300 border border-cyan-800 font-mono">
          GC0308 + ES7210 VAD
        </span>
      </div>

      {/* Grid: Camera Motion + Mic / VAD */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* 1. Camera Motion Detection Panel */}
        <div className="bg-neutral-950 rounded-xl p-3.5 border border-neutral-800 flex flex-col gap-2.5">
          <div className="flex items-center justify-between text-xs text-neutral-300">
            <span className="font-medium flex items-center gap-1.5">
              <Eye className="w-3.5 h-3.5 text-emerald-400" />
              GC0308 カメラ動体検知 (160x120 QQVGA)
            </span>
            <button
              onClick={() => setUseWebcam(!useWebcam)}
              className="text-[11px] text-cyan-400 hover:text-cyan-300 underline font-mono cursor-pointer"
            >
              {useWebcam ? "Webカメラ使用中" : "シミュレーション中"}
            </button>
          </div>

          {/* Camera Canvas Preview */}
          <div className="relative aspect-[4/3] w-full bg-neutral-900 rounded-lg overflow-hidden border border-neutral-800 flex items-center justify-center">
            <canvas
              ref={camCanvasRef}
              width={160}
              height={120}
              className="w-full h-full object-cover"
            />
            {useWebcam && <video ref={videoRef} className="hidden" playsInline muted />}

            {/* Motion Overlay */}
            <div className="absolute top-2 left-2 bg-neutral-950/80 px-2 py-0.5 rounded text-[10px] font-mono text-neutral-200 border border-neutral-700">
              Score: <strong className="text-emerald-400">{motionScore}%</strong> (しきい値: {motionThreshold}%)
            </div>

            {motionScore >= motionThreshold && (
              <div className="absolute bottom-2 right-2 bg-emerald-500/90 text-black text-[9px] font-bold px-2 py-0.5 rounded animate-pulse">
                動体検知 TRIGGER!
              </div>
            )}
          </div>

          {/* Motion Bar */}
          <div className="flex flex-col gap-1">
            <div className="w-full bg-neutral-800 rounded-full h-2 overflow-hidden relative">
              <div
                className={`h-full transition-all duration-100 ${
                  motionScore >= motionThreshold ? "bg-emerald-400" : "bg-sky-500"
                }`}
                style={{ width: `${Math.min(100, motionScore)}%` }}
              />
              <div
                className="absolute top-0 bottom-0 w-0.5 bg-red-400"
                style={{ left: `${motionThreshold}%` }}
                title={`Threshold: ${motionThreshold}%`}
              />
            </div>
            <div className="flex justify-between text-[10px] text-neutral-400">
              <span>動体なし (0%)</span>
              <span>しきい値: {motionThreshold}%</span>
              <span>高活動 (100%)</span>
            </div>
          </div>
        </div>

        {/* 2. Audio & VAD (Voice Activity Detection) Panel */}
        <div className="bg-neutral-950 rounded-xl p-3.5 border border-neutral-800 flex flex-col justify-between gap-2.5">
          <div className="flex items-center justify-between text-xs text-neutral-300">
            <span className="font-medium flex items-center gap-1.5">
              <Mic className="w-3.5 h-3.5 text-cyan-400" />
              ES7210 デュアルマイク & VAD判定
            </span>
            <button
              onClick={() => setMicActive(!micActive)}
              className={`px-2.5 py-1 rounded-md text-xs font-medium flex items-center gap-1 transition-all ${
                micActive
                  ? "bg-rose-500/20 text-rose-300 border border-rose-500/50"
                  : "bg-cyan-500/20 text-cyan-300 border border-cyan-500/50 hover:bg-cyan-500/30"
              }`}
            >
              {micActive ? <MicOff className="w-3 h-3" /> : <Mic className="w-3 h-3" />}
              {micActive ? "マイク停止" : "マイク起動"}
            </button>
          </div>

          {/* VU Meter & Level */}
          <div className="bg-neutral-900 rounded-lg p-3 border border-neutral-800 flex flex-col gap-2">
            <div className="flex items-center justify-between text-[11px] text-neutral-400">
              <span>リアルタイム音量 RMS (VAD)</span>
              <span className="font-mono text-cyan-400">
                {(audioInputLevel * 100).toFixed(0)}%
              </span>
            </div>

            {/* Level Bars */}
            <div className="flex gap-1 h-6 items-end bg-neutral-950 p-1 rounded border border-neutral-800">
              {Array.from({ length: 16 }).map((_, i) => {
                const threshold = (i + 1) / 16;
                const active = audioInputLevel >= threshold;
                return (
                  <div
                    key={i}
                    className={`flex-1 rounded-sm transition-all duration-75 ${
                      active
                        ? i > 12
                          ? "bg-rose-500"
                          : i > 8
                          ? "bg-amber-400"
                          : "bg-emerald-400"
                        : "bg-neutral-800"
                    }`}
                    style={{ height: `${((i + 1) / 16) * 100}%` }}
                  />
                );
              })}
            </div>

            {/* Silence timer status */}
            <div className="flex items-center justify-between text-[11px] pt-1">
              <span className="text-neutral-400">無音タイマー:</span>
              <span
                className={`font-mono font-bold ${
                  silenceCounter >= silenceTimeoutSec - 2
                    ? "text-rose-400 animate-pulse"
                    : "text-neutral-300"
                }`}
              >
                {silenceCounter}秒 / {silenceTimeoutSec}秒
              </span>
            </div>
          </div>

          {/* Live Transcript / Wake Word Info */}
          <div className="text-[11px] bg-neutral-900/60 p-2 rounded border border-neutral-800/80">
            <div className="text-neutral-400 flex items-center justify-between">
              <span>ウェイクワード設定:</span>
              <strong className="text-cyan-300 font-mono">「{wakeWord}」</strong>
            </div>
            {transcript && (
              <p className="mt-1 text-emerald-300 font-mono truncate">
                🗣️ &ldquo;{transcript}&rdquo;
              </p>
            )}
          </div>
        </div>
      </div>

      {/* Manual Prompt Input & Test Buttons */}
      <div className="flex flex-col gap-2 pt-2 border-t border-neutral-800">
        <span className="text-xs font-medium text-neutral-300 flex items-center gap-1.5">
          <Sparkles className="w-3.5 h-3.5 text-amber-400" />
          クイック発話テスト (クリックで感情と音声をテスト)
        </span>
        <div className="flex flex-wrap gap-1.5">
          {[
            { label: "👋 こんにちは！", msg: "こんにちは！元気？" },
            { label: "✨ びっくりさせて", msg: "わっ！すごいニュースがあったよ！" },
            { label: "🤔 難しい計算して", msg: "123かける456の答えを教えて！" },
            { label: "😢 落ち込んでるの", msg: "今日失敗しちゃって落ち込んでるんだ..." },
            { label: "😠 ちょっと怒って", msg: "片付けサボっちゃった！怒って！" },
            { label: "🌙 おやすみ", msg: "もう寝るね、おやすみなさい！" },
          ].map((preset, idx) => (
            <button
              key={idx}
              onClick={() => onSendMessage(preset.msg)}
              className="text-xs bg-neutral-800 hover:bg-neutral-700 active:scale-95 text-neutral-200 px-3 py-1.5 rounded-lg border border-neutral-700/80 transition-all cursor-pointer flex items-center gap-1"
            >
              {preset.label}
            </button>
          ))}
        </div>

        {/* Text Input for custom speech */}
        <form
          onSubmit={(e) => {
            e.preventDefault();
            if (customInput.trim()) {
              onSendMessage(customInput.trim());
              setCustomInput("");
            }
          }}
          className="flex gap-2 mt-1"
        >
          <input
            type="text"
            value={customInput}
            onChange={(e) => setCustomInput(e.target.value)}
            placeholder="テキストで話しかける (例: 「今日の天気を教えて」)"
            className="flex-1 bg-neutral-950 border border-neutral-700 rounded-lg px-3 py-2 text-xs text-neutral-100 placeholder:text-neutral-500 focus:outline-none focus:border-cyan-500"
          />
          <button
            type="submit"
            className="bg-cyan-600 hover:bg-cyan-500 active:scale-95 text-white px-4 py-2 rounded-lg text-xs font-medium flex items-center gap-1.5 transition-all shadow cursor-pointer"
          >
            <Send className="w-3.5 h-3.5" />
            話しかける
          </button>
        </form>
      </div>
    </div>
  );
};
