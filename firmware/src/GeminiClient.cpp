#include "GeminiClient.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <esp_heap_caps.h>
#include <functional>

namespace {
// Base64 table
static const char B64_CHARS[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void encodeBase64(const uint8_t* in, size_t inLen, char* out) {
    size_t i = 0, j = 0;
    while (i < inLen) {
        uint32_t octet_a = i < inLen ? in[i++] : 0;
        uint32_t octet_b = i < inLen ? in[i++] : 0;
        uint32_t octet_c = i < inLen ? in[i++] : 0;
        uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

        out[j++] = B64_CHARS[(triple >> 18) & 0x3F];
        out[j++] = B64_CHARS[(triple >> 12) & 0x3F];
        out[j++] = (i > inLen + 1) ? '=' : B64_CHARS[(triple >> 6) & 0x3F];
        out[j++] = (i > inLen) ? '=' : B64_CHARS[triple & 0x3F];
    }
    out[j] = '\0';
}

void buildWavHeader(uint8_t* h, uint32_t pcmLen, uint32_t sampleRate = 16000) {
    uint32_t totalLen = pcmLen + 36;
    uint32_t byteRate = sampleRate * 1 * 2;
    memcpy(h, "RIFF", 4);
    h[4] = (uint8_t)(totalLen & 0xff); h[5] = (uint8_t)((totalLen >> 8) & 0xff);
    h[6] = (uint8_t)((totalLen >> 16) & 0xff); h[7] = (uint8_t)((totalLen >> 24) & 0xff);
    memcpy(h + 8, "WAVEfmt ", 8);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0; // subchunk1 size
    h[20] = 1; h[21] = 0; // PCM format
    h[22] = 1; h[23] = 0; // mono
    h[24] = (uint8_t)(sampleRate & 0xff); h[25] = (uint8_t)((sampleRate >> 8) & 0xff);
    h[26] = (uint8_t)((sampleRate >> 16) & 0xff); h[27] = (uint8_t)((sampleRate >> 24) & 0xff);
    h[28] = (uint8_t)(byteRate & 0xff); h[29] = (uint8_t)((byteRate >> 8) & 0xff);
    h[30] = (uint8_t)((byteRate >> 16) & 0xff); h[31] = (uint8_t)((byteRate >> 24) & 0xff);
    h[32] = 2; h[33] = 0; // block align
    h[34] = 16; h[35] = 0; // bits per sample
    memcpy(h + 36, "data", 4);
    h[40] = (uint8_t)(pcmLen & 0xff); h[41] = (uint8_t)((pcmLen >> 8) & 0xff);
    h[42] = (uint8_t)((pcmLen >> 16) & 0xff); h[43] = (uint8_t)((pcmLen >> 24) & 0xff);
}

// Streaming Base64 Decoder: 4文字 -> 3バイト変換し、44バイトのWAVヘッダーをスキップしてPCMを供給
class Base64StreamDecoder {
public:
    typedef std::function<void(const uint8_t* pcm, size_t len)> ChunkCallback;

    explicit Base64StreamDecoder(ChunkCallback cb)
        : cb_(cb), bufIdx_(0), totalDecodedBytes_(0) {}

    void write(char c) {
        int val = decodeChar(c);
        if (val >= 0) {
            buf_[bufIdx_++] = (uint8_t)val;
            if (bufIdx_ == 4) {
                flushBlock(3);
                bufIdx_ = 0;
            }
        } else if (c == '=') {
            // パディング処理
            if (bufIdx_ == 3) {
                flushBlock(2);
                bufIdx_ = 0;
            } else if (bufIdx_ == 2) {
                flushBlock(1);
                bufIdx_ = 0;
            }
        }
    }

    void finish() {
        if (bufIdx_ >= 2) {
            flushBlock(bufIdx_ - 1);
            bufIdx_ = 0;
        }
    }

private:
    ChunkCallback cb_;
    uint8_t buf_[4];
    uint8_t bufIdx_;
    size_t totalDecodedBytes_;

    static int decodeChar(char c) {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    }

    void flushBlock(size_t outBytes) {
        uint8_t raw[3];
        raw[0] = (buf_[0] << 2) | ((buf_[1] >> 4) & 0x03);
        if (outBytes >= 2) raw[1] = ((buf_[1] & 0x0f) << 4) | ((buf_[2] >> 2) & 0x0f);
        if (outBytes >= 3) raw[2] = ((buf_[2] & 0x03) << 6) | (buf_[3] & 0x3f);

        for (size_t i = 0; i < outBytes; ++i) {
            totalDecodedBytes_++;
            // 最初の44バイトはWAV RIFFヘッダーなのでスキップ
            if (totalDecodedBytes_ > 44) {
                if (cb_) {
                    cb_(&raw[i], 1);
                }
            }
        }
    }
};

// TTSストリーミングレスポンス処理用Sink
class TTSResponseSink : public Stream {
public:
    explicit TTSResponseSink(AudioChunkCallback onAudio)
        : onAudio_(onAudio), inBase64Data_(false), state_(0), matchStage_(0), totalPcmBytes_(0),
          decoder_([this](const uint8_t* pcm, size_t len) {
              // 512バイトごとにまとめてコールバック
              for (size_t i = 0; i < len; ++i) {
                  pcmBuffer_[pcmBufIdx_++] = pcm[i];
                  totalPcmBytes_++;
                  if (pcmBufIdx_ >= sizeof(pcmBuffer_)) {
                      if (onAudio_) onAudio_(pcmBuffer_, pcmBufIdx_);
                      pcmBufIdx_ = 0;
                  }
              }
          }) {}

    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        for (size_t i = 0; i < size; ++i) {
            char c = static_cast<char>(data[i]);
            if (!inBase64Data_) {
                // "data" キーを検索
                static const char KEY[] = "\"data\"";
                if (matchStage_ == 0) {
                    if (c == KEY[state_]) {
                        state_++;
                        if (state_ == strlen(KEY)) {
                            matchStage_ = 1; // "data" 一致、次は ':' を待つ
                            state_ = 0;
                        }
                    } else {
                        state_ = (c == KEY[0]) ? 1 : 0;
                    }
                } else if (matchStage_ == 1) {
                    if (c == ':') {
                        matchStage_ = 2; // ':' 検出、次は '"' を待つ
                    } else if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                        matchStage_ = 0;
                        state_ = 0;
                    }
                } else if (matchStage_ == 2) {
                    if (c == '"') {
                        inBase64Data_ = true; // Base64音声データ開始
                        matchStage_ = 0;
                        state_ = 0;
                    } else if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
                        matchStage_ = 0;
                        state_ = 0;
                    }
                }
            } else {
                if (c == '"') {
                    // Base64データ終端
                    inBase64Data_ = false;
                    decoder_.finish();
                    if (pcmBufIdx_ > 0 && onAudio_) {
                        onAudio_(pcmBuffer_, pcmBufIdx_);
                        pcmBufIdx_ = 0;
                    }
                } else if (c != '\r' && c != '\n' && c != ' ') {
                    decoder_.write(c);
                }
            }
        }
        return size;
    }

    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    size_t getPcmBytes() const { return totalPcmBytes_; }

private:
    AudioChunkCallback onAudio_;
    bool inBase64Data_;
    size_t state_;
    uint8_t matchStage_ = 0;
    size_t totalPcmBytes_;
    uint8_t pcmBuffer_[512];
    size_t pcmBufIdx_ = 0;
    Base64StreamDecoder decoder_;
};

// HTTPClient removes HTTP chunk framing before writing decoded bytes here.
class ResponseSink : public Stream {
public:
    explicit ResponseSink(std::function<void(const String&)> onEvent = nullptr)
        : onEvent_(onEvent) {}
    size_t write(uint8_t byte) override { return write(&byte, 1); }
    size_t write(const uint8_t* data, size_t size) override {
        for (size_t i = 0; i < size; ++i) {
            if (overflow_) return i;
            const char ch = static_cast<char>(data[i]);
            if (!onEvent_) {
                if (body_.length() >= 32768) { overflow_ = true; return i; }
                body_ += ch;
            } else if (ch == '\n') {
                consumeLine();
            } else if (ch != '\r') {
                if (line_.length() >= 65536) { overflow_ = true; return i; }
                line_ += ch;
            }
        }
        return size;
    }
    int available() override { return 0; }
    int read() override { return -1; }
    int peek() override { return -1; }
    void flush() override {}
    bool complete() const { return !overflow_; }
    const String& body() const { return body_; }
private:
    void consumeLine() {
        if (line_.isEmpty()) {
            if (!event_.isEmpty()) { onEvent_(event_); event_ = ""; }
        } else if (line_.startsWith("data:")) {
            String data = line_.substring(5);
            if (data.startsWith(" ")) data.remove(0, 1);
            if (event_.length() + data.length() + 1 > 65536) { overflow_ = true; }
            else { if (!event_.isEmpty()) event_ += '\n'; event_ += data; }
        }
        line_ = "";
    }
    std::function<void(const String&)> onEvent_;
    String line_, event_, body_;
    bool overflow_ = false;
};

// Only fixed categories and uppercase API enums may reach serial output.
String safeApiEnum(const char* value) {
    if (!value || !*value || strlen(value) > 80) return "UNKNOWN";
    for (const char* p = value; *p; ++p) {
        if (!((*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') || *p == '_')) return "UNKNOWN";
    }
    return String(value);
}
void reportApiError(const String& body) {
    JsonDocument errorDoc;
    if (deserializeJson(errorDoc, body)) {
        Serial.println("[Gemini] API error category=UNREADABLE_ERROR_BODY");
        return;
    }
    String status = safeApiEnum(errorDoc["error"]["status"] | "UNKNOWN");
    String reason = "UNKNOWN";
    for (JsonObject detail : errorDoc["error"]["details"].as<JsonArray>()) {
        if (detail["reason"].is<const char*>()) reason = safeApiEnum(detail["reason"]);
    }
    String message = errorDoc["error"]["message"] | "";
    String category = "UNKNOWN";
    String lower = message;
    lower.toLowerCase();
    if (lower.indexOf("overloaded") >= 0 || lower.indexOf("high demand") >= 0) category = "MODEL_OVERLOADED";
    else if (lower.indexOf("quota") >= 0 || lower.indexOf("rate-limit") >= 0) category = "QUOTA_EXCEEDED";
    else if (lower.indexOf("unavailable") >= 0) category = "SERVICE_UNAVAILABLE";
    else if (message.indexOf("reported as leaked") >= 0) category = "API_KEY_LEAKED";
    else if (message.indexOf("unrestricted") >= 0) category = "API_KEY_UNRESTRICTED";
    else if (message.indexOf("denied access") >= 0) category = "PROJECT_ACCESS_DENIED";
    else if (message.indexOf("referer") >= 0 || message.indexOf("referrer") >= 0) category = "REFERRER_RESTRICTION";
    else if (message.indexOf("API key not valid") >= 0) category = "API_KEY_INVALID";
    else if (message.indexOf("unregistered callers") >= 0) category = "API_KEY_MISSING_OR_INVALID";
    else if (message.indexOf("disabled") >= 0) category = "SERVICE_OR_KEY_DISABLED";
    Serial.printf("[Gemini] API error status=%s reason=%s category=%s\n", status.c_str(), reason.c_str(), category.c_str());
}
}

String GeminiClient::s_apiKey = "";
String GeminiClient::s_model = "gemini-3.5-flash";
String GeminiClient::s_voice = "Kore";

const char* GEMINI_HOST = "generativelanguage.googleapis.com";

void GeminiClient::init(const String& apiKey, const String& model, const String& voice) {
    s_apiKey = apiKey;
    s_model = model.isEmpty() ? "gemini-3.5-flash" : model;
    s_voice = voice.isEmpty() ? "Kore" : voice;
}

AvatarEmotion GeminiClient::parseEmotionTag(const String& tag) {
    if (tag.indexOf("HAPPY") >= 0) return EMOTION_HAPPY;
    if (tag.indexOf("SURPRISED") >= 0) return EMOTION_SURPRISED;
    if (tag.indexOf("THINKING") >= 0) return EMOTION_THINKING;
    if (tag.indexOf("SAD") >= 0) return EMOTION_SAD;
    if (tag.indexOf("ANGRY") >= 0) return EMOTION_ANGRY;
    if (tag.indexOf("SLEEP") >= 0) return EMOTION_SLEEP;
    return EMOTION_NORMAL;
}

String GeminiClient::extractCleanText(const String& rawText) {
    String clean = rawText;
    int tagStart = clean.indexOf("[EMOTION:");
    if (tagStart >= 0) {
        int tagEnd = clean.indexOf("]", tagStart);
        if (tagEnd > tagStart) {
            clean.remove(tagStart, (tagEnd - tagStart) + 1);
        }
    }
    clean.trim();
    return clean;
}

// 1. テキスト対話ストリーミング + TTS音声合成
bool GeminiClient::sendUserPromptStream(
    const String& prompt,
    EmotionCallback onEmotion,
    TokenCallback onToken,
    AudioChunkCallback onAudio
) {
    if (s_apiKey.isEmpty()) {
        Serial.println("[Gemini] Stream failed. reason=API_KEY_MISSING");
        return false;
    }

    String modelToUse = s_model;
    // gemini-3.8-flashはFree tierの20回/日制限があるため、gemini-3.5-flashへフォールバック
    if (modelToUse == "gemini-3.8-flash") {
        modelToUse = "gemini-3.5-flash";
    }

    WiFiClientSecure client;
    client.setInsecure(); // ESP32のTLSハンドシェイク高速化
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(30000);
    http.setReuse(false);

    String url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":streamGenerateContent?alt=sse";
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", s_apiKey);
    const char* responseHeaders[] = {"Content-Type"};
    http.collectHeaders(responseHeaders, 1);

    JsonDocument doc;
    JsonArray contents = doc["contents"].to<JsonArray>();
    JsonObject part = contents.add<JsonObject>()["parts"].add<JsonObject>();
    part["text"] = prompt;

    JsonObject sysInst = doc["systemInstruction"].to<JsonObject>();
    sysInst["parts"].add<JsonObject>()["text"] =
        "You are Stack-chan, an adorable desktop companion robot living on an M5Stack CoreS3 Lite. "
        "Respond in cheerful Japanese in 1-2 brief sentences. "
        "Prefix response with one tag: [EMOTION:HAPPY], [EMOTION:SURPRISED], [EMOTION:THINKING], [EMOTION:SAD], [EMOTION:ANGRY], [EMOTION:SLEEP], [EMOTION:NORMAL].";

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int status = http.POST(jsonPayload);
    Serial.printf("[Gemini] HTTP status=%d (model=%s)\n", status, modelToUse.c_str());

    // 429または503の場合はgemini-flash-latestへ自動リトライ、さらに必要ならgemini-3.1-flash-lite
    if ((status == 429 || status == 503) && modelToUse != "gemini-flash-latest") {
        http.end();
        Serial.println("[Gemini] Retrying with model: gemini-flash-latest...");
        modelToUse = "gemini-flash-latest";
        url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":streamGenerateContent?alt=sse";
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("x-goog-api-key", s_apiKey);
            http.collectHeaders(responseHeaders, 1);
            status = http.POST(jsonPayload);
            Serial.printf("[Gemini] Retry HTTP status=%d (model=%s)\n", status, modelToUse.c_str());
        }
    }
    if ((status == 429 || status == 503) && modelToUse != "gemini-3.1-flash-lite") {
        http.end();
        Serial.println("[Gemini] Retrying with model: gemini-3.1-flash-lite...");
        modelToUse = "gemini-3.1-flash-lite";
        url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":streamGenerateContent?alt=sse";
        if (http.begin(client, url)) {
            http.addHeader("Content-Type", "application/json");
            http.addHeader("x-goog-api-key", s_apiKey);
            http.collectHeaders(responseHeaders, 1);
            status = http.POST(jsonPayload);
            Serial.printf("[Gemini] Retry HTTP status=%d (model=%s)\n", status, modelToUse.c_str());
        }
    }

    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        Serial.println("[Gemini] Stream failed. HTTP request rejected.");
        return false;
    }

    bool emotionFound = false;
    String fullReply = "";
    bool parseFailed = false;
    bool apiFailed = false;
    size_t textBytes = 0;

    ResponseSink sink([&](const String& jsonData) {
        if (jsonData == "[DONE]") return;
        JsonDocument chunkDoc;
        if (deserializeJson(chunkDoc, jsonData)) { parseFailed = true; return; }
        if (!chunkDoc["error"].isNull()) { apiFailed = true; return; }
        for (JsonObject p : chunkDoc["candidates"][0]["content"]["parts"].as<JsonArray>()) {
            if ((p["thought"] | false) || !p["text"].is<const char*>()) continue;
            String chunkStr = p["text"].as<String>();
            textBytes += chunkStr.length();
            fullReply += chunkStr;

            if (!emotionFound && fullReply.length() < 256) {
                int tagStart = fullReply.indexOf("[EMOTION:");
                int tagEnd = tagStart >= 0 ? fullReply.indexOf("]", tagStart) : -1;
                if (tagEnd > tagStart && tagStart >= 0) {
                    if (onEmotion) onEmotion(parseEmotionTag(fullReply.substring(tagStart, tagEnd + 1)));
                    emotionFound = true;
                }
            }
            if (onToken) onToken(chunkStr);
        }
    });

    const int transferred = http.writeToStream(&sink);
    http.end();

    if (transferred < 0 || parseFailed || apiFailed || textBytes == 0) {
        Serial.printf("[Gemini] Stream failed. Transport=%d parse=%u api=%u text_bytes=%u\n",
                      transferred, parseFailed, apiFailed, static_cast<unsigned>(textBytes));
        return false;
    }

    Serial.printf("[Gemini] Stream complete. Text bytes=%u\n", static_cast<unsigned>(textBytes));

    // 音声コールバックがあればTTSで音声を合成・再生
    String cleanSpeech = extractCleanText(fullReply);
    if (!cleanSpeech.isEmpty() && onAudio) {
        Serial.printf("[TTS] Synthesizing speech for: \"%s\" (voice=%s)...\n",
                      cleanSpeech.c_str(), s_voice.c_str());
        generateTTS(cleanSpeech, onAudio);
    }

    return true;
}

// 2. 実機マイク録音PCM (16kHz mono) によるマルチモーダル音声対話 (STT + LLM + TTS)
bool GeminiClient::sendUserAudioDialogue(
    const int16_t* pcmSamples,
    size_t sampleCount,
    EmotionCallback onEmotion,
    TokenCallback onToken,
    AudioChunkCallback onAudio
) {
    if (s_apiKey.isEmpty()) {
        Serial.println("[Gemini] Audio dialogue failed. reason=API_KEY_MISSING");
        return false;
    }
    if (!pcmSamples || sampleCount < 1600) {
        Serial.println("[Gemini] Audio sample too short, falling back to text prompt.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    // 16kHz mono 16-bit PCM を WAVフォーマットにパック
    const uint32_t pcmBytes = sampleCount * sizeof(int16_t);
    const uint32_t wavBytes = 44 + pcmBytes;
    const size_t b64Bytes = ((wavBytes + 2) / 3) * 4;

    uint8_t* wavBuffer = (uint8_t*)heap_caps_malloc(wavBytes, MALLOC_CAP_SPIRAM);
    if (!wavBuffer) {
        Serial.println("[Gemini] PSRAM allocation failed for wavBuffer.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    buildWavHeader(wavBuffer, pcmBytes, 16000);
    memcpy(wavBuffer + 44, pcmSamples, pcmBytes);

    char* b64Audio = (char*)heap_caps_malloc(b64Bytes + 1, MALLOC_CAP_SPIRAM);
    if (!b64Audio) {
        heap_caps_free(wavBuffer);
        Serial.println("[Gemini] PSRAM allocation failed for b64Audio.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    encodeBase64(wavBuffer, wavBytes, b64Audio);
    heap_caps_free(wavBuffer); // WAVバッファは即時解放

    // JSONペイロード構築
    const char jsonPrefix[] =
        "{\"contents\":[{\"parts\":[{\"inlineData\":{\"mimeType\":\"audio/wav\",\"data\":\"";
    const char jsonSuffix[] =
        "\"}},{\"text\":\"ユーザーの音声を聴いて、M5Stack CoreS3のスタックちゃんとして1〜2文の短い日本語で答えてください。返答の先頭に必ず[EMOTION:HAPPY]などの感情タグをつけてください。\"}]}]}";

    const size_t prefixLen = strlen(jsonPrefix);
    const size_t suffixLen = strlen(jsonSuffix);
    const size_t totalJsonLen = prefixLen + b64Bytes + suffixLen;

    char* jsonPayload = (char*)heap_caps_malloc(totalJsonLen + 1, MALLOC_CAP_SPIRAM);
    if (!jsonPayload) {
        heap_caps_free(b64Audio);
        Serial.println("[Gemini] PSRAM allocation failed for jsonPayload.");
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    memcpy(jsonPayload, jsonPrefix, prefixLen);
    memcpy(jsonPayload + prefixLen, b64Audio, b64Bytes);
    memcpy(jsonPayload + prefixLen + b64Bytes, jsonSuffix, suffixLen + 1);
    heap_caps_free(b64Audio); // Base64バッファは即時解放

    String modelToUse = s_model;
    if (modelToUse == "gemini-3.8-flash") {
        modelToUse = "gemini-3.5-flash";
    }

    Serial.printf("[Gemini] Sending %u bytes of audio (%u ms) to %s...\n",
                  static_cast<unsigned>(pcmBytes),
                  static_cast<unsigned>(sampleCount * 1000 / 16000),
                  modelToUse.c_str());

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(30000);
    http.setReuse(false);

    String url = String("https://") + GEMINI_HOST + "/v1beta/models/" + modelToUse + ":generateContent";
    if (!http.begin(client, url)) {
        heap_caps_free(jsonPayload);
        return false;
    }

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", s_apiKey);

    int status = http.sendRequest("POST", (uint8_t*)jsonPayload, totalJsonLen);
    heap_caps_free(jsonPayload); // 送信後は即時解放
    Serial.printf("[Gemini] Audio dialogue HTTP status=%d\n", status);

    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        // 失敗時はテキストプロンプトでフォールバック
        return sendUserPromptStream("こんにちは！元気？", onEmotion, onToken, onAudio);
    }

    ResponseSink sink;
    http.writeToStream(&sink);
    http.end();

    JsonDocument resDoc;
    if (deserializeJson(resDoc, sink.body())) {
        Serial.println("[Gemini] JSON parsing error on dialogue response.");
        return false;
    }

    const char* replyCStr = resDoc["candidates"][0]["content"]["parts"][0]["text"] | "";
    String replyText = String(replyCStr);
    if (replyText.isEmpty()) {
        Serial.println("[Gemini] Empty reply received.");
        return false;
    }

    Serial.printf("[Gemini] Reply: %s\n", replyText.c_str());

    // 感情タグ抽出
    int tagStart = replyText.indexOf("[EMOTION:");
    if (tagStart >= 0) {
        int tagEnd = replyText.indexOf("]", tagStart);
        if (tagEnd > tagStart) {
            if (onEmotion) onEmotion(parseEmotionTag(replyText.substring(tagStart, tagEnd + 1)));
        }
    } else {
        if (onEmotion) onEmotion(EMOTION_HAPPY);
    }

    String cleanSpeech = extractCleanText(replyText);
    if (onToken) onToken(cleanSpeech);

    // TTS音声合成・再生
    if (!cleanSpeech.isEmpty() && onAudio) {
        Serial.printf("[TTS] Synthesizing speech for: \"%s\" (voice=%s)...\n",
                      cleanSpeech.c_str(), s_voice.c_str());
        generateTTS(cleanSpeech, onAudio);
    }

    return true;
}

// 3. テキストから24kHz PCM音声を合成 (gemini-3.8-flash-lite-tts)
bool GeminiClient::generateTTS(const String& text, AudioChunkCallback onAudio) {
    if (s_apiKey.isEmpty() || text.isEmpty() || !onAudio) {
        return false;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setConnectTimeout(8000);
    http.setTimeout(25000);
    http.setReuse(false);

    String url = String("https://") + GEMINI_HOST + "/v1beta/models/gemini-3.8-flash-lite-tts:generateContent";
    if (!http.begin(client, url)) return false;

    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", s_apiKey);

    JsonDocument doc;
    JsonObject part = doc["contents"].to<JsonArray>().add<JsonObject>()["parts"].add<JsonObject>();
    part["text"] = text;

    JsonObject genCfg = doc["generationConfig"].to<JsonObject>();
    genCfg["responseModalities"].to<JsonArray>().add("AUDIO");
    genCfg["speechConfig"]["voiceConfig"]["prebuiltVoiceConfig"]["voiceName"] = s_voice;

    String jsonPayload;
    serializeJson(doc, jsonPayload);

    int status = http.POST(jsonPayload);
    Serial.printf("[TTS] HTTP status=%d\n", status);

    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        return false;
    }

    // ストリーミングデコードでPCMを取り出して即時再生リングバッファへ供給
    TTSResponseSink ttsSink(onAudio);
    http.writeToStream(&ttsSink);
    http.end();

    Serial.printf("[TTS] Completed! Decoded PCM bytes=%u (~%u ms @ 24kHz)\n",
                  static_cast<unsigned>(ttsSink.getPcmBytes()),
                  static_cast<unsigned>(ttsSink.getPcmBytes() * 1000 / (24000 * 2)));

    return ttsSink.getPcmBytes() > 0;
}
