"use client";

import React, { useEffect, useRef } from "react";

export type EmotionType =
  | "NORMAL"
  | "HAPPY"
  | "SAD"
  | "ANGRY"
  | "SURPRISED"
  | "THINKING"
  | "TALKING"
  | "SLEEP";

interface StackChanFaceProps {
  emotion: EmotionType;
  lipSyncLevel?: number; // 0.0 ~ 1.0
  isSpeaking?: boolean;
  scale?: number;
  width?: number;
  height?: number;
  batteryLevel?: number;
  wifiConnected?: boolean;
  showOsd?: boolean;
  onFaceClick?: () => void;
}

export const StackChanFace: React.FC<StackChanFaceProps> = ({
  emotion,
  lipSyncLevel = 0,
  isSpeaking = false,
  width = 320,
  height = 240,
  batteryLevel = 92,
  wifiConnected = true,
  showOsd = true,
  onFaceClick,
}) => {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  // Animation states
  const stateRef = useRef({
    eyeOpen: 1.0,
    isBlinking: false,
    blinkStartTime: 0,
    lastBlinkTime: 0,
    nextBlinkInterval: 3000,
    mouthOpen: 0.0,
    eyebrowAngle: 0.0,
    eyebrowY: 0.0,
    gazeX: 0.0,
    gazeY: 0.0,
    sleepZzzPhase: 0,
  });

  useEffect(() => {
    stateRef.current.lastBlinkTime = Date.now();
    let animId: number;
    const canvas = canvasRef.current;
    if (!canvas) return;
    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    const render = () => {
      const now = Date.now();
      const st = stateRef.current;

      // 1. Blinking physics
      if (emotion !== "SLEEP") {
        if (!st.isBlinking && now - st.lastBlinkTime > st.nextBlinkInterval) {
          st.isBlinking = true;
          st.blinkStartTime = now;
          st.nextBlinkInterval = 2500 + Math.random() * 3000;
        }

        if (st.isBlinking) {
          const elapsed = now - st.blinkStartTime;
          if (elapsed < 100) {
            st.eyeOpen = 1.0 - elapsed / 100;
          } else if (elapsed < 200) {
            st.eyeOpen = (elapsed - 100) / 100;
          } else {
            st.eyeOpen = 1.0;
            st.isBlinking = false;
            st.lastBlinkTime = now;
          }
        }
      } else {
        st.eyeOpen = 0.0;
      }

      // 2. Lip sync smoothing
      const targetMouth = isSpeaking
        ? Math.max(lipSyncLevel, 0.15 + Math.sin(now * 0.015) * 0.25)
        : lipSyncLevel;
      st.mouthOpen += (targetMouth - st.mouthOpen) * 0.35;

      // 3. Eyebrow & gaze targets
      let targetBrowAngle = 0;
      let targetBrowY = 0;
      let targetGazeX = 0;
      let targetGazeY = 0;

      switch (emotion) {
        case "HAPPY":
          targetBrowAngle = -16;
          targetBrowY = -6;
          break;
        case "SAD":
          targetBrowAngle = 22;
          targetBrowY = 8;
          break;
        case "ANGRY":
          targetBrowAngle = -30;
          targetBrowY = 10;
          break;
        case "SURPRISED":
          targetBrowAngle = 0;
          targetBrowY = -14;
          break;
        case "THINKING":
          targetBrowAngle = 12;
          targetBrowY = -4;
          targetGazeX = 0.5 + Math.sin(now * 0.003) * 0.2;
          targetGazeY = -0.5;
          break;
        case "SLEEP":
          targetBrowAngle = 4;
          targetBrowY = 2;
          break;
        default:
          targetBrowAngle = 0;
          targetBrowY = 0;
          targetGazeX = Math.sin(now * 0.001) * 0.15;
          targetGazeY = 0;
          break;
      }

      st.eyebrowAngle += (targetBrowAngle - st.eyebrowAngle) * 0.18;
      st.eyebrowY += (targetBrowY - st.eyebrowY) * 0.18;
      st.gazeX += (targetGazeX - st.gazeX) * 0.15;
      st.gazeY += (targetGazeY - st.gazeY) * 0.15;
      st.sleepZzzPhase = (now % 3000) / 3000;

      // Clear Screen (M5Stack CoreS3 dark cyberpunk background)
      ctx.fillStyle = "#111827"; // Deep slate / obsidian
      ctx.fillRect(0, 0, width, height);

      // Coordinates
      const lx = 100 + st.gazeX * 10;
      const rx = 220 + st.gazeX * 10;
      const cy = 115 + st.gazeY * 8;
      const eyeColor = "#FFFFFF";

      // 4. Draw Eyebrows (Hide during SLEEP to prevent extra horizontal lines)
      if (emotion !== "SLEEP") {
        ctx.fillStyle = eyeColor;
        const browLen = 38;
        const browThick = 5.5;
        const browBaseY = 66 + st.eyebrowY;

        // Left eyebrow
        ctx.save();
        ctx.translate(lx, browBaseY);
        ctx.rotate((st.eyebrowAngle * Math.PI) / 180);
        ctx.beginPath();
        ctx.roundRect(-browLen / 2, -browThick / 2, browLen, browThick, 3);
        ctx.fill();
        ctx.restore();

        // Right eyebrow
        ctx.save();
        ctx.translate(rx, browBaseY);
        ctx.rotate((-st.eyebrowAngle * Math.PI) / 180);
        ctx.beginPath();
        ctx.roundRect(-browLen / 2, -browThick / 2, browLen, browThick, 3);
        ctx.fill();
        ctx.restore();
      }

      // 5. Draw Eyes
      if (emotion === "HAPPY") {
        // Happy crescent curved eyes
        ctx.strokeStyle = eyeColor;
        ctx.lineWidth = 6;
        ctx.lineCap = "round";

        // Left eye arc
        ctx.beginPath();
        ctx.arc(lx, cy + 8, 22, Math.PI * 1.15, Math.PI * 1.85, false);
        ctx.stroke();

        // Right eye arc
        ctx.beginPath();
        ctx.arc(rx, cy + 8, 22, Math.PI * 1.15, Math.PI * 1.85, false);
        ctx.stroke();

        // Cute pink blush cheeks
        ctx.fillStyle = "rgba(251, 113, 133, 0.7)"; // rosy pink
        ctx.beginPath();
        ctx.arc(lx - 28, cy + 24, 10, 0, Math.PI * 2);
        ctx.fill();
        ctx.beginPath();
        ctx.arc(rx + 28, cy + 24, 10, 0, Math.PI * 2);
        ctx.fill();
      } else if (emotion === "SLEEP" || st.eyeOpen <= 0.05) {
        // Closed eyes with soft cute downward arch curve
        ctx.strokeStyle = eyeColor;
        ctx.lineWidth = 5;
        ctx.lineCap = "round";

        ctx.beginPath();
        ctx.arc(lx, cy + 6, 18, Math.PI * 1.15, Math.PI * 1.85, false);
        ctx.stroke();

        ctx.beginPath();
        ctx.arc(rx, cy + 6, 18, Math.PI * 1.15, Math.PI * 1.85, false);
        ctx.stroke();

        // Sleep Zzz floating particles
        if (emotion === "SLEEP") {
          ctx.fillStyle = "#60A5FA";
          ctx.font = "bold 14px monospace";
          const z1Y = 70 - st.sleepZzzPhase * 40;
          const z1X = 250 + Math.sin(st.sleepZzzPhase * 6) * 8;
          ctx.globalAlpha = 1 - st.sleepZzzPhase;
          ctx.fillText("Z", z1X, z1Y);
          ctx.font = "bold 11px monospace";
          ctx.fillText("z", z1X - 12, z1Y + 14);
          ctx.globalAlpha = 1.0;
        }
      } else {
        // Standard pill / oval eyes
        let radX = 18;
        let radY = 28 * Math.max(0.08, st.eyeOpen);

        if (emotion === "SURPRISED") {
          radX = 23;
          radY = 33;
        }

        ctx.fillStyle = eyeColor;
        // Left eye
        ctx.beginPath();
        ctx.ellipse(lx, cy, radX, radY, 0, 0, Math.PI * 2);
        ctx.fill();

        // Right eye
        ctx.beginPath();
        ctx.ellipse(rx, cy, radX, radY, 0, 0, Math.PI * 2);
        ctx.fill();

        // Cute sparkle reflections if eyes are open
        if (st.eyeOpen > 0.4) {
          ctx.fillStyle = "#1E293B"; // dark pupil
          ctx.beginPath();
          ctx.arc(lx - 4, cy - radY * 0.25, 5, 0, Math.PI * 2);
          ctx.fill();
          ctx.beginPath();
          ctx.arc(rx - 4, cy - radY * 0.25, 5, 0, Math.PI * 2);
          ctx.fill();

          ctx.fillStyle = "#FFFFFF"; // catchlight
          ctx.beginPath();
          ctx.arc(lx - 2, cy - radY * 0.3, 2.5, 0, Math.PI * 2);
          ctx.fill();
          ctx.beginPath();
          ctx.arc(rx - 2, cy - radY * 0.3, 2.5, 0, Math.PI * 2);
          ctx.fill();
        }
      }

      // 6. Draw Mouth (No mouth during SLEEP to remove unwanted horizontal white lines)
      const mx = 160;
      const my = 182;
      const mouthColor = "#FB7185"; // rose pink

      if (emotion === "SLEEP") {
        // Do not draw mouth during SLEEP - peaceful closed face with no horizontal line
      } else if (st.mouthOpen > 0.08) {
        // Lip sync dynamic mouth
        const mw = 22 + st.mouthOpen * 24;
        const mh = 8 + st.mouthOpen * 32;

        ctx.fillStyle = mouthColor;
        ctx.beginPath();
        ctx.ellipse(mx, my, mw / 2, mh / 2, 0, 0, Math.PI * 2);
        ctx.fill();

        ctx.strokeStyle = "#FFFFFF";
        ctx.lineWidth = 2.5;
        ctx.stroke();

        // Little cute tongue
        if (st.mouthOpen > 0.35) {
          ctx.fillStyle = "#F43F5E";
          ctx.beginPath();
          ctx.ellipse(mx, my + mh * 0.2, mw * 0.3, mh * 0.2, 0, 0, Math.PI * 2);
          ctx.fill();
        }
      } else {
        // Idle mouths per emotion
        ctx.strokeStyle = "#FFFFFF";
        ctx.lineWidth = 4;
        ctx.lineCap = "round";

        if (emotion === "HAPPY") {
          ctx.beginPath();
          ctx.arc(mx, my - 6, 18, Math.PI * 0.15, Math.PI * 0.85, false);
          ctx.stroke();
        } else if (emotion === "SAD" || emotion === "ANGRY") {
          ctx.beginPath();
          ctx.arc(mx, my + 14, 18, Math.PI * 1.15, Math.PI * 1.85, false);
          ctx.stroke();
        } else if (emotion === "SURPRISED") {
          ctx.fillStyle = mouthColor;
          ctx.beginPath();
          ctx.ellipse(mx, my, 8, 12, 0, 0, Math.PI * 2);
          ctx.fill();
          ctx.stroke();
        } else {
          // Normal smile line
          ctx.beginPath();
          ctx.roundRect(mx - 12, my - 2, 24, 4, 2);
          ctx.fillStyle = "#FFFFFF";
          ctx.fill();
        }
      }

      // 7. OSD (On-Screen Display: Battery, Wifi, State)
      if (showOsd) {
        ctx.fillStyle = "rgba(148, 163, 184, 0.8)";
        ctx.font = "10px monospace";
        ctx.textAlign = "left";

        // WiFi status
        ctx.fillText(wifiConnected ? "WiFi:OK" : "WiFi:OFF", 10, 16);

        // Battery
        ctx.textAlign = "right";
        ctx.fillText(`${batteryLevel}% [■■■]`, width - 10, 16);

        // Emotion Badge
        ctx.textAlign = "center";
        ctx.font = "bold 9px sans-serif";
        ctx.fillStyle = "#38BDF8";
        ctx.fillText(`M5Stack CoreS3 • ${emotion}`, width / 2, 16);
      }

      animId = requestAnimationFrame(render);
    };

    animId = requestAnimationFrame(render);
    return () => cancelAnimationFrame(animId);
  }, [emotion, isSpeaking, lipSyncLevel, width, height, batteryLevel, wifiConnected, showOsd]);

  return (
    <div
      onClick={onFaceClick}
      className="relative rounded-lg overflow-hidden shadow-inner cursor-pointer select-none group"
      style={{ width: `${width}px`, height: `${height}px` }}
      title="クリック/タップしてスタックちゃんを起こす・インタラクト"
    >
      <canvas
        ref={canvasRef}
        width={width}
        height={height}
        className="w-full h-full block"
      />
      <div className="absolute inset-0 bg-cyan-500/0 group-hover:bg-cyan-500/5 transition-colors pointer-events-none" />
    </div>
  );
};
