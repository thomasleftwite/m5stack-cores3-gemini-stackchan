#include "GeminiClient.h"
#include "GeminiModels.h"
#include "ConversationRules.h"
#include "ReplyRetry.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <mbedtls/base64.h>
#include <memory>
#include <algorithm>
extern const uint8_t googleRoots[] asm("_binary_certs_google_roots_pem_start");
namespace {
class PsAllocator : public ArduinoJson::Allocator {
 public:
    void* allocate(size_t n) override { return heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
    void deallocate(void* p) override { heap_caps_free(p); }
    void* reallocate(void* p, size_t n) override { return heap_caps_realloc(p, n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); }
} allocator;
struct Free { void operator()(uint8_t* p) const { heap_caps_free(p); } };
using Buffer = std::unique_ptr<uint8_t, Free>;
Buffer buffer(size_t n) { return Buffer((uint8_t*)allocator.allocate(n)); }
std::atomic<GeminiClient::Stage> state{GeminiClient::Idle};
std::atomic<bool> running{false}, cancelled{false}, transcribed{false};
std::atomic<bool> newQuestion{false};
std::atomic<int> httpCode{0};
const int16_t* input = nullptr;
size_t inputCount = 0;
String key, voice, dateTime, transcript, reply;
String historyUser[3], historyReply[3];
unsigned historyCount = 0;
Buffer receivedAudio;
conversation::WavView audioView;
AvatarEmotion currentEmotion = EMOTION_NORMAL;
const char* failure = "";
uint32_t retryAfter = 0;
bool automaticRetryHeaderValid = true;
constexpr size_t MaxBody = 1400000;

class Sink : public Stream {
 public:
    Buffer data; size_t used = 0, cap; bool failed = false;
    explicit Sink(size_t capacity) : data(buffer(capacity + 1)), cap(capacity) { if (!data) failed = true; }
    size_t write(uint8_t b) override { return write(&b, 1); }
    size_t write(const uint8_t* p, size_t n) override {
        if (failed || cancelled.load() || n > cap - used) { failed = true; return 0; }
        memcpy(data.get() + used, p, n); used += n; data.get()[used] = 0; return n;
    }
    int available() override { return 0; } int read() override { return -1; }
    int peek() override { return -1; } void flush() override {}
};
bool post(const char* model, JsonDocument& request, JsonDocument& response, size_t cap, uint32_t timeoutMs = 15000) {
    if (cancelled.load()) return false;
    httpCode.store(0);
    automaticRetryHeaderValid = true;
    const uint32_t attemptStarted = millis();
    if (WiFi.status() != WL_CONNECTED) { failure = "WiFi disconnected"; return false; }
    if (time(nullptr) < 1700000000) { failure = "Clock not synchronized"; return false; }
    if (request.overflowed()) { failure = "Request memory unavailable"; return false; }
    size_t length = measureJson(request); auto payload = buffer(length + 1);
    if (!payload) { failure = "Request memory unavailable"; return false; }
    serializeJson(request, (char*)payload.get(), length + 1);
    WiFiClientSecure tls; tls.setCACert((const char*)googleRoots);
    tls.setHandshakeTimeout(std::min(uint32_t(8), (timeoutMs + 999) / 1000));
    HTTPClient http; http.setConnectTimeout(std::min(uint32_t(8000), timeoutMs));
    http.setTimeout(uint16_t(std::min(uint32_t(15000), timeoutMs))); http.setReuse(false);
    String url = String("https://generativelanguage.googleapis.com/v1beta/models/") + model + ":generateContent";
    if (!http.begin(tls, url)) { failure = "TLS setup failed"; return false; }
    http.addHeader("Content-Type", "application/json"); http.addHeader("x-goog-api-key", key);
    const char* headers[] = {"Retry-After", "Content-Type"}; http.collectHeaders(headers, 2);
    int code = http.POST(payload.get(), length); payload.reset(); httpCode.store(code);
    Serial.printf("[API] stage=%d status=%d elapsed_ms=%u\n", int(state.load()), code, unsigned(millis() - attemptStarted));
    if (cancelled.load()) { http.end(); return false; }
    if (code != 200) {
        String retryHeader = http.header("Retry-After"); retryHeader.trim();
        for (size_t i = 0; i < retryHeader.length(); ++i)
            if (retryHeader[i] < '0' || retryHeader[i] > '9') automaticRetryHeaderValid = false;
        int wait = 0;
        if (automaticRetryHeaderValid) for (size_t i = 0; i < retryHeader.length(); ++i)
            wait = std::min(3600, wait * 10 + retryHeader[i] - '0');
        retryAfter = millis() + uint32_t(std::min(3600, std::max(code == 429 ? 60 : 2, wait))) * 1000;
        failure = code == 503 ? "Gemini temporarily overloaded; retry later" : code == 429 ? "Free quota reached; retry later" : code == 403 ? "API access denied" : code == 404 ? "Model unavailable" : "API request failed";
        http.end(); return false;
    }
    if (!http.header("Content-Type").startsWith("application/json")) { http.end(); failure = "Unexpected response type"; return false; }
    int expected = http.getSize();
    if (expected > int(cap)) { http.end(); failure = "Response too large"; return false; }
    Sink sink(cap);
    int count = sink.failed ? -1 : http.writeToStream(&sink);
    http.end();
    if (cancelled.load()) return false;
    if (count < 0 || sink.failed || !sink.used || (expected >= 0 && sink.used != size_t(expected)) ||
        deserializeJson(response, (const char*)sink.data.get(), sink.used)) { failure = "Incomplete API response"; return false; }
    if (response.overflowed() || response["error"].is<JsonObject>()) { failure = "Invalid API response"; return false; }
    retryAfter = 0;
    failure = "";
    return true;
}
bool completeCandidate(JsonDocument& response) {
    return response["candidates"][0]["finishReason"].is<const char*>() &&
        String(response["candidates"][0]["finishReason"].as<const char*>()) == "STOP";
}
String textResult(JsonDocument& response) {
    if (!completeCandidate(response)) return "";
    String text;
    for (JsonObjectConst part : response["candidates"][0]["content"]["parts"].as<JsonArrayConst>())
        if (!(part["thought"] | false) && part["text"].is<const char*>()) text += part["text"].as<const char*>();
    text.trim(); return text;
}
struct ReplyTransport {
    JsonDocument& request;
    JsonDocument& response;
    uint32_t began;
    ReplyTransport(JsonDocument& req, JsonDocument& res) : request(req), response(res), began(millis()) { }
    bool attempt() {
        const uint32_t elapsed = millis() - began;
        if (elapsed >= replyretry::RetryStartBudgetMs) return false;
        response.clear();
        return post(geminimodels::Reply, request, response, 16000, replyretry::RetryStartBudgetMs - elapsed);
    }
    int status() const { return httpCode.load(); }
    uint32_t nowMillis() const { return millis(); }
    uint32_t retryWaitMs() const { return automaticRetryHeaderValid ? GeminiClient::retryWaitMs() : replyretry::MaxAutomaticWaitMs + 1; }
    uint32_t jitterMs() const { return esp_random() % 501; }
    bool cancelled() const { return ::cancelled.load(); }
    void onRetry() { state.store(GeminiClient::ReplyRetry); }
    bool wait(uint32_t duration) {
        Serial.printf("[API] reply_retry_wait_ms=%u\n", unsigned(duration));
        uint32_t start = millis();
        while (millis() - start < duration) { if (cancelled()) return false; vTaskDelay(pdMS_TO_TICKS(20)); }
        return !cancelled();
    }
};
void textConfig(JsonDocument& request, int maxTokens) {
    request["generationConfig"]["responseMimeType"] = "application/json";
    request["generationConfig"]["maxOutputTokens"] = maxTokens;
    request["generationConfig"]["thinkingConfig"]["thinkingLevel"] = "low";
}
String limitText(String value, size_t characters) {
    size_t pos = 0, count = 0;
    while (pos < value.length() && count < characters) {
        uint8_t c = value[pos]; size_t n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : 4;
        if (pos + n > value.length()) break;
        pos += n; ++count;
    }
    return value.substring(0, pos);
}
}
void GeminiClient::init(const String& apiKey, const String& chosenVoice) { key = apiKey; voice = chosenVoice; }
bool GeminiClient::start(const int16_t* pcm, size_t samples, const String& now) {
    if (busy()) return false;
    clear(); input = pcm; inputCount = samples; dateTime = now;
    if (!input || samples < 3200 || samples > conversation::InputSamples || key.isEmpty()) { failure = "Audio or API key missing"; state.store(Failed); return false; }
    return retry();
}
bool GeminiClient::retry() {
    if (busy()) return false;
    if (key.isEmpty() || (transcript.isEmpty() && (!input || inputCount < 3200))) { failure = "Audio or API key missing"; state.store(Failed); return false; }
    if (retryWaitMs()) { failure = "Please wait before retry"; state.store(Failed); return false; }
    cancelled.store(false); failure = ""; httpCode.store(0); running.store(true);
    state.store(transcript.isEmpty() ? STT : reply.isEmpty() ? Reply : TTS);
    if (xTaskCreatePinnedToCore(worker, "GeminiFile", 16384, nullptr, 1, nullptr, 0) != pdPASS) {
        running.store(false); failure = "Worker unavailable"; state.store(Failed); return false;
    }
    return true;
}
void GeminiClient::worker(void*) {
    bool ok = true;
    if (transcript.isEmpty() && !cancelled.load()) {
        state.store(STT);
        size_t wavSize = 44 + inputCount * 2, encodedSize = ((wavSize + 2) / 3) * 4;
        auto wav = buffer(wavSize), encoded = buffer(encodedSize + 1);
        if (!wav || !encoded) { ok = false; failure = "Recording memory unavailable"; }
        else {
            conversation::wavHeader(wav.get(), inputCount * 2); memcpy(wav.get() + 44, input, inputCount * 2);
            size_t n = 0;
            ok = mbedtls_base64_encode(encoded.get(), encodedSize + 1, &n, wav.get(), wavSize) == 0; wav.reset();
            if (ok) {
                encoded.get()[n] = 0;
                JsonDocument request(&allocator), response(&allocator);
                request["contents"][0]["parts"][0]["inlineData"]["mimeType"] = "audio/wav";
                request["contents"][0]["parts"][0]["inlineData"]["data"] = (const char*)encoded.get();
                request["contents"][0]["parts"][1]["text"] = "音声を忠実に日本語で文字起こししてください。質問への回答、要約、推測による補完は禁止。無音はsilence、認識不能はunclear。JSONのみ: {\"status\":\"ok|silence|unclear\",\"transcript\":\"認識文\"}";
                textConfig(request, 1024);
                ok = post(geminimodels::Stt, request, response, 16000);
                if (ok) {
                    JsonDocument result(&allocator);
                    ok = !deserializeJson(result, textResult(response));
                    if (ok) {
                        transcript = result["transcript"] | ""; transcript.trim();
                        ok = String(result["status"] | "") == "ok" && !transcript.isEmpty() && transcript.length() <= 4096;
                        if (ok) transcribed.store(true);
                    }
                    if (!ok) { transcript = ""; newQuestion.store(true); failure = "Speech unclear; please ask again"; }
                }
            }
        }
    }
    if (ok && reply.isEmpty() && !cancelled.load()) {
        state.store(Reply);
        JsonDocument request(&allocator), response(&allocator);
        request["systemInstruction"]["parts"][0]["text"] = "あなたはスタックチャン。親しみやすい日本語1～2文、60文字以内で答える。検索機能はないため最新情報を作らない。JSONのみ返す: {\"emotion\":\"NORMAL|HAPPY|SAD|ANGRY|SURPRISED|THINKING\",\"text\":\"読み上げ本文\"}。タグや絵文字や箇条書きを本文に含めない。現在時刻: " + dateTime;
        unsigned at = 0;
        for (unsigned i = 0; i < historyCount; ++i) {
            request["contents"][at]["role"] = "user"; request["contents"][at++]["parts"][0]["text"] = historyUser[i];
            request["contents"][at]["role"] = "model"; request["contents"][at++]["parts"][0]["text"] = historyReply[i];
        }
        request["contents"][at]["role"] = "user"; request["contents"][at]["parts"][0]["text"] = transcript;
        textConfig(request, 2048);
        ReplyTransport transport{request, response};
        ok = replyretry::execute(transport);
        if (ok) {
            JsonDocument result(&allocator); ok = !deserializeJson(result, textResult(response));
            if (ok) {
                reply = result["text"] | ""; reply.trim(); reply = limitText(reply, 60); ok = !reply.isEmpty();
                String e = result["emotion"] | "NORMAL";
                currentEmotion = e == "HAPPY" ? EMOTION_HAPPY : e == "SAD" ? EMOTION_SAD : e == "ANGRY" ? EMOTION_ANGRY : e == "SURPRISED" ? EMOTION_SURPRISED : e == "THINKING" ? EMOTION_THINKING : EMOTION_NORMAL;
            }
            if (!ok) failure = "Answer unavailable";
        }
    }
    if (ok && !cancelled.load()) {
        state.store(TTS);
        JsonDocument request(&allocator), response(&allocator);
        request["contents"][0]["parts"][0]["text"] = reply;
        request["generationConfig"]["responseModalities"][0] = "AUDIO";
        request["generationConfig"]["speechConfig"]["voiceConfig"]["prebuiltVoiceConfig"]["voiceName"] = voice;
        ok = post(geminimodels::Tts, request, response, MaxBody);
        if (ok) {
            ok = completeCandidate(response); unsigned blocks = 0;
            for (JsonObjectConst part : response["candidates"][0]["content"]["parts"].as<JsonArrayConst>()) {
                if ((part["thought"] | false) || !part["inlineData"].is<JsonObjectConst>()) continue;
                ++blocks;
                String type = part["inlineData"]["mimeType"] | "";
                const char* encoded = part["inlineData"]["data"] | "";
                size_t encodedSize = strlen(encoded), decodedSize = 0;
                if (type != "audio/wav" || !encodedSize || encodedSize > 1300000) { ok = false; break; }
                auto wav = buffer(encodedSize / 4 * 3 + 4);
                if (!wav || mbedtls_base64_decode(wav.get(), encodedSize / 4 * 3 + 4, &decodedSize, (const uint8_t*)encoded, encodedSize) ||
                    !conversation::validateWav(wav.get(), decodedSize, audioView)) { ok = false; break; }
                receivedAudio = std::move(wav);
            }
            ok = ok && blocks == 1 && receivedAudio != nullptr;
            if (!ok) { receivedAudio.reset(); failure = "Incomplete or unsupported audio file"; }
        }
    }
    if (cancelled.load()) { receivedAudio.reset(); state.store(Cancelled); }
    else if (ok) {
        if (historyCount == 3) { for (unsigned i = 0; i < 2; ++i) { historyUser[i] = historyUser[i+1]; historyReply[i] = historyReply[i+1]; } --historyCount; }
        historyUser[historyCount] = transcript; historyReply[historyCount++] = reply;
        state.store(Complete);
    } else state.store(Failed);
    running.store(false); vTaskDelete(nullptr);
}
void GeminiClient::cancel() { cancelled.store(true); }
bool GeminiClient::busy() { return running.load(); }
GeminiClient::Stage GeminiClient::stage() { return state.load(); }
int GeminiClient::httpStatus() { return httpCode.load(); }
uint32_t GeminiClient::retryWaitMs() {
    int32_t wait = int32_t(retryAfter - millis());
    if (retryAfter && wait > 0) return uint32_t(wait);
    retryAfter = 0; return 0;
}
const char* GeminiClient::error() { return busy() ? "" : failure; }
bool GeminiClient::hasTranscript() { return transcribed.load(); }
bool GeminiClient::needsNewQuestion() { return newQuestion.load(); }
const int16_t* GeminiClient::audio(size_t* count) { *count = audioView.bytes / 2; return receivedAudio ? (const int16_t*)(receivedAudio.get() + audioView.offset) : nullptr; }
AvatarEmotion GeminiClient::emotion() { return currentEmotion; }
void GeminiClient::clear(bool clearHistory) {
    if (busy()) return;
    receivedAudio.reset(); audioView = {}; transcript = ""; reply = ""; transcribed.store(false);
    newQuestion.store(false);
    input = nullptr; inputCount = 0; dateTime = ""; cancelled.store(false); state.store(Idle); failure = "";
    if (clearHistory) { for (unsigned i = 0; i < 3; ++i) { historyUser[i] = ""; historyReply[i] = ""; } historyCount = 0; }
}
