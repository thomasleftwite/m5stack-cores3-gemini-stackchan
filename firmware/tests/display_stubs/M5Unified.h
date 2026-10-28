#pragma once
#include <string>
#include <stdint.h>
#include <math.h>
#include <algorithm>
struct String : std::string {
    using std::string::string;
    String(const std::string& s) : std::string(s) {}
    String(unsigned n) : std::string(std::to_string(n)) {}
    bool isEmpty() const { return empty(); }
};
constexpr int TFT_WHITE = 65535;
constexpr float DEG_TO_RAD = 0.01745329252f;
namespace fonts { inline int efontJA_12, efontJA_16; }
inline uint32_t testClock = 100;
inline uint32_t millis() { return testClock; }
inline long random(long a, long) { return a; }
template<class T> T constrain(T x, T a, T b) { return std::max(a, std::min(x, b)); }
class M5GFX {
 public:
    bool settings = false;
    std::string text;
    std::string primitives;
    int brightness = 100;
    unsigned clears = 0, transfers = 0, lostSettings = 0;
    template<class... T> void record(const char* name, T... values) {
        primitives += name;
        ((primitives += "," + std::to_string(int(values))), ...);
        primitives += ";";
    }
    void fillScreen(int color) { settings = false; text.clear(); primitives.clear(); record("bg", color); ++clears; }
    void setBrightness(int level) { brightness = level; }
    void fillRoundRect(int x, int y, int w, int h, int r, int color) {
        if (x == 220 && y == 5 && w == 95 && h == 32) settings = true;
        record("rr", x, y, w, h, r, color);
    }
    template<class... T> void setFont(T...) {}
    template<class... T> void setTextColor(T...) {}
    template<class... T> void setCursor(T...) {}
    template<class... T> void setTextSize(T...) {}
    void print(const String& value) { text += value; }
    void print(const char* value) { text += value; }
    void println(const String& value) { text += value; text += '\n'; }
    void println(const char* value) { text += value; text += '\n'; }
    template<class... T> void drawArc(T... v) { record("arc", v...); }
    template<class... T> void fillCircle(T... v) { record("circle", v...); }
    template<class... T> void drawCircle(T... v) { record("circleOutline", v...); }
    template<class... T> void drawRoundRect(T... v) { record("rrOutline", v...); }
};
inline struct { M5GFX Display; } M5;
class M5Canvas : public M5GFX {
    M5GFX* target_;
 public:
    explicit M5Canvas(M5GFX* target) : target_(target) {}
    void setColorDepth(int) {}
    void createSprite(int, int) {}
    void pushSprite(int, int) {
        if (target_->settings && !settings) ++target_->lostSettings;
        target_->settings = settings; ++target_->transfers;
        target_->text = text;
        target_->primitives = primitives;
    }
};
