#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp32-hal-psram.h>

extern "C" void* ps_malloc(size_t size) {
    if (!psramFound()) {
        static bool logged = false;
        if (!logged) {
            Serial.println("ps_malloc fallback: PSRAM not found, using internal RAM");
            logged = true;
        }
    }
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}

