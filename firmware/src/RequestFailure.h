#pragma once
#include <string.h>
// Whitelist fixed failure categories. Never log the error string itself.
namespace requestfailure {
enum Code : unsigned { None = 0, Clock = 1, WiFi = 2, RequestMemory = 3, TLS = 4,
    MissingInput = 5, Worker = 6, RecordingMemory = 7, RetryWait = 8,
    UnclearSpeech = 9, Answer = 10, AudioFormat = 11, ResponseType = 12,
    ResponseSize = 13, IncompleteResponse = 14, InvalidResponse = 15,
    API = 16, Overloaded = 17, Quota = 18, Access = 19, Model = 20, Unknown = 99 };
inline Code classify(const char* error) {
    if (!error || !*error) return None;
    static const struct { const char* text; Code code; } entries[] = {
        {"Clock not synchronized", Clock}, {"WiFi disconnected", WiFi},
        {"Request memory unavailable", RequestMemory}, {"TLS setup failed", TLS},
        {"Audio or API key missing", MissingInput}, {"Worker unavailable", Worker},
        {"Recording memory unavailable", RecordingMemory}, {"Please wait before retry", RetryWait},
        {"Speech unclear; please ask again", UnclearSpeech}, {"Answer unavailable", Answer},
        {"Incomplete or unsupported audio file", AudioFormat}, {"Unexpected response type", ResponseType},
        {"Response too large", ResponseSize}, {"Incomplete API response", IncompleteResponse},
        {"Invalid API response", InvalidResponse}, {"API request failed", API},
        {"Gemini temporarily overloaded; retry later", Overloaded}, {"Free quota reached; retry later", Quota},
        {"API access denied", Access}, {"Model unavailable", Model}
    };
    for (const auto& entry : entries) if (!strcmp(error, entry.text)) return entry.code;
    return Unknown;
}
}
