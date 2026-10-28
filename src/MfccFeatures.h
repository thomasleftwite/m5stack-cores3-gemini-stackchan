#pragma once
#include <math.h>
#include <stdint.h>
#include <algorithm>
// UI-loop-only 20 ms / 16 kHz MFCC: 512 FFT, 40 triangular Mel bands,
// coefficients 1..12 (energy coefficient excluded). No audio is retained.
namespace localwake { namespace detail {
struct MfccTables {
    float window[320], edges[42], dct[12][40], inverseEmphasis[257];
    MfccTables() {
        for (int i = 0; i < 320; ++i) window[i] = .5f - .5f * cosf(6.283185307f * i / 319);
        float low = 2595 * log10f(1 + 80.f / 700), high = 2595 * log10f(1 + 7600.f / 700);
        for (int i = 0; i < 42; ++i) edges[i] = 700 * (powf(10, (low + (high - low) * i / 41) / 2595) - 1);
        for (int k = 0; k < 12; ++k) for (int m = 0; m < 40; ++m)
            dct[k][m] = sqrtf(2.f / 40) * cosf(3.141592654f * (k + 1) * (m + .5f) / 40);
        for (int b = 0; b <= 256; ++b) inverseEmphasis[b] = 1 / (1 + .97f*.97f - 1.94f*cosf(6.283185307f*b/512));
    }
};
inline bool mfcc(const int16_t* samples, int16_t* out) {
    static const MfccTables tables;
    // Fixed workspace avoids large arrays on the ESP32 loop task's stack.
    static float real[512], imag[512], power[257], mel[40], periodicSignal[160];
    float dc = 0;
    for (int i = 0; i < 320; ++i) dc += samples[i];
    dc /= 320;
    float previous = (samples[0] - dc) / 32768;
    float even = 0;
    for (int i = 0; i < 512; ++i) {
        float current = i < 320 ? (samples[i] - dc) / 32768 : 0;
        float emphasized = current - .97f * previous;
        real[i] = i < 320 ? emphasized * tables.window[i] : 0;
        if (i < 320) {
            if (!(i & 1)) even = emphasized;
            else periodicSignal[i / 2] = (even + emphasized) * .5f;
        }
        imag[i] = 0; previous = current;
    }
    for (unsigned i = 1, j = 0; i < 512; ++i) {
        unsigned bit = 256;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { std::swap(real[i], real[j]); std::swap(imag[i], imag[j]); }
    }
    for (unsigned length = 2; length <= 512; length <<= 1) {
        float angle = -6.283185307f / length, stepR = cosf(angle), stepI = sinf(angle);
        for (unsigned start = 0; start < 512; start += length) {
            float wr = 1, wi = 0;
            for (unsigned j = 0; j < length / 2; ++j) {
                unsigned a = start + j, b = a + length / 2;
                float tr = wr * real[b] - wi * imag[b], ti = wr * imag[b] + wi * real[b];
                real[b] = real[a] - tr; imag[b] = imag[a] - ti;
                real[a] += tr; imag[a] += ti;
                float nextR = wr * stepR - wi * stepI;
                wi = wr * stepI + wi * stepR; wr = nextR;
            }
        }
    }
    for (int b = 0; b <= 256; ++b) power[b] = real[b]*real[b] + imag[b]*imag[b];
    // Evidence of voiced speech, independent of the word matcher. Correct the
    // preemphasis before measuring spectral flatness, otherwise white noise
    // acquires a misleading rising spectrum.
    float total = 0, logTotal = 0, low = 0, high = 0;
    int bins = 0;
    for (int b = 5; b <= 128; ++b) {
        float p = fmaxf(1e-12f, power[b] * tables.inverseEmphasis[b]);
        total += p; logTotal += logf(p); ++bins;
        if (b < 32) low += p; else high += p;
    }
    float flatness = expf(logTotal / bins) / fmaxf(1e-12f, total / bins);
    bool periodic = false;
    if (flatness < .45f && low > total*.01f && high > total*.01f) {
        for (int lag = 16; lag <= 100 && !periodic; ++lag) {
            float product = 0, a = 0, b = 0;
            for (int i = lag; i < 160; ++i) {
                float x = periodicSignal[i], y = periodicSignal[i-lag];
                product += x*y; a += x*x; b += y*y;
            }
            periodic = product > 0 && product*product >= .65f*.65f*a*b && a*b > 1e-15f;
        }
    }
    for (int m = 0; m < 40; ++m) {
        float energy = 0, left = tables.edges[m], center = tables.edges[m+1], right = tables.edges[m+2];
        int first = std::max(0, int(ceilf(left / 31.25f))), last = std::min(256, int(floorf(right / 31.25f)));
        for (int b = first; b <= last; ++b) {
            float hz = b * 31.25f;
            float weight = hz <= center ? (hz - left) / (center - left) : (right - hz) / (right - center);
            energy += fmaxf(0, weight) * power[b];
        }
        mel[m] = logf(fmaxf(1e-12f, energy));
    }
    for (int k = 0; k < 12; ++k) {
        float value = 0;
        for (int m = 0; m < 40; ++m) value += tables.dct[k][m] * mel[m];
        out[k] = int16_t(fmaxf(-32768, fminf(32767, roundf(value * 8))));
    }
    return periodic;
}
}}
