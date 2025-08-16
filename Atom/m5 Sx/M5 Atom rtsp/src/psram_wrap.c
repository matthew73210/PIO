#include <stddef.h>
#include <stdbool.h>
#include <esp_heap_caps.h>
#include <esp_log.h>

void* __real_ps_malloc(size_t size);

void* __wrap_ps_malloc(size_t size) {
    void* ptr = __real_ps_malloc(size);
    if (ptr) {
        return ptr;
    }
    static bool logged = false;
    if (!logged) {
        ESP_LOGW("psram", "PSRAM allocation failed, falling back to internal RAM");
        logged = true;
    }
    return heap_caps_malloc(size, MALLOC_CAP_8BIT);
}
