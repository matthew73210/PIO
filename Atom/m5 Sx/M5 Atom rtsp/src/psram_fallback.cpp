#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

extern "C" void* __real_ps_malloc(size_t size);

extern "C" void* __wrap_ps_malloc(size_t size) {
    if (psramFound()) {
        return __real_ps_malloc(size);
    }
    static bool logged = false;
    if (!logged) {
        log_e("PSRAM not found, falling back to heap");
        logged = true;
    }
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}
