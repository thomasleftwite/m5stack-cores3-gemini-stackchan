"use client";

import React from "react";
import {
  Settings,
  Wifi,
  Mic,
  Volume2,
  Video,
  Clock,
  Bot,
  RefreshCw,
  Rotate3D,
  Sliders,
} from "lucide-react";

interface SettingsPanelProps {
  geminiApiKey: string;
  setGeminiApiKey: (v: string) => void;
  geminiModel: string;
  setGeminiModel: (v: string) => void;
  ttsVoice: string;
  setTtsVoice: (v: string) => void;
  wakeWord: string;
  setWakeWord: (v: string) => void;
  silenceTimeoutSec: number;
  setSilenceTimeoutSec: (v: number) => void;
  motionThreshold: number;
  setMotionThreshold: (v: number) => void;
  servoEnabled: boolean;
  setServoEnabled: (v: boolean) => void;
  servoPanPin: number;
  setServoPanPin: (v: number) => void;
  servoTiltPin: number;
  setServoTiltPin: (v: number) => void;
  servoPanCenter: number;
  setServoPanCenter: (v: number) => void;
  servoTiltCenter: number;
  setServoTiltCenter: (v: number) => void;
  onResetDefaults: () => void;
}

export const SettingsPanel: React.FC<SettingsPanelProps> = ({
  geminiApiKey,
  setGeminiApiKey,
  geminiModel,
  setGeminiModel,
  ttsVoice,
  setTtsVoice,
  wakeWord,
  setWakeWord,
  silenceTimeoutSec,
  setSilenceTimeoutSec,
  motionThreshold,
  setMotionThreshold,
  servoEnabled,
  setServoEnabled,
  servoPanPin,
  setServoPanPin,
  servoTiltPin,
  setServoTiltPin,
  servoPanCenter,
  setServoPanCenter,
  servoTiltCenter,
  setServoTiltCenter,
  onResetDefaults,
}) => {
  return (
    <div className="bg-neutral-900 border border-neutral-800 rounded-2xl p-5 shadow-xl flex flex-col gap-5">
      <div className="flex items-center justify-between pb-3 border-b border-neutral-800">
        <div className="flex items-center gap-2">
          <Settings className="w-5 h-5 text-cyan-400" />
          <h3 className="font-semibold text-neutral-100 text-sm">
            CoreS3 Lite / スタックチャン 動作パラメータ設定 (config.json)
          </h3>
        </div>
        <button
          onClick={onResetDefaults}
          className="text-xs text-neutral-400 hover:text-neutral-200 flex items-center gap-1 cursor-pointer transition"
          title="初期値に戻す"
        >
          <RefreshCw className="w-3.5 h-3.5" />
          デフォルト復帰
        </button>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4 text-xs">
        <div className="flex flex-col gap-1.5 bg-neutral-950 p-3 rounded-xl border border-cyan-800 md:col-span-2 lg:col-span-3">
          <span className="text-cyan-300 flex items-center gap-1.5 font-medium">
            <Wifi className="w-3.5 h-3.5" />
            WiFiは本体からスマホで設定
          </span>
          <p className="text-neutral-300 leading-relaxed">
            初回または接続失敗時、本体が「StackChan-Setup」を起動します。
            スマホをこのWiFiに接続し、ブラウザで http://192.168.4.1 を開いて接続先を保存してください。
            接続先を変更するときは、再起動直後の5秒以内に本体画面を3秒間押し続けます。
          </p>
        </div>

        {/* Gemini API Key */}
        <div className="flex flex-col gap-1.5 bg-neutral-950 p-3 rounded-xl border border-neutral-800">
          <label className="text-neutral-400 flex items-center gap-1.5 font-medium">
            <Bot className="w-3.5 h-3.5 text-amber-400" />
            Gemini API キー (config.json保存用)
          </label>
          <input
            type="password"
            value={geminiApiKey}
            onChange={(e) => setGeminiApiKey(e.target.value)}
            placeholder="AIzaSy..."
            className="bg-neutral-900 border border-neutral-700 rounded-lg px-2.5 py-1.5 text-neutral-100 font-mono focus:outline-none focus:border-cyan-500"
          />
        </div>

        {/* TTS Voice */}
        <div className="flex flex-col gap-1.5 bg-neutral-950 p-3 rounded-xl border border-neutral-800">
          <label className="text-neutral-400 flex items-center gap-1.5 font-medium">
            <Volume2 className="w-3.5 h-3.5 text-emerald-400" />
            Gemini TTS 音声モデル (gemini-3.8-flash-lite-tts)
          </label>
          <select
            value={ttsVoice}
            onChange={(e) => setTtsVoice(e.target.value)}
            className="bg-neutral-900 border border-neutral-700 rounded-lg px-2.5 py-1.5 text-neutral-100 font-mono focus:outline-none focus:border-cyan-500 cursor-pointer"
          >
            <option value="Kore">Kore (落ち着いた明るい声 / デフォルト推奨)</option>
            <option value="Puck">Puck (元気で親しみやすい若々しい声)</option>
            <option value="Fenrir">Fenrir (深みのある落ち着いた声)</option>
            <option value="Zephyr">Zephyr (柔らかくクリアなトーン)</option>
            <option value="Charon">Charon (重厚で温かみのある声)</option>
          </select>
        </div>

        {/* Wake Word */}
        <div className="flex flex-col gap-1.5 bg-neutral-950 p-3 rounded-xl border border-neutral-800">
          <label className="text-neutral-400 flex items-center gap-1.5 font-medium">
            <Mic className="w-3.5 h-3.5 text-purple-400" />
            ウェイクワード (対話開始キーワード)
          </label>
          <input
            type="text"
            value={wakeWord}
            onChange={(e) => setWakeWord(e.target.value)}
            placeholder="スタックちゃん"
            className="bg-neutral-900 border border-neutral-700 rounded-lg px-2.5 py-1.5 text-neutral-100 font-mono focus:outline-none focus:border-cyan-500"
          />
        </div>

        {/* Silence Timeout */}
        <div className="flex flex-col gap-1.5 bg-neutral-950 p-3 rounded-xl border border-neutral-800">
          <div className="flex justify-between items-center text-neutral-400 font-medium">
            <span className="flex items-center gap-1.5">
              <Clock className="w-3.5 h-3.5 text-rose-400" />
              無音タイムアウト
            </span>
            <span className="text-rose-400 font-mono font-bold">{silenceTimeoutSec} 秒</span>
          </div>
          <input
            type="range"
            min={3}
            max={20}
            step={1}
            value={silenceTimeoutSec}
            onChange={(e) => setSilenceTimeoutSec(Number(e.target.value))}
            className="accent-rose-500 cursor-pointer"
          />
          <span className="text-[10px] text-neutral-500">
            発話が途切れてから会話終了（スリープ）へ移行するまでの時間
          </span>
        </div>

        {/* Camera Motion Sensitivity */}
        <div className="flex flex-col gap-1.5 bg-neutral-950 p-3 rounded-xl border border-neutral-800 md:col-span-2 lg:col-span-3">
          <div className="flex justify-between items-center text-neutral-400 font-medium">
            <span className="flex items-center gap-1.5">
              <Video className="w-3.5 h-3.5 text-amber-400" />
              GC0308 カメラ動体検知 感度しきい値
            </span>
            <span className="text-amber-400 font-mono font-bold">{motionThreshold} %</span>
          </div>
          <input
            type="range"
            min={10}
            max={60}
            step={1}
            value={motionThreshold}
            onChange={(e) => setMotionThreshold(Number(e.target.value))}
            className="accent-amber-500 cursor-pointer"
          />
          <div className="flex justify-between text-[10px] text-neutral-500">
            <span>高感度 (わずかな動きで起動・10%)</span>
            <span>通常 (25%)</span>
            <span>低感度 (大きな動きのみ・60%)</span>
          </div>
        </div>

        {/* Port A SG90 2-Axis Servo Config Section */}
        <div className="flex flex-col gap-3 bg-neutral-950 p-4 rounded-xl border border-cyan-800/60 md:col-span-2 lg:col-span-3">
          <div className="flex items-center justify-between pb-2 border-b border-neutral-800">
            <span className="font-bold text-neutral-200 flex items-center gap-2">
              <Rotate3D className="w-4 h-4 text-cyan-400" />
              Port A (Grove Red) SG90 2軸サーボ設定 (首振り・うなずき)
            </span>
            <label className="flex items-center gap-2 cursor-pointer">
              <span className="text-[11px] text-neutral-400">サーボ連携</span>
              <input
                type="checkbox"
                checked={servoEnabled}
                onChange={(e) => setServoEnabled(e.target.checked)}
                className="accent-cyan-500 cursor-pointer w-4 h-4"
              />
            </label>
          </div>

          <div className="grid grid-cols-1 sm:grid-cols-2 md:grid-cols-4 gap-3 text-xs">
            {/* Pan Pin */}
            <div className="flex flex-col gap-1">
              <label className="text-neutral-400">Pan ピン (左右首振り / Yaw)</label>
              <div className="flex items-center gap-2">
                <span className="bg-neutral-900 px-2 py-1 rounded text-cyan-300 font-mono font-bold border border-neutral-700">
                  GPIO {servoPanPin}
                </span>
                <span className="text-[10px] text-neutral-500">Port A Pin 1 (SDA)</span>
              </div>
            </div>

            {/* Tilt Pin */}
            <div className="flex flex-col gap-1">
              <label className="text-neutral-400">Tilt ピン (上下うなずき / Pitch)</label>
              <div className="flex items-center gap-2">
                <span className="bg-neutral-900 px-2 py-1 rounded text-cyan-300 font-mono font-bold border border-neutral-700">
                  GPIO {servoTiltPin}
                </span>
                <span className="text-[10px] text-neutral-500">Port A Pin 2 (SCL)</span>
              </div>
            </div>

            {/* Pan Center Trim */}
            <div className="flex flex-col gap-1">
              <div className="flex justify-between text-neutral-400">
                <span>Pan センター角度</span>
                <span className="font-mono text-cyan-400">{servoPanCenter}°</span>
              </div>
              <input
                type="range"
                min={70}
                max={110}
                value={servoPanCenter}
                onChange={(e) => setServoPanCenter(Number(e.target.value))}
                className="accent-cyan-500 cursor-pointer"
              />
            </div>

            {/* Tilt Center Trim */}
            <div className="flex flex-col gap-1">
              <div className="flex justify-between text-neutral-400">
                <span>Tilt センター角度</span>
                <span className="font-mono text-cyan-400">{servoTiltCenter}°</span>
              </div>
              <input
                type="range"
                min={70}
                max={110}
                value={servoTiltCenter}
                onChange={(e) => setServoTiltCenter(Number(e.target.value))}
                className="accent-cyan-500 cursor-pointer"
              />
            </div>
          </div>
          <p className="text-[10px] text-neutral-500">
            現在はサーボ再有効化版です。通常のGroveケーブルを使用する場合、動作時はPCのUSBを外し、TAKAO基板の外部電源のみで給電してください。書込み時は外部電源OFFでGroveを外します。外部給電で消灯せずタップ復帰する動作を2回確認しました。長時間安定性は未確認です。
          </p>
        </div>
      </div>
    </div>
  );
};
