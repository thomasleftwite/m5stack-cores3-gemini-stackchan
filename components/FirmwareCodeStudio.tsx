"use client";

import React, { useState } from "react";
import { FIRMWARE_FILES, FirmwareFile } from "@/lib/firmware-sources";
import {
  Code,
  Download,
  Copy,
  Check,
  FileCode,
  FolderArchive,
  Settings,
  Sparkles,
  Info,
  Terminal,
} from "lucide-react";
import JSZip from "jszip";

interface FirmwareCodeStudioProps {
  apiKeyParam: string;
  wifiSsidParam: string;
  wifiPassParam: string;
  ttsVoiceParam: string;
  wakeWordParam: string;
  silenceSecParam: number;
  motionThreshParam: number;
  servoEnabledParam?: boolean;
  servoPanPinParam?: number;
  servoTiltPinParam?: number;
  servoPanCenterParam?: number;
  servoTiltCenterParam?: number;
}

export const FirmwareCodeStudio: React.FC<FirmwareCodeStudioProps> = ({
  apiKeyParam,
  wifiSsidParam,
  wifiPassParam,
  ttsVoiceParam,
  wakeWordParam,
  silenceSecParam,
  motionThreshParam,
  servoEnabledParam = true,
  servoPanPinParam = 2,
  servoTiltPinParam = 1,
  servoPanCenterParam = 90,
  servoTiltCenterParam = 90,
}) => {
  const [selectedFileIdx, setSelectedFileIdx] = useState(0);
  const [copied, setCopied] = useState(false);
  const [isZipping, setIsZipping] = useState(false);

  // Generate dynamic config.json content reflecting current props
  const currentConfigContent = `{
  "wifi_ssid": "${wifiSsidParam || "YOUR_WIFI_SSID"}",
  "wifi_password": "${wifiPassParam || "YOUR_WIFI_PASSWORD"}",
  "gemini_api_key": "${apiKeyParam || "AIzaSyYOUR_GEMINI_API_KEY"}",
  "gemini_model": "gemini-3.8-flash",
  "tts_voice": "${ttsVoiceParam}",
  "wake_word": "${wakeWordParam}",
  "silence_timeout_sec": ${silenceSecParam},
  "camera_motion_threshold": ${motionThreshParam},
  "spk_volume": 160,
  "mic_gain": 80,
  "servo_enabled": ${servoEnabledParam},
  "servo_pan_pin": ${servoPanPinParam},
  "servo_tilt_pin": ${servoTiltPinParam},
  "servo_pan_center": ${servoPanCenterParam},
  "servo_tilt_center": ${servoTiltCenterParam}
}`;

  const files = FIRMWARE_FILES.map((f) => {
    if (f.name === "config.json") {
      return { ...f, content: currentConfigContent };
    }
    return f;
  });

  const selectedFile = files[selectedFileIdx] || files[0];

  const handleCopy = () => {
    navigator.clipboard.writeText(selectedFile.content);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleDownloadZip = async () => {
    setIsZipping(true);
    try {
      const zip = new JSZip();

      // Add each file to its respective relative path (includes comprehensive README.md)
      files.forEach((f) => {
        zip.file(f.path, f.content);
      });

      const blob = await zip.generateAsync({ type: "blob" });
      const url = URL.createObjectURL(blob);
      const a = document.createElement("a");
      a.href = url;
      a.download = "m5stack-cores3-gemini-stackchan.zip";
      a.click();
      URL.revokeObjectURL(url);
    } catch (e) {
      console.error("ZIP creation failed:", e);
    } finally {
      setIsZipping(false);
    }
  };

  return (
    <div className="bg-neutral-900 border border-neutral-800 rounded-2xl overflow-hidden shadow-2xl flex flex-col">
      {/* Studio Header */}
      <div className="bg-neutral-950 px-5 py-3.5 border-b border-neutral-800 flex flex-wrap items-center justify-between gap-3">
        <div className="flex items-center gap-2.5">
          <FileCode className="w-5 h-5 text-cyan-400" />
          <div>
            <h2 className="text-sm font-bold text-neutral-100 flex items-center gap-2">
              M5Stack CoreS3 Lite ファームウェア コードスタジオ
              <span className="text-[10px] bg-cyan-950 text-cyan-300 border border-cyan-800 px-2 py-0.5 rounded-full font-mono">
                PlatformIO • Arduino C++ • FreeRTOS
              </span>
            </h2>
            <p className="text-[11px] text-neutral-400">
              全ソースコードはそのままコンパイル・書き込み可能な本番仕様です
            </p>
          </div>
        </div>

        {/* Actions */}
        <div className="flex items-center gap-2">
          <button
            onClick={handleCopy}
            className="px-3 py-1.5 rounded-lg bg-neutral-800 hover:bg-neutral-700 text-neutral-200 text-xs font-medium flex items-center gap-1.5 border border-neutral-700 transition cursor-pointer"
          >
            {copied ? <Check className="w-3.5 h-3.5 text-emerald-400" /> : <Copy className="w-3.5 h-3.5" />}
            {copied ? "コピー完了！" : "ファイルコピー"}
          </button>
          <button
            onClick={handleDownloadZip}
            disabled={isZipping}
            className="px-3.5 py-1.5 rounded-lg bg-cyan-600 hover:bg-cyan-500 active:scale-95 text-white text-xs font-semibold flex items-center gap-1.5 shadow-md transition cursor-pointer disabled:opacity-50"
          >
            <Download className="w-3.5 h-3.5" />
            {isZipping ? "ZIP生成中..." : "プロジェクト一括ダウンロード (ZIP)"}
          </button>
        </div>
      </div>

      {/* Main Studio Body: File Nav + Code View */}
      <div className="grid grid-cols-1 md:grid-cols-12 min-h-[500px]">
        {/* Left: File Tree (4 cols) */}
        <div className="md:col-span-4 bg-neutral-950/70 border-r border-neutral-800 p-3 flex flex-col gap-1 overflow-y-auto">
          <span className="text-[11px] font-mono text-neutral-500 uppercase px-2 py-1">
            PROJECT FILES
          </span>
          {files.map((file, idx) => (
            <button
              key={file.path}
              onClick={() => setSelectedFileIdx(idx)}
              className={`w-full text-left px-3 py-2 rounded-lg text-xs font-mono transition-all flex items-center justify-between cursor-pointer ${
                selectedFileIdx === idx
                  ? "bg-cyan-950/60 text-cyan-300 border border-cyan-800/80 font-bold"
                  : "text-neutral-400 hover:text-neutral-200 hover:bg-neutral-800/60"
              }`}
            >
              <div className="flex items-center gap-2 truncate">
                <span
                  className={`w-1.5 h-1.5 rounded-full ${
                    file.name.endsWith(".ini")
                      ? "bg-purple-400"
                      : file.name.endsWith(".json")
                      ? "bg-amber-400"
                      : file.name.endsWith(".h")
                      ? "bg-sky-400"
                      : "bg-emerald-400"
                  }`}
                />
                <span className="truncate">{file.path}</span>
              </div>
              <span className="text-[10px] text-neutral-500 font-sans">
                {file.language}
              </span>
            </button>
          ))}

          {/* Quick Build Tips */}
          <div className="mt-4 p-3 bg-neutral-900 rounded-xl border border-neutral-800/80 text-[11px] text-neutral-300 flex flex-col gap-2">
            <span className="font-semibold text-neutral-200 flex items-center gap-1.5">
              <Terminal className="w-3.5 h-3.5 text-cyan-400" />
              ビルドコマンド (PlatformIO CLI)
            </span>
            <div className="bg-black p-2 rounded font-mono text-[10px] text-emerald-400 select-all">
              pio run -t uploadfs<br />
              pio run -t upload
            </div>
            <p className="text-[10px] text-neutral-400">
              ※最初に <code>uploadfs</code> で <code>config.json</code> をLittleFSに転送してください。
            </p>
          </div>
        </div>

        {/* Right: Code Viewer (8 cols) */}
        <div className="md:col-span-8 bg-neutral-900/90 flex flex-col">
          {/* File Tab Meta */}
          <div className="bg-neutral-950/40 px-4 py-2 border-b border-neutral-800 flex items-center justify-between text-xs font-mono text-neutral-400">
            <span className="text-cyan-400">{selectedFile.path}</span>
            <span className="text-neutral-500">{selectedFile.description}</span>
          </div>

          {/* Code Text Area */}
          <div className="flex-1 p-4 overflow-x-auto font-mono text-xs text-neutral-200 bg-[#0d1117] leading-relaxed select-text">
            <pre className="whitespace-pre">{selectedFile.content}</pre>
          </div>
        </div>
      </div>
    </div>
  );
};
