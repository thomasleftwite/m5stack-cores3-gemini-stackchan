#pragma once
#ifdef ARDUINO
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif
// Temporary D1 instrumentation. No audio, transcript, configuration or private identifiers.
namespace runtimetrace {
enum Event : unsigned { Boot = 1, ModeChanged = 2, Tick = 3, EnrollmentResult = 4,
    StoreBegin = 10, StoreWrite = 11, StoreWritten = 12, StoreAllocated = 13,
    StoreChecked = 14, StoreRenamed = 15, StorePublished = 16 };
inline void mark(Event event, unsigned state = 0) {
#ifdef ARDUINO
    Serial.printf("[DEBUG-r13d1] event=%u state=%u ms=%u heap=%u stack=%u\n",
        unsigned(event), state, unsigned(millis()), unsigned(ESP.getFreeHeap()),
        unsigned(uxTaskGetStackHighWaterMark(nullptr)));
#else
    (void)event; (void)state;
#endif
}
}
