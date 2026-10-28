#include <ArduinoJson.h>
#include "../src/ConversationRules.h"
#include <string>
#include <stdio.h>
int main() {
    const size_t inputLength = ((44 + conversation::InputSamples * 2 + 2) / 3) * 4;
    const size_t outputLength = ((conversation::MaxOutputBytes + 2) / 3) * 4;
    std::string input(inputLength, 'A');
    JsonDocument request;
    request["contents"][0]["parts"][0]["inlineData"]["data"] = input.c_str();
    bool sendOk = !request.overflowed() && request["contents"][0]["parts"][0]["inlineData"]["data"].as<JsonString>().size() == inputLength;
    std::string serialized;
    serializeJson(request, serialized);
    sendOk = sendOk && serialized.find(input) != std::string::npos;
    const std::string output(outputLength, 'B');
    const std::string response = "{\"candidates\":[{\"content\":{\"parts\":[{\"inlineData\":{\"data\":\"" + output + "\"}}]}}]}";
    JsonDocument received;
    const auto error = deserializeJson(received, static_cast<const char*>(response.c_str()), response.size());
    bool receiveOk = !error && !received.overflowed() && received["candidates"][0]["content"]["parts"][0]["inlineData"]["data"].as<JsonString>().size() == outputLength;
    printf("%s: outgoing audio string %zu bytes without JSON overflow (length_bytes=%u)\n", sendOk ? "PASS" : "FAIL", inputLength, unsigned(ARDUINOJSON_STRING_LENGTH_SIZE));
    printf("%s: incoming audio string %zu bytes without truncation (length_bytes=%u)\n", receiveOk ? "PASS" : "FAIL", outputLength, unsigned(ARDUINOJSON_STRING_LENGTH_SIZE));
    return sendOk && receiveOk ? 0 : 1;
}
