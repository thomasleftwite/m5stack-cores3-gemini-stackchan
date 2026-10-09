#include "AudioTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>
#include <esp_heap_caps.h>

static RingbufHandle_t s_audioRingBuf = nullptr;
static volatile float s_liveRMS = 0.0f;
static volatile bool s_voiceActive = false;
static volatile uint32_t s_lastVoiceTime = 0;
static volatile bool s_isPlaying = false;

static uint8_t s_spkVolume = 160;
static uint8_t s_micGain = 80;

// CoreS3 は ES7210 (Mic) と AW88298 (Speaker) が同じ I2S バスを共有するため、
// audioWorkerTask 内で直列に排他制御を行う
static volatile AudioMode s_requestedMode = AUDIO_MODE_MIC;
static AudioMode s_currentMode = AUDIO_MODE_UNINIT;

#define AUDIO_BUF_SIZE (96 * 1024) // 96KB PSRAM RingBuffer for 24kHz speaker playback
#define RECORD_MAX_SAMPLES (16000 * 4) // 4秒間 @ 16kHz mono = 64,000 samples = 128KB

static int16_t* s_recordBuffer = nullptr;
static volatile size_t s_recordSampleCount = 0;
static volatile bool s_isRecording = false;
static volatile size_t s_speechSampleCount = 0;

void AudioTask::init(uint8_t micGain, uint8_t spkVolume) {
    s_micGain = micGain;
    s_spkVolume = spkVolume;

    // PSRAM上にスピーカー再生リングバッファ生成
    if (!s_audioRingBuf) {
        s_audioRingBuf = xRingbufferCreate(AUDIO_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    }

    // PSRAM上にマイク録音バッファ (128KB) を確保
    if (!s_recordBuffer) {
        s_recordBuffer = (int16_t*)heap_caps_malloc(RECORD_MAX_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        if (!s_recordBuffer) {
            s_recordBuffer = (int16_t*)malloc(RECORD_MAX_SAMPLES * sizeof(int16_t));
        }
        if (s_recordBuffer) {
            memset(s_recordBuffer, 0, RECORD_MAX_SAMPLES * sizeof(int16_t));
        }
    }

    s_requestedMode = AUDIO_MODE_MIC;
    s_currentMode = AUDIO_MODE_UNINIT;
    s_lastVoiceTime = millis();
}

void AudioTask::start() {
    xTaskCreatePinnedToCore(
        audioWorkerTask,
        "AudioWorker",
        4096,
        nullptr,
        3,
        nullptr,
        1
    );
}

void AudioTask::requestMicMode() {
    s_requestedMode = AUDIO_MODE_MIC;
}

void AudioTask::requestPlaybackMode() {
    s_requestedMode = AUDIO_MODE_PLAYBACK;
}

AudioMode AudioTask::getCurrentMode() {
    return s_currentMode;
}

float AudioTask::getLiveRMS() {
    return s_liveRMS;
}

bool AudioTask::isVoiceDetected() {
    return s_voiceActive;
}

uint32_t AudioTask::getSilenceDurationMs() {
    return millis() - s_lastVoiceTime;
}

void AudioTask::resetSilenceTimer() {
    s_lastVoiceTime = millis();
}

void AudioTask::enqueueAudioChunk(const uint8_t* pcmData, size_t length) {
    if (s_audioRingBuf && pcmData && length > 0) {
        // スピーカーモードへの切替を要求
        if (s_requestedMode != AUDIO_MODE_PLAYBACK) {
            s_requestedMode = AUDIO_MODE_PLAYBACK;
        }
        s_isPlaying = true;
        xRingbufferSend(s_audioRingBuf, pcmData, length, pdMS_TO_TICKS(150));
    }
}

bool AudioTask::isPlaying() {
    return s_isPlaying || (s_currentMode == AUDIO_MODE_PLAYBACK && M5.Speaker.isPlaying());
}

void AudioTask::stopPlayback() {
    s_requestedMode = AUDIO_MODE_MIC;
    s_isPlaying = false;
}

void AudioTask::startRecording() {
    s_recordSampleCount = 0;
    s_speechSampleCount = 0;
    s_isRecording = true;
    s_lastVoiceTime = millis();
    s_requestedMode = AUDIO_MODE_MIC;
    Serial.println("[Audio] Recording started for speech input (max 4.0s @ 16kHz)...");
}

void AudioTask::stopRecording() {
    s_isRecording = false;
    Serial.printf("[Audio] Recording stopped. Samples=%u (%u ms), speechSamples=%u (%u ms)\n",
                  static_cast<unsigned>(s_recordSampleCount),
                  static_cast<unsigned>(s_recordSampleCount * 1000 / 16000),
                  static_cast<unsigned>(s_speechSampleCount),
                  static_cast<unsigned>(s_speechSampleCount * 1000 / 16000));
}

bool AudioTask::isRecording() {
    return s_isRecording;
}

const int16_t* AudioTask::getRecordedPCM(size_t* outSamples) {
    if (outSamples) *outSamples = s_recordSampleCount;
    return s_recordBuffer;
}

size_t AudioTask::getRecordedBytes() {
    return s_recordSampleCount * sizeof(int16_t);
}

bool AudioTask::hasMeaningfulSpeech() {
    // 250ms以上の実発話 (音量閾値超え) が蓄積されていれば有意な発話とみなす (16000 * 25 / 100 = 4000 samples)
    return s_speechSampleCount >= 4000;
}

void AudioTask::audioWorkerTask(void* pvParameters) {
    int16_t micBuffer[256];

    // 4面ローテーションバッファ (各512サンプル = 1024バイト = 約21.3ms @ 24kHz)
    // M5.Speaker.playRaw はポインタをDMAキューに保持するため、
    // DMA転送完了前に上書きされないようローテーションバッファを使用
    static int16_t spkBuffers[4][512];
    static size_t spkBufIdx = 0;
    static uint32_t lastChunkMillis = 0;

    while (true) {
        // 1. I2Sバス排他モード切替 (すべてのI2S呼出をこのワーカタスク内で直列化)
        if (s_requestedMode != s_currentMode) {
            if (s_requestedMode == AUDIO_MODE_PLAYBACK) {
                Serial.println("[Audio] Switching I2S: MIC (ES7210) -> SPEAKER (AW88298)...");
                if (s_currentMode == AUDIO_MODE_MIC) {
                    M5.Mic.end();
                    vTaskDelay(pdMS_TO_TICKS(15));
                }
                M5.Speaker.begin();
                M5.Speaker.setVolume(s_spkVolume);
                M5.Speaker.setChannelVolume(0, s_spkVolume);
                s_currentMode = AUDIO_MODE_PLAYBACK;
                lastChunkMillis = millis();
                Serial.println("[Audio] I2S switched to SPEAKER mode successfully.");
            } else if (s_requestedMode == AUDIO_MODE_MIC) {
                Serial.println("[Audio] Switching I2S: SPEAKER (AW88298) -> MIC (ES7210)...");
                if (s_currentMode == AUDIO_MODE_PLAYBACK) {
                    M5.Speaker.stop();
                    while (M5.Speaker.isPlaying()) {
                        vTaskDelay(pdMS_TO_TICKS(5));
                    }
                    M5.Speaker.end();
                    vTaskDelay(pdMS_TO_TICKS(15));
                }
                M5.Mic.begin();
                s_currentMode = AUDIO_MODE_MIC;
                s_isPlaying = false;
                s_liveRMS = 0.0f;
                s_voiceActive = false;
                s_lastVoiceTime = millis();
                Serial.println("[Audio] I2S switched to MIC mode successfully.");
            }
        }

        // 2. モード別の実行処理
        if (s_currentMode == AUDIO_MODE_MIC) {
            if (M5.Mic.record(micBuffer, 256, 16000)) {
                int64_t sumSquare = 0;
                for (int i = 0; i < 256; i++) {
                    sumSquare += (int32_t)micBuffer[i] * micBuffer[i];
                }
                float rms = sqrtf((float)sumSquare / 256.0f);
                s_liveRMS = rms / 8000.0f;

                // 人間の通常発話 (30-50cm) に適したVAD閾値 (RMS ~360)
                if (s_liveRMS > 0.045f) {
                    s_voiceActive = true;
                    s_lastVoiceTime = millis();
                    if (s_isRecording) {
                        s_speechSampleCount += 256;
                    }
                } else {
                    s_voiceActive = false;
                }

                if (s_isRecording && s_recordBuffer) {
                    if (s_recordSampleCount + 256 <= RECORD_MAX_SAMPLES) {
                        memcpy(&s_recordBuffer[s_recordSampleCount], micBuffer, 256 * sizeof(int16_t));
                        s_recordSampleCount += 256;
                    } else {
                        s_isRecording = false;
                    }
                }
            }
            vTaskDelay(pdMS_TO_TICKS(5));
        }
        else if (s_currentMode == AUDIO_MODE_PLAYBACK) {
            // M5Unified Speaker Channel 0 の空き状態確認
            // (0: 停止中, 1: 再生中で空きキューあり, 2: キュー満杯)
            size_t playingState = M5.Speaker.isPlaying(0);
            bool chunkFed = false;

            if (playingState < 2 && s_audioRingBuf) {
                size_t itemSize = 0;
                void* item = xRingbufferReceiveUpTo(s_audioRingBuf, &itemSize, pdMS_TO_TICKS(5), sizeof(spkBuffers[0]));
                if (item && itemSize > 0) {
                    size_t samples = itemSize / sizeof(int16_t);
                    if (samples > 512) samples = 512;

                    memcpy(spkBuffers[spkBufIdx], item, samples * sizeof(int16_t));
                    vRingbufferReturnItem(s_audioRingBuf, item);

                    // リップシンク用RMS計算 (TTS音声に合わせてアバターの口を同期)
                    int64_t sumSquare = 0;
                    for (size_t i = 0; i < samples; i++) {
                        sumSquare += (int32_t)spkBuffers[spkBufIdx][i] * spkBuffers[spkBufIdx][i];
                    }
                    float rms = sqrtf((float)sumSquare / (float)samples);
                    s_liveRMS = rms / 6000.0f;

                    // Channel 0 で24kHz mono 16bit PCM再生
                    M5.Speaker.playRaw(spkBuffers[spkBufIdx], samples, 24000, false, 1, 0);
                    spkBufIdx = (spkBufIdx + 1) % 4;

                    s_isPlaying = true;
                    lastChunkMillis = millis();
                    chunkFed = true;
                }
            }

            // リングバッファが空で、かつスピーカー再生キューも空になった場合
            if (!chunkFed && playingState == 0) {
                // 最後のPCMチャンク投入から200ms以上経過していたら再生完了
                if (s_isPlaying && (millis() - lastChunkMillis > 200)) {
                    s_isPlaying = false;
                    s_liveRMS = 0.0f;
                    Serial.println("[Audio] Playback completed. Returning to MIC mode.");
                    s_requestedMode = AUDIO_MODE_MIC;
                }
            }

            vTaskDelay(pdMS_TO_TICKS(5));
        }
        else {
            vTaskDelay(pdMS_TO_TICKS(5));
        }
    }
}
