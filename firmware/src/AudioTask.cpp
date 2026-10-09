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

#define AUDIO_BUF_SIZE (128 * 1024) // 128KB PSRAM RingBuffer for 24kHz speaker playback
#define RECORD_MAX_SAMPLES (16000 * 4) // 4秒間 @ 16kHz mono = 64,000 samples = 128KB

static int16_t* s_recordBuffer = nullptr;
static volatile size_t s_recordSampleCount = 0;
static volatile bool s_isRecording = false;
static volatile size_t s_speechSampleCount = 0;

static volatile bool s_streamFinished = false;
static volatile size_t s_bufferedBytes = 0;
static volatile bool s_playbackStarted = false;
static float s_noiseFloor = 0.006f;

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

    // M5.begin(cfg) で両方のデバイス(ES7210マイク/AW88298スピーカー)が給電・初期化されている。
    // CoreS3の単一I2Sバス競合を防ぐため、初期状態はスピーカーを一旦停止・終了し、
    // マイク待受モードを排他起動する。
    M5.Speaker.stop();
    M5.Speaker.end();
    vTaskDelay(pdMS_TO_TICKS(20));

    s_currentMode = AUDIO_MODE_MIC;
    s_requestedMode = AUDIO_MODE_MIC;
    s_lastVoiceTime = millis();
    Serial.println("[Audio] Audio subsystem initialized in MIC mode.");
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
        // 16-bit PCM word境界 (2バイト) にアライン
        size_t safeLen = length & ~1;
        if (safeLen == 0) return;

        // スピーカーモードへの切替を要求
        if (s_requestedMode != AUDIO_MODE_PLAYBACK) {
            s_requestedMode = AUDIO_MODE_PLAYBACK;
        }
        s_isPlaying = true;
        BaseType_t res = xRingbufferSend(s_audioRingBuf, pcmData, safeLen, pdMS_TO_TICKS(150));
        if (res == pdTRUE) {
            s_bufferedBytes += safeLen;
        }
    }
}

void AudioTask::finishAudioStream() {
    s_streamFinished = true;
}

bool AudioTask::isPlaying() {
    return s_isPlaying || (s_currentMode == AUDIO_MODE_PLAYBACK && M5.Speaker.isPlaying());
}

void AudioTask::stopPlayback() {
    s_requestedMode = AUDIO_MODE_MIC;
    s_isPlaying = false;
    s_playbackStarted = false;
    s_streamFinished = false;
    s_bufferedBytes = 0;
    M5.Speaker.stop();

    // リングバッファに残っている未再生PCMをフラッシュ
    if (s_audioRingBuf) {
        size_t dummySize = 0;
        void* dummy = nullptr;
        while ((dummy = xRingbufferReceiveUpTo(s_audioRingBuf, &dummySize, 0, 1024)) != nullptr) {
            vRingbufferReturnItem(s_audioRingBuf, dummy);
        }
    }
}

void AudioTask::playChirp(bool happy) {
    playSoundCue(happy ? SOUND_CUE_WAKE_WORD : SOUND_CUE_ERROR);
}

void AudioTask::playSoundCue(SoundCue cue) {
    if (!s_audioRingBuf) return;
    const uint32_t sampleRate = 24000;

    int toneCount = 0;
    float freqs[4] = {0};
    size_t durations[4] = {0};
    float amplitude = 10000.0f;

    switch (cue) {
        case SOUND_CUE_WAKE_MOTION:
            // 1. 動体検知で起動: 静かで優しい短音 (660Hz, 80ms)
            toneCount = 1;
            freqs[0] = 660.0f;
            durations[0] = 80;
            amplitude = 7500.0f;
            break;

        case SOUND_CUE_WAKE_WORD:
            // 2. ウェイクワード/タップで入力待ち: 軽快な上昇 2音 (880Hz -> 1320Hz, 各70ms)
            toneCount = 2;
            freqs[0] = 880.0f;
            durations[0] = 70;
            freqs[1] = 1320.0f;
            durations[1] = 90;
            amplitude = 11000.0f;
            break;

        case SOUND_CUE_REC_COMPLETE:
            // 3. 入力録音完了: クリアで短い受理音 (1046Hz [C6], 90ms)
            toneCount = 1;
            freqs[0] = 1046.5f;
            durations[0] = 90;
            amplitude = 9500.0f;
            break;

        case SOUND_CUE_ERROR:
        default:
            // 4. AIデータ送受信失敗・Gemini連携失敗: 困惑を伝える下降 3音 (880Hz -> 660Hz -> 440Hz, 各80ms)
            toneCount = 3;
            freqs[0] = 880.0f;
            durations[0] = 75;
            freqs[1] = 660.0f;
            durations[1] = 75;
            freqs[2] = 440.0f;
            durations[2] = 120;
            amplitude = 12000.0f;
            break;
    }

    int16_t chirpPcm[256];
    float phase = 0.0f;

    for (int t = 0; t < toneCount; t++) {
        float freq = freqs[t];
        size_t totalSamples = (sampleRate * durations[t]) / 1000;
        size_t generated = 0;

        while (generated < totalSamples) {
            size_t batch = totalSamples - generated;
            if (batch > 256) batch = 256;

            for (size_t i = 0; i < batch; i++) {
                // クリックノイズ防止エンベロープ (フェードイン & フェードアウト)
                float env = 1.0f;
                float progress = (float)(generated + i) / (float)totalSamples;
                if (progress < 0.12f) env = progress / 0.12f;
                else if (progress > 0.85f) env = (1.0f - progress) / 0.15f;

                float sampleVal = sinf(phase) * amplitude * env;
                chirpPcm[i] = static_cast<int16_t>(sampleVal);
                phase += (2.0f * 3.14159265f * freq) / static_cast<float>(sampleRate);
                if (phase > 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;
            }

            enqueueAudioChunk(reinterpret_cast<const uint8_t*>(chirpPcm), batch * sizeof(int16_t));
            generated += batch;
        }
    }
    finishAudioStream();
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
    // 768サンプル (~48ms) 以上の有声区間が検知されたか、または総録音時間が0.5秒 (8000サンプル) を超えている場合はユーザー発話ありと判定
    return (s_speechSampleCount >= 768) || (s_recordSampleCount >= 8000);
}

void AudioTask::audioWorkerTask(void* pvParameters) {
    int16_t micBuffer[256];

    // 4面ローテーションバッファ (各1024サンプル = 2048バイト = 約42.6ms @ 24kHz)
    // 適切なチャンクサイズでDMAキューに供給することで、ネットワークジッターや途切れノイズを完全に防止
    static int16_t spkBuffers[4][1024];
    static size_t spkBufIdx = 0;
    static uint32_t lastChunkMillis = 0;

    while (true) {
        // 1. I2Sバス排他モード切替 (すべてのI2S呼出をこのワーカタスク内で直列化)
        if (s_requestedMode != s_currentMode) {
            if (s_requestedMode == AUDIO_MODE_PLAYBACK) {
                Serial.println("[Audio] Switching I2S: MIC (ES7210) -> SPEAKER (AW88298)...");
                if (s_currentMode == AUDIO_MODE_MIC) {
                    M5.Mic.end();
                    vTaskDelay(pdMS_TO_TICKS(30));
                }
                M5.Speaker.begin();
                M5.Speaker.setVolume(s_spkVolume);
                M5.Speaker.setChannelVolume(0, s_spkVolume);
                s_currentMode = AUDIO_MODE_PLAYBACK;
                s_playbackStarted = false;
                lastChunkMillis = millis();
                Serial.println("[Audio] I2S switched to SPEAKER mode successfully.");
            } else if (s_requestedMode == AUDIO_MODE_MIC) {
                Serial.println("[Audio] Switching I2S: SPEAKER (AW88298) -> MIC (ES7210)...");
                if (s_currentMode == AUDIO_MODE_PLAYBACK) {
                    M5.Speaker.stop();
                    while (M5.Speaker.isPlaying()) {
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    vTaskDelay(pdMS_TO_TICKS(20));
                    M5.Speaker.end();
                    vTaskDelay(pdMS_TO_TICKS(40));
                }
                M5.Mic.begin();
                s_currentMode = AUDIO_MODE_MIC;
                s_isPlaying = false;
                s_playbackStarted = false;
                s_streamFinished = false;
                s_bufferedBytes = 0;
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

                // 環境ノイズフロアの適応型トラッキング (定常騒音を学習して適応)
                s_noiseFloor = s_noiseFloor * 0.98f + s_liveRMS * 0.02f;
                if (s_noiseFloor < 0.003f) s_noiseFloor = 0.003f;
                if (s_noiseFloor > 0.035f) s_noiseFloor = 0.035f;

                // 通常対話距離 (30-60cm) に最適化した動的VAD閾値 (RMS約80〜120相当)
                float voiceThreshold = fmaxf(0.010f, s_noiseFloor * 1.5f);

                if (s_liveRMS > voiceThreshold) {
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
            // ジッターバッファ: 再生開始前に16KB (~340ms) の蓄積またはストリーム完了を待機し、途切れを根絶
            if (!s_playbackStarted) {
                if (s_bufferedBytes >= 16384 || s_streamFinished) {
                    s_playbackStarted = true;
                } else {
                    vTaskDelay(pdMS_TO_TICKS(5));
                    continue;
                }
            }

            // M5Unified Speaker Channel 0 の空き状態確認
            // (0: 停止中, 1: 再生中で空きキューあり, 2: キュー満杯)
            size_t playingState = M5.Speaker.isPlaying(0);
            bool chunkFed = false;

            if (playingState < 2 && s_audioRingBuf) {
                size_t itemSize = 0;
                void* item = xRingbufferReceiveUpTo(s_audioRingBuf, &itemSize, pdMS_TO_TICKS(5), sizeof(spkBuffers[0]));
                if (item && itemSize > 0) {
                    size_t samples = itemSize / sizeof(int16_t);
                    if (samples > 1024) samples = 1024;

                    memcpy(spkBuffers[spkBufIdx], item, samples * sizeof(int16_t));
                    vRingbufferReturnItem(s_audioRingBuf, item);
                    if (s_bufferedBytes >= itemSize) s_bufferedBytes -= itemSize;
                    else s_bufferedBytes = 0;

                    // リップシンク用RMS計算 (TTS音声に合わせてアバターの口を同期)
                    int64_t sumSquare = 0;
                    for (size_t i = 0; i < samples; i++) {
                        sumSquare += (int32_t)spkBuffers[spkBufIdx][i] * spkBuffers[spkBufIdx][i];
                    }
                    float rms = sqrtf((float)sumSquare / (float)samples);
                    s_liveRMS = rms / 6000.0f;

                    // Channel 0 で24kHz mono 16bit PCM再生 (1024サンプル = ~42.6ms)
                    M5.Speaker.playRaw(spkBuffers[spkBufIdx], samples, 24000, false, 1, 0);
                    spkBufIdx = (spkBufIdx + 1) % 4;

                    s_isPlaying = true;
                    lastChunkMillis = millis();
                    chunkFed = true;
                }
            }

            // ストリーム送信が完了し、リングバッファとスピーカーDMAキューの両方が空になったら再生完了
            if (s_playbackStarted && s_streamFinished && s_bufferedBytes == 0 && playingState == 0 && !chunkFed) {
                if (millis() - lastChunkMillis > 200) {
                    s_isPlaying = false;
                    s_liveRMS = 0.0f;
                    s_playbackStarted = false;
                    s_streamFinished = false;
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
