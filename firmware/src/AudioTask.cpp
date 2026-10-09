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

#define AUDIO_BUF_SIZE (64 * 1024) // 64KB PSRAM RingBuffer for 24kHz speaker playback
#define RECORD_MAX_SAMPLES (16000 * 4) // 4秒間 @ 16kHz mono = 64,000 samples = 128KB

static int16_t* s_recordBuffer = nullptr;
static volatile size_t s_recordSampleCount = 0;
static volatile bool s_isRecording = false;
static volatile size_t s_speechSampleCount = 0;

void AudioTask::init(uint8_t micGain, uint8_t spkVolume) {
    // 重要: M5.begin() が既にSpeakerとMicを起動しているため、
    // 重複して .begin() を呼ぶと "register I2S object failed" エラーになるのを防ぐ
    M5.Speaker.setVolume(spkVolume);

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
        xRingbufferSend(s_audioRingBuf, pcmData, length, pdMS_TO_TICKS(100));
        s_isPlaying = true;
    }
}

bool AudioTask::isPlaying() {
    return s_isPlaying || M5.Speaker.isPlaying();
}

void AudioTask::stopPlayback() {
    M5.Speaker.stop();
    s_isPlaying = false;
}

void AudioTask::startRecording() {
    s_recordSampleCount = 0;
    s_speechSampleCount = 0;
    s_isRecording = true;
    s_lastVoiceTime = millis();
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
    // 350ms以上の実発話 (音量閾値超え) が蓄積されていれば有意な発話とみなす
    return s_speechSampleCount >= (16000 * 35 / 100);
}

void AudioTask::audioWorkerTask(void* pvParameters) {
    int16_t micBuffer[256];
    uint8_t spkBuffer[512];

    while (true) {
        // 1. マイク読み取り & VAD & 録音
        if (M5.Mic.record(micBuffer, 256, 16000)) {
            int64_t sumSquare = 0;
            for (int i = 0; i < 256; i++) {
                sumSquare += (int32_t)micBuffer[i] * micBuffer[i];
            }
            float rms = sqrtf((float)sumSquare / 256.0f);
            s_liveRMS = rms / 8000.0f;

            if (s_liveRMS > 0.09f) {
                s_voiceActive = true;
                s_lastVoiceTime = millis();
                if (s_isRecording) {
                    s_speechSampleCount += 256;
                }
            } else {
                s_voiceActive = false;
            }

            // 録音中ならPSRAMバッファへ追記
            if (s_isRecording && s_recordBuffer) {
                if (s_recordSampleCount + 256 <= RECORD_MAX_SAMPLES) {
                    memcpy(&s_recordBuffer[s_recordSampleCount], micBuffer, 256 * sizeof(int16_t));
                    s_recordSampleCount += 256;
                } else {
                    // バッファ上限に達したら自動停止
                    s_isRecording = false;
                }
            }
        }

        // 2. スピーカー再生 (DMA投入)
        if (s_audioRingBuf) {
            size_t itemSize = 0;
            void* item = xRingbufferReceiveUpTo(s_audioRingBuf, &itemSize, pdMS_TO_TICKS(5), sizeof(spkBuffer));
            if (item && itemSize > 0) {
                M5.Speaker.playRaw((const int16_t*)item, itemSize / 2, 24000, false, 1);
                vRingbufferReturnItem(s_audioRingBuf, item);
                s_isPlaying = true;
            } else {
                if (!M5.Speaker.isPlaying()) {
                    s_isPlaying = false;
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
