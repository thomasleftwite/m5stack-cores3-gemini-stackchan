#include "AudioTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/ringbuf.h>

static RingbufHandle_t s_audioRingBuf = nullptr;
static volatile float s_liveRMS = 0.0f;
static volatile bool s_voiceActive = false;
static volatile uint32_t s_lastVoiceTime = 0;
static volatile bool s_isPlaying = false;

#define AUDIO_BUF_SIZE (64 * 1024) // 64KB PSRAM RingBuffer

void AudioTask::init(uint8_t micGain, uint8_t spkVolume) {
    // 重要: M5.begin() が既にSpeakerとMicを起動しているため、
    // 重複して .begin() を呼ぶと "register I2S object failed" エラーになるのを防ぐ
    M5.Speaker.setVolume(spkVolume);

    // PSRAM上にリングバッファ生成
    if (!s_audioRingBuf) {
        s_audioRingBuf = xRingbufferCreate(AUDIO_BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
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

void AudioTask::audioWorkerTask(void* pvParameters) {
    int16_t micBuffer[256];
    uint8_t spkBuffer[512];

    while (true) {
        // 1. マイク読み取り & VAD
        if (M5.Mic.record(micBuffer, 256, 16000)) {
            int64_t sumSquare = 0;
            for (int i = 0; i < 256; i++) {
                sumSquare += (int32_t)micBuffer[i] * micBuffer[i];
            }
            float rms = sqrtf((float)sumSquare / 256.0f);
            s_liveRMS = rms / 8000.0f;

            if (s_liveRMS > 0.08f) {
                s_voiceActive = true;
                s_lastVoiceTime = millis();
            } else {
                s_voiceActive = false;
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
