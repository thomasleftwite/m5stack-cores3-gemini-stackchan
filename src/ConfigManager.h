#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "LocalStorage.h"
#include "GeminiModels.h"

struct AppConfig {
    String gemini_api_key = "";
    String gemini_model = geminimodels::Reply;
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
        File file = localstorage::filesystem().open("/config.json", "w");
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

        const size_t expected = measureJsonPretty(doc);
        const size_t written = serializeJsonPretty(doc, file);
        file.flush();
        file.close();
        const bool ok = written == expected;
        Serial.printf("[Config] default_saved=%d\n", int(ok));
        return ok;
    }

    static bool load(AppConfig& cfg) {
        bool mounted = localstorage::begin();
        Serial.printf("[Storage] backend=FFat mounted=%d\n", int(mounted));
        if (!mounted) {
            return false;
        }

        // 2. /config.json が存在しない場合はデフォルトファイルを作成して保存
        if (!localstorage::filesystem().exists("/config.json")) {
            Serial.println("[Config] /config.json not found. Initializing default config file...");
            return save(cfg);
        }

        File file = localstorage::filesystem().open("/config.json", "r");
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
        // Retain the legacy config field for export compatibility. R13 uses fixed models.
        // Old on-device config values cannot override the approved reply model.
        cfg.gemini_model = geminimodels::Reply;
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

        Serial.println("[Config] Local configuration loaded.");
        Serial.printf("[Config] key_configured=%d\n", int(!cfg.gemini_api_key.isEmpty()));
        return true;
    }
};
