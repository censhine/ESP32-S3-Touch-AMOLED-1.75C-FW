#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_CRC 0x109
static inline const char *esp_err_to_name(esp_err_t err) { (void)err; return "mock error"; }
static inline void test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGI(...) test_log(__VA_ARGS__)

typedef pthread_mutex_t StaticSemaphore_t;
typedef pthread_mutex_t *SemaphoreHandle_t;
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portMAX_DELAY 0
#define portENTER_CRITICAL(lock) pthread_mutex_lock(lock)
#define portEXIT_CRITICAL(lock) pthread_mutex_unlock(lock)
static inline SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t *mutex) {
    return pthread_mutex_init(mutex, NULL) == 0 ? mutex : NULL;
}
static inline void xSemaphoreTake(SemaphoreHandle_t mutex, int timeout) {
    (void)timeout; pthread_mutex_lock(mutex);
}
static inline void xSemaphoreGive(SemaphoreHandle_t mutex) { pthread_mutex_unlock(mutex); }
static inline int64_t esp_timer_get_time(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)now.tv_sec * 1000000 + now.tv_nsec / 1000;
}
static inline uint32_t esp_crc32_le(uint32_t crc, const uint8_t *data, uint32_t length) {
    crc = ~crc;
    for (uint32_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
    }
    return ~crc;
}
static inline size_t test_strlcpy(char *dest, const char *src, size_t size) {
    size_t len = strlen(src);
    if (size) { size_t n = len < size - 1 ? len : size - 1; memcpy(dest, src, n); dest[n] = 0; }
    return len;
}
#define strlcpy test_strlcpy

typedef int wl_handle_t;
#define WL_INVALID_HANDLE -1
typedef struct {
    bool format_if_mount_failed;
    int max_files;
    size_t allocation_unit_size;
    bool disk_status_check_enable;
    bool use_one_fat;
} esp_vfs_fat_mount_config_t;
esp_err_t esp_vfs_fat_spiflash_mount_rw_wl(const char *, const char *, const esp_vfs_fat_mount_config_t *, wl_handle_t *);
esp_err_t esp_vfs_fat_spiflash_unmount_rw_wl(const char *, wl_handle_t);
esp_err_t esp_vfs_fat_info(const char *, uint64_t *, uint64_t *);
esp_err_t wl_unmount(wl_handle_t);
