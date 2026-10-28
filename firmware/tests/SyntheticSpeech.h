#pragma once
#include <stdint.h>
#include <math.h>
// Source/filter vowel proxy, generated mathematically; never a recording.
inline void syntheticSpeechFrame(int16_t* out, unsigned frame, float amplitude, bool alternate = false) {
    const float f1[] = {600,300,800}, f2[] = {1400,2200,1100};
    const float other1[] = {250,900,400}, other2[] = {3400,3400,3200};
    unsigned vowel = frame < 10 ? 0 : frame < 20 ? 1 : 2;
    for (unsigned i = 0; i < 320; ++i) {
        float value = 0;
        for (unsigned h = 1; h <= 32; ++h) {
            float hz = 120.f*h, a = (hz-(alternate ? other1[vowel] : f1[vowel]))/120, b = (hz-(alternate ? other2[vowel] : f2[vowel]))/180;
            value += (expf(-.5f*a*a)+.6f*expf(-.5f*b*b))/sqrtf(float(h))*sinf(6.2831853f*hz*(frame*320+i)/16000);
        }
        out[i] = int16_t(value * amplitude);
    }
}
