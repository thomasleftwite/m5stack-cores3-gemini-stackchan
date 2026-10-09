#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>

struct AppConfig {
    String gemini_api_key = "";
    String gemini_model = "gemini-3.1-flash-lite";
    String tts_voice = "Kore";
    String wake_word = "スタックちゃん";
    int silence_timeout_sec = 6;
    int camera_motion_threshold = 25;
    uint8_t spk_volume = 160;
    uint8_t mic_gain = 80;

    // Port A (Grove Red) SG90 2軸サーボ設定
    bool servo_enabled = true;
    uint8_t servo_pan_pin = 2;   // Port A Pin 1 (G2)
    uint8_t servo_tilt_pin = 1;  // Port A Pin 2 (G1)
    int servo_pan_center = 90;
    int servo_tilt_center = 90;
};

class ConfigManager {
public:
    static bool save(const AppConfig& cfg) {
        File file = LittleFS.open("/config.json", "w");
        if (!file) {
            Serial.println("[Config] Failed to open /config.json for writing");
            return false;
        }

        JsonDocument doc;
        doc["gemini_api_key"] = cfg.gemini_api_key;
        doc["gemini_model"] = cfg.gemini_model;
        doc["tts_voice"] = cfg.tts_voice;
        doc["wake_word"] = cfg.wake_word;
        doc["silence_timeout_sec"] = cfg.silence_timeout_sec;
        doc["camera_motion_threshold"] = cfg.camera_motion_threshold;
        doc["spk_volume"] = cfg.spk_volume;
        doc["mic_gain"] = cfg.mic_gain;
        doc["servo_enabled"] = cfg.servo_enabled;
        doc["servo_pan_pin"] = cfg.servo_pan_pin;
        doc["servo_tilt_pin"] = cfg.servo_tilt_pin;
        doc["servo_pan_center"] = cfg.servo_pan_center;
        doc["servo_tilt_center"] = cfg.servo_tilt_center;

        serializeJsonPretty(doc, file);
        file.close();
        Serial.println("[Config] Auto-created /config.json on LittleFS");
        return true;
    }

    static bool load(AppConfig& cfg) {
        // 1. LittleFSのマウント試行 (破損または未フォーマット時は自動フォーマット)
        bool mounted = LittleFS.begin(false);
        if (!mounted) {
            Serial.println("[Config] LittleFS unformatted or corrupt. Formatting now...");
            mounted = LittleFS.begin(true); // formatOnFail = true
        }

        if (!mounted) {
            Serial.println("[Config] LittleFS mount failed. Using embedded defaults.");
            return false;
        }

        // 2. /config.json が存在しない場合はデフォルトファイルを作成して保存
        if (!LittleFS.exists("/config.json")) {
            Serial.println("[Config] /config.json not found. Initializing default config file...");
            save(cfg);
            return true;
        }

        File file = LittleFS.open("/config.json", "r");
        if (!file) {
            Serial.println("[Config] Failed to open /config.json. Using defaults.");
            return false;
        }

        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, file);
        file.close();

        if (error) {
            Serial.printf("[Config] JSON Parse failed: %s. Using defaults.\n", error.c_str());
            return false;
        }

        cfg.gemini_api_key = doc["gemini_api_key"] | cfg.gemini_api_key;
        cfg.gemini_model = doc["gemini_model"] | "gemini-3.1-flash-lite";
        if (cfg.gemini_model == "gemini-3.5-flash" || cfg.gemini_model == "gemini-3.8-flash") {
            cfg.gemini_model = "gemini-3.1-flash-lite";
        }
        cfg.tts_voice = doc["tts_voice"] | "Kore";
        cfg.wake_word = doc["wake_word"] | "スタックちゃん";
        cfg.silence_timeout_sec = doc["silence_timeout_sec"] | 6;
        cfg.camera_motion_threshold = doc["camera_motion_threshold"] | 25;
        cfg.spk_volume = doc["spk_volume"] | 160;
        cfg.mic_gain = doc["mic_gain"] | 80;

        cfg.servo_enabled = doc["servo_enabled"] | true;
        cfg.servo_pan_pin = doc["servo_pan_pin"] | 2;
        cfg.servo_tilt_pin = doc["servo_tilt_pin"] | 1;
        cfg.servo_pan_center = doc["servo_pan_center"] | 90;
        cfg.servo_tilt_center = doc["servo_tilt_center"] | 90;

        Serial.println("[Config] Loaded successfully from LittleFS");
        return true;
    }
};
