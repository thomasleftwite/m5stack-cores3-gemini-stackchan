#include "GeminiClient.h"
#include <ArduinoJson.h>

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
    WiFiClientSecure client;
    client.setInsecure(); // ESP32のTLSハンドシェイク高速化
    client.setTimeout(10000);

    if (!client.connect(GEMINI_HOST, 443)) {
        Serial.println("[Gemini] Direct connection to Google API failed");
        return false;
    }

    String url = "/v1beta/models/" + s_model + ":streamGenerateContent?alt=sse&key=" + s_apiKey;

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

    client.print(String("POST ") + url + " HTTP/1.1\r\n" +
                 "Host: " + GEMINI_HOST + "\r\n" +
                 "Content-Type: application/json\r\n" +
                 "User-Agent: aistudio-build\r\n" +
                 "Connection: close\r\n" +
                 "Content-Length: " + jsonPayload.length() + "\r\n\r\n" +
                 jsonPayload);

    while (client.connected()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) break;
    }

    bool emotionFound = false;
    String fullReply = "";

    while (client.connected() || client.available()) {
        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim();

            if (line.startsWith("data: ")) {
                String jsonData = line.substring(6);
                JsonDocument chunkDoc;
                DeserializationError err = deserializeJson(chunkDoc, jsonData);
                if (!err) {
                    const char* textChunk = chunkDoc["candidates"][0]["content"]["parts"][0]["text"];
                    if (textChunk) {
                        String chunkStr = String(textChunk);
                        fullReply += chunkStr;

                        if (!emotionFound) {
                            int tagStart = fullReply.indexOf("[EMOTION:");
                            int tagEnd = fullReply.indexOf("]", tagStart);
                            if (tagStart >= 0 && tagEnd > tagStart) {
                                String emotionTag = fullReply.substring(tagStart, tagEnd + 1);
                                AvatarEmotion emo = parseEmotionTag(emotionTag);
                                if (onEmotion) onEmotion(emo);
                                emotionFound = true;
                                fullReply = fullReply.substring(tagEnd + 1);
                            }
                        }

                        if (onToken) onToken(chunkStr);
                    }
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    client.stop();

    Serial.printf("[Gemini] Stream complete. Total reply: %s\n", fullReply.c_str());
    return true;
}
