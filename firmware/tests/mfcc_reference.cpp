#include "../src/MfccFeatures.h"
#include <stdio.h>
int main() {
    for (int test = 0; test < 6; ++test) {
        int16_t samples[320], result[12]; uint32_t random = 1234;
        for (int i = 0; i < 320; ++i) {
            random = random * 1664525u + 1013904223u;
            float value = test == 0 ? 0 : test == 1 ? 5000 * sinf(6.2831853f * 440 * i / 16000) :
                test == 2 ? 3200 * cosf(6.2831853f * 800 * i / 16000) + 1000 * sinf(6.2831853f * 2200 * i / 16000) :
                test == 3 ? 4000 * sinf(6.2831853f * (300 * i / 16000.f + 2300 * i * i / (2.f * 320 * 16000))) :
                (int(random >> 16) - 32768) / 20.f;
            if (test == 5) value = value * .5f + 4000;
            samples[i] = int16_t(value);
        }
        localwake::detail::mfcc(samples, result);
        printf("{\"samples\":[");
        for (int i = 0; i < 320; ++i) printf("%s%d", i ? "," : "", int(samples[i]));
        printf("],\"mfcc\":[");
        for (int i = 0; i < 12; ++i) printf("%s%d", i ? "," : "", int(result[i]));
        puts("]}");
    }
}
