"use client";

import React, { useEffect, useState } from "react";
import { EmotionType, StackChanFace } from "./StackChanFace";
import { Mic, Volume2, Video, Power, Wifi, Cpu, Rotate3D } from "lucide-react";

interface CoreS3HardwareFrameProps {
  emotion: EmotionType;
  lipSyncLevel: number;
  isSpeaking: boolean;
  audioInputLevel?: number; // 0.0 ~ 1.0 mic RMS
  isCameraActive?: boolean;
  appState: string;
  servoEnabled?: boolean;
  onScreenClick?: () => void;
  onPowerClick?: () => void;
}

export const CoreS3HardwareFrame: React.FC<CoreS3HardwareFrameProps> = ({
  emotion,
  lipSyncLevel,
  isSpeaking,
  audioInputLevel = 0,
  isCameraActive = true,
  appState,
  servoEnabled = true,
  onScreenClick,
  onPowerClick,
}) => {
  // Real-time simulated Port A SG90 2-axis servo angles (Degrees)
  let panAngle = 90;
  let tiltAngle = 90;

  if (servoEnabled) {
    switch (emotion) {
      case "HAPPY":
        panAngle = 90;
        tiltAngle = 100; // gentle nod
        break;
      case "THINKING":
        panAngle = 104; // head tilt right
        tiltAngle = 102; // look up
        break;
      case "SURPRISED":
        panAngle = 90;
        tiltAngle = 112; // head raised alertly
        break;
      case "ANGRY":
        panAngle = 76; // shaking head
        tiltAngle = 90;
        break;
      case "SAD":
        panAngle = 90;
        tiltAngle = 74; // head down
        break;
      case "SLEEP":
        panAngle = 90;
        tiltAngle = 72; // head slumped down
        break;
      default:
        panAngle = 90;
        tiltAngle = 90;
        break;
    }

    // Add micro-nod during speaking
    if (isSpeaking && lipSyncLevel > 0.1) {
      tiltAngle += Math.round(lipSyncLevel * 6);
    }
  }

  // Convert SG90 90-degree center to CSS 3D perspective rotation (-12deg to +12deg)
  const cssRotY = (panAngle - 90) * 0.7;
  const cssRotX = -(tiltAngle - 90) * 0.6;

  return (
    <div className="flex flex-col items-center select-none" style={{ perspective: "1000px" }}>
      {/* Port A SG90 Servo Status HUD */}
      <div className="mb-2.5 px-3 py-1 rounded-full bg-neutral-900/90 border border-neutral-700/80 flex items-center gap-3 text-[10px] font-mono shadow-md backdrop-blur-sm">
        <div className="flex items-center gap-1 text-cyan-300 font-bold">
          <Rotate3D className="w-3.5 h-3.5 text-cyan-400 animate-spin" style={{ animationDuration: "8s" }} />
          PORT A: SG90 × 2軸
        </div>
        <div className="flex items-center gap-2 text-neutral-300">
          <span>
            PAN (G2): <strong className="text-emerald-400">{panAngle}°</strong>
          </span>
          <span>•</span>
          <span>
            TILT (G1): <strong className="text-sky-400">{tiltAngle}°</strong>
          </span>
        </div>
      </div>

      {/* Physical CoreS3 Chassis with 3D Rotation from SG90 Servos */}
      <div
        className="relative w-[380px] bg-neutral-900 rounded-3xl p-5 shadow-[0_25px_60px_-15px_rgba(0,0,0,0.8),0_0_0_1px_rgba(255,255,255,0.1)] border-2 border-neutral-700/80 transition-transform duration-300 ease-out"
        style={{
          transform: `rotateY(${cssRotY}deg) rotateX(${cssRotX}deg)`,
          transformOrigin: "center bottom",
        }}
      >
        {/* Top Bezel: Camera Lens & Microphones */}
        <div className="flex items-center justify-between px-4 pb-3">
          {/* Left ES7210 Mic Grille */}
          <div className="flex items-center gap-1.5" title="ES7210 Left Mic">
            <div
              className={`w-2.5 h-2.5 rounded-full border border-neutral-600 transition-colors ${
                audioInputLevel > 0.08 ? "bg-emerald-400 shadow-[0_0_8px_#34d399]" : "bg-neutral-800"
              }`}
            />
            <span className="text-[9px] font-mono tracking-tighter text-neutral-400">MIC L</span>
          </div>

          {/* Center GC0308 Camera Lens */}
          <div className="flex items-center gap-2 bg-neutral-950/80 px-3 py-1 rounded-full border border-neutral-800">
            <div className="relative w-4 h-4 rounded-full bg-neutral-800 flex items-center justify-center border border-neutral-600">
              <div className="w-2 h-2 rounded-full bg-sky-950 border border-sky-500/80">
                <div className="w-1 h-1 rounded-full bg-cyan-300" />
              </div>
              {isCameraActive && (
                <div className="absolute -top-1 -right-1 w-1.5 h-1.5 rounded-full bg-emerald-400 animate-ping" />
              )}
            </div>
            <span className="text-[9px] font-mono font-medium text-neutral-300 tracking-wider">
              GC0308 CAM
            </span>
          </div>

          {/* Right ES7210 Mic Grille */}
          <div className="flex items-center gap-1.5" title="ES7210 Right Mic">
            <span className="text-[9px] font-mono tracking-tighter text-neutral-400">MIC R</span>
            <div
              className={`w-2.5 h-2.5 rounded-full border border-neutral-600 transition-colors ${
                audioInputLevel > 0.08 ? "bg-emerald-400 shadow-[0_0_8px_#34d399]" : "bg-neutral-800"
              }`}
            />
          </div>
        </div>

        {/* 2.0" IPS LCD Display (320x240 Native) */}
        <div className="relative rounded-xl overflow-hidden border-2 border-neutral-800 bg-black shadow-inner flex justify-center items-center">
          <StackChanFace
            emotion={emotion}
            lipSyncLevel={lipSyncLevel}
            isSpeaking={isSpeaking}
            width={320}
            height={240}
            onFaceClick={onScreenClick}
          />
        </div>

        {/* Bottom Bezel: M5 Button, Speaker & Status Indicators */}
        <div className="flex items-center justify-between px-2 pt-3.5">
          {/* M5 Branding */}
          <div className="flex items-center gap-2">
            <div
              onClick={onPowerClick}
              role="button"
              className="w-7 h-7 rounded-lg bg-red-600 hover:bg-red-500 active:scale-95 transition-all flex items-center justify-center shadow-md cursor-pointer border border-red-400/40"
              title="M5 Button / Wake trigger"
            >
              <span className="text-[10px] font-black tracking-tight text-white">M5</span>
            </div>
            <div className="flex flex-col">
              <span className="text-[11px] font-bold tracking-tight text-neutral-200">
                CoreS3 Lite
              </span>
              <span className="text-[9px] font-mono text-neutral-500">
                ESP32-S3FN8 • 8MB PSRAM
              </span>
            </div>
          </div>

          {/* Speaker Sound Slots (AW88298) */}
          <div className="flex items-center gap-1 bg-neutral-950/80 px-2.5 py-1 rounded-full border border-neutral-800">
            <Volume2
              className={`w-3.5 h-3.5 transition-colors ${
                isSpeaking ? "text-cyan-400 animate-pulse" : "text-neutral-500"
              }`}
            />
            <div className="flex gap-0.5 items-end h-3">
              {[0.4, 0.8, 0.6, 1.0, 0.5].map((h, i) => (
                <div
                  key={i}
                  className={`w-0.5 rounded-full transition-all duration-75 ${
                    isSpeaking ? "bg-cyan-400" : "bg-neutral-700"
                  }`}
                  style={{
                    height: isSpeaking ? `${Math.max(3, lipSyncLevel * 12 * h)}px` : "3px",
                  }}
                />
              ))}
            </div>
          </div>
        </div>

        {/* Hardware Status Tag */}
        <div className="mt-3 pt-2.5 border-t border-neutral-800/80 flex items-center justify-between text-[10px] font-mono text-neutral-400 px-1">
          <span className="flex items-center gap-1.5">
            <span
              className={`w-2 h-2 rounded-full ${
                appState === "SLEEP"
                  ? "bg-amber-500 animate-pulse"
                  : appState === "THINKING"
                  ? "bg-purple-500 animate-ping"
                  : appState === "SPEAKING"
                  ? "bg-cyan-400"
                  : "bg-emerald-400"
              }`}
            />
            STATUS: <strong className="text-neutral-200">{appState}</strong>
          </span>
          <span className="text-cyan-400 font-semibold">Port A: G2(Pan) G1(Tilt)</span>
        </div>
      </div>

      <p className="mt-2 text-xs text-neutral-400 flex items-center gap-1">
        💡 感情や音声に合わせて Port A (G2/G1) SG90 2軸サーボが実機同様に動きます
      </p>
    </div>
  );
};
