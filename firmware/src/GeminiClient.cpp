#include "GeminiClient.h"
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <functional>

namespace {
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
                if (body_.length() >= 16384) { overflow_ = true; return i; }
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
    bool complete() const { return !overflow_ && line_.isEmpty() && event_.isEmpty(); }
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
String GeminiClient::s_model = "gemini-3.8-flash";
String GeminiClient::s_voice = "Kore";

const char* GEMINI_HOST = "generativelanguage.googleapis.com";

void GeminiClient::init(const String& apiKey, const String& model, const String& voice) {
    s_apiKey = apiKey;
    s_model = model;
    s_voice = voice;
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
    WiFiClientSecure client;
    client.setInsecure(); // ESP32のTLSハンドシェイク高速化
    HTTPClient http;
    http.setConnectTimeout(10000);
    http.setTimeout(30000);
    http.setReuse(false);
    String url = String("https://") + GEMINI_HOST + "/v1beta/models/" + s_model + ":streamGenerateContent?alt=sse";
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

    const int status = http.POST(jsonPayload);
    Serial.printf("[Gemini] HTTP status=%d\n", status);
    if (status != HTTP_CODE_OK) {
        if (status > 0) {
            ResponseSink errorBody;
            http.writeToStream(&errorBody);
            reportApiError(errorBody.body());
        }
        http.end();
        Serial.println("[Gemini] Stream failed. HTTP request rejected or transport failed.");
        return false;
    }
    if (!http.header("Content-Type").startsWith("text/event-stream")) {
        http.end();
        Serial.println("[Gemini] Stream failed. Unexpected content type.");
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
        for (JsonObject part : chunkDoc["candidates"][0]["content"]["parts"].as<JsonArray>()) {
            if ((part["thought"] | false) || !part["text"].is<const char*>()) continue;
            String chunkStr = part["text"].as<String>();
            textBytes += chunkStr.length();
            // Keep only a bounded prefix to find a tag split across events.
            if (!emotionFound && fullReply.length() < 256) {
                fullReply += chunkStr.substring(0, 256 - fullReply.length());
                int tagStart = fullReply.indexOf("[EMOTION:");
                int tagEnd = tagStart >= 0 ? fullReply.indexOf("]", tagStart) : -1;
                if (tagEnd > tagStart && tagStart >= 0) {
                    if (onEmotion) onEmotion(parseEmotionTag(fullReply.substring(tagStart, tagEnd + 1)));
                    emotionFound = true;
                    fullReply = "";
                }
            }
            if (onToken) onToken(chunkStr);
        }
    });
    const int transferred = http.writeToStream(&sink);
    http.end();
    if (transferred < 0 || !sink.complete() || parseFailed || apiFailed || textBytes == 0) {
        Serial.printf("[Gemini] Stream failed. Transport=%d parse=%u api=%u text_bytes=%u\n",
                      transferred, parseFailed, apiFailed, static_cast<unsigned>(textBytes));
        return false;
    }
    Serial.printf("[Gemini] Stream complete. Text bytes=%u\n", static_cast<unsigned>(textBytes));
    return true;
}
