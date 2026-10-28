#include "StackChanAvatar.h"

StackChanAvatar::StackChanAvatar() : _canvas(&M5.Display) {}

void StackChanAvatar::init(M5GFX* display) {
    _gfx = display;
    _canvas.setColorDepth(16);
    _canvas.createSprite(320, 240);
    _lastBlinkTime = millis();
}

void StackChanAvatar::setEmotion(AvatarEmotion emotion) {
    _targetEmotion = emotion;
    _emotion = emotion;
}

void StackChanAvatar::setMode(AvatarMode mode) {
    if (_mode == mode) return;
    _mode = mode;
    _isBlinking = false; _eyeOpen = 1.0f; _lastBlinkTime = millis();
    _gazeX = _gazeY = _mouthOpen = _targetMouthOpen = 0;
    _eyebrowAngle = _eyebrowY = 0;
}

void StackChanAvatar::setLipSyncLevel(float level) {
    _targetMouthOpen = constrain(level, 0.0f, 1.0f);
}

void StackChanAvatar::setGaze(float x, float y) {
    _gazeX = constrain(x, -1.0f, 1.0f);
    _gazeY = constrain(y, -1.0f, 1.0f);
}

void StackChanAvatar::triggerBlink() {
    _isBlinking = true;
    _blinkStartTime = millis();
}

void StackChanAvatar::update() {
    _menuVisible = false;
    uint32_t now = millis();
    switch (_mode) {
        case AvatarMode::Sleep: _emotion = EMOTION_SLEEP; break;
        case AvatarMode::Ready: _emotion = EMOTION_NORMAL; break;
        case AvatarMode::Listening: _emotion = EMOTION_LISTENING; break;
        case AvatarMode::Thinking: _emotion = EMOTION_THINKING; break;
        case AvatarMode::Speaking: _emotion = EMOTION_TALKING; break;
    }
    if (_mode != AvatarMode::Speaking) _targetMouthOpen = 0;

    // 1. 自動瞬きロジック
    if (_emotion != EMOTION_SLEEP) {
        if (!_isBlinking && (now - _lastBlinkTime > _nextBlinkInterval)) {
            triggerBlink();
            _nextBlinkInterval = random(2500, 5000);
        }
        if (_isBlinking) {
            uint32_t elapsed = now - _blinkStartTime;
            if (elapsed < 120) {
                _eyeOpen = 1.0f - (float)elapsed / 120.0f;
            } else if (elapsed < 240) {
                _eyeOpen = (float)(elapsed - 120) / 120.0f;
            } else {
                _eyeOpen = 1.0f;
                _isBlinking = false;
                _lastBlinkTime = now;
            }
        }
    } else {
        _eyeOpen = 0.0f; // 睡眠時は目を閉じる
    }

    // 2. 口の開閉スムージング
    _mouthOpen += (_targetMouthOpen - _mouthOpen) * 0.35f;

    // 3. 感情に応じた眉毛・表情パラメータの補間
    float targetAngle = 0.0f;
    float targetEyebrowY = 0.0f;

    switch (_emotion) {
        case EMOTION_HAPPY:
            targetAngle = -15.0f;
            targetEyebrowY = -6.0f;
            break;
        case EMOTION_SAD:
            targetAngle = 20.0f;
            targetEyebrowY = 8.0f;
            break;
        case EMOTION_ANGRY:
            targetAngle = -28.0f;
            targetEyebrowY = 10.0f;
            break;
        case EMOTION_SURPRISED:
            targetAngle = 0.0f;
            targetEyebrowY = -14.0f;
            break;
        case EMOTION_THINKING:
            targetAngle = 10.0f;
            targetEyebrowY = -4.0f;
            _gazeX = 0.5f + 0.2f * sinf(now / 700.0f); _gazeY = -0.5f;
            break;
        case EMOTION_LISTENING:
            targetEyebrowY = -10.0f;
            break;
        case EMOTION_TALKING:
            targetAngle = -8.0f;
            targetEyebrowY = -3.0f;
            break;
        default:
            targetAngle = 0.0f;
            targetEyebrowY = 0.0f;
            break;
    }
    _eyebrowAngle += (targetAngle - _eyebrowAngle) * 0.2f;
    _eyebrowY += (targetEyebrowY - _eyebrowY) * 0.2f;

    // 4. キャンバス描画
    _canvas.fillScreen(0x18E3); // サイバーダーク色

    drawEyebrows();
    drawEyes();
    drawMouth();


    if (!_status.isEmpty()) {
        _canvas.setFont(&fonts::efontJA_12);
        _canvas.setTextColor(TFT_WHITE, 0x18E3);
        _canvas.setCursor(5, 216);
        _canvas.print(_status);
    }
    // 5. 画面へ一括転送
    _canvas.pushSprite(0, 0);
}

void StackChanAvatar::drawMenu(const String& title, const String& action, const String& detail) {
    if (_menuVisible && title == _menuTitle && action == _menuAction && detail == _menuDetail) return;
    _menuTitle = title; _menuAction = action; _menuDetail = detail; _menuVisible = true;
    _canvas.fillScreen(0x18E3);
    _canvas.setFont(&fonts::efontJA_16);
    _canvas.setTextColor(TFT_WHITE, 0x18E3);
    _canvas.setTextSize(1);
    _canvas.setCursor(12, 15); _canvas.println(title);
    _canvas.setCursor(12, 45); _canvas.println(detail);
    _canvas.fillRoundRect(10, 85, 300, 60, 8, 0x249f);
    _canvas.setTextColor(TFT_WHITE, 0x249f);
    _canvas.setCursor(20, 105); _canvas.println(action);
    _canvas.setTextColor(TFT_WHITE, 0x18E3);
    _canvas.setCursor(20, 195); _canvas.println("戻る / キャンセル");
    _canvas.pushSprite(0, 0);
}

void StackChanAvatar::drawEyes() {
    int leftCenterX = 100 + (int)(_gazeX * 8);
    int rightCenterX = 220 + (int)(_gazeX * 8);
    int centerY = 110 + (int)(_gazeY * 8);

    uint16_t eyeColor = TFT_WHITE;
    if (_emotion == EMOTION_SLEEP) eyeColor = 0x7BEF;
    else if (_emotion == EMOTION_LISTENING) eyeColor = 0x07FF;
    else if (_emotion == EMOTION_THINKING) eyeColor = 0xFFE0;
    else if (_emotion == EMOTION_TALKING) eyeColor = 0x07E0;

    if (_emotion == EMOTION_HAPPY) {
        _canvas.drawArc(leftCenterX, centerY + 8, 22, 26, 200, 340, eyeColor);
        _canvas.drawArc(rightCenterX, centerY + 8, 22, 26, 200, 340, eyeColor);
        _canvas.fillCircle(leftCenterX - 28, centerY + 24, 9, 0xFBEF);
        _canvas.fillCircle(rightCenterX + 28, centerY + 24, 9, 0xFBEF);
        return;
    }

    if (_emotion == EMOTION_SLEEP || _eyeOpen <= 0.05f) {
        // 睡眠時は優しいアーチ状の閉じた目 (上向きの円弧で穏やかな寝顔)
        _canvas.drawArc(leftCenterX, centerY + 6, 16, 20, 200, 340, eyeColor);
        _canvas.drawArc(rightCenterX, centerY + 6, 16, 20, 200, 340, eyeColor);
        return;
    }

    int eyeRadiusX = 18;
    int eyeRadiusY = (int)(28 * _eyeOpen);
    if (_emotion == EMOTION_LISTENING) { eyeRadiusX = 22; eyeRadiusY = int(34 * _eyeOpen); }
    else if (_emotion == EMOTION_THINKING) eyeRadiusY = int(13 * _eyeOpen);
    else if (_emotion == EMOTION_TALKING) eyeRadiusY = int(23 * _eyeOpen);
    if (_emotion == EMOTION_SURPRISED) {
        eyeRadiusX = 22;
        eyeRadiusY = 32;
    }

    _canvas.fillRoundRect(leftCenterX - eyeRadiusX, centerY - eyeRadiusY, eyeRadiusX * 2, eyeRadiusY * 2, eyeRadiusX, eyeColor);
    _canvas.fillRoundRect(rightCenterX - eyeRadiusX, centerY - eyeRadiusY, eyeRadiusX * 2, eyeRadiusY * 2, eyeRadiusX, eyeColor);

    if (_eyeOpen > 0.5f) {
        _canvas.fillCircle(leftCenterX - 6, centerY - eyeRadiusY / 2, 4, 0x0000);
        _canvas.fillCircle(rightCenterX - 6, centerY - eyeRadiusY / 2, 4, 0x0000);
        _canvas.fillCircle(leftCenterX - 4, centerY - eyeRadiusY / 2 + 1, 2, TFT_WHITE);
        _canvas.fillCircle(rightCenterX - 4, centerY - eyeRadiusY / 2 + 1, 2, TFT_WHITE);
    }
}

void StackChanAvatar::drawEyebrows() {
    if (_emotion == EMOTION_SLEEP) {
        return; // 睡眠時は眉毛を描画しない（不要な横線の除去）
    }

    uint16_t browColor = TFT_WHITE;
    int browLen = 36;
    int browThick = 5;

    int lx = 100;
    int ly = 65 + (int)_eyebrowY;
    int ldy = (int)(tan(_eyebrowAngle * DEG_TO_RAD) * (browLen / 2));
    _canvas.fillRoundRect(lx - browLen / 2, ly - ldy, browLen, browThick, 2, browColor);

    int rx = 220;
    int ry = 65 + (int)_eyebrowY;
    int rdy = (int)(tan(-_eyebrowAngle * DEG_TO_RAD) * (browLen / 2));
    _canvas.fillRoundRect(rx - browLen / 2, ry - rdy, browLen, browThick, 2, browColor);
}

void StackChanAvatar::drawMouth() {
    if (_emotion == EMOTION_SLEEP) {
        return; // 睡眠時は口（不要な横白線）を描画しない
    }

    int cx = 160;
    int cy = 180;
    uint16_t mouthColor = 0xFBEF;

    if (_emotion == EMOTION_LISTENING) {
        _canvas.fillRoundRect(cx - 7, cy, 14, 3, 1, 0x07FF);
        return;
    }
    if (_emotion == EMOTION_THINKING) {
        _canvas.drawCircle(cx + 10, cy, 6, 0xFFE0);
        return;
    }

    if (_mouthOpen > 0.08f) {
        int mw = 22 + (int)(_mouthOpen * 20);
        int mh = 8 + (int)(_mouthOpen * 28);
        _canvas.fillRoundRect(cx - mw / 2, cy - mh / 2, mw, mh, mw / 2, mouthColor);
        _canvas.drawRoundRect(cx - mw / 2, cy - mh / 2, mw, mh, mw / 2, TFT_WHITE);
    } else {
        if (_emotion == EMOTION_HAPPY) {
            _canvas.drawArc(cx, cy - 4, 16, 18, 20, 160, TFT_WHITE);
        } else if (_emotion == EMOTION_SAD || _emotion == EMOTION_ANGRY) {
            _canvas.drawArc(cx, cy + 12, 16, 18, 200, 340, TFT_WHITE);
        } else if (_emotion == EMOTION_SURPRISED) {
            _canvas.fillCircle(cx, cy, 9, mouthColor);
            _canvas.drawCircle(cx, cy, 9, TFT_WHITE);
        } else {
            _canvas.fillRoundRect(cx - 10, cy, 20, 4, 2, TFT_WHITE);
        }
    }
}
