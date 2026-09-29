#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 1,
    ESP_ERR_INVALID_STATE = 2, ESP_ERR_NO_MEM = 3, ESP_ERR_TIMEOUT = 4,
    ESP_ERR_INVALID_RESPONSE = 5, ESP_ERR_NOT_FOUND = 6,
    ESP_ERR_WIFI_NOT_STARTED = 7, ESP_ERR_WIFI_NOT_CONNECT = 8,
    ESP_ERR_WIFI_INIT_STATE = 9, ESP_ERR_WIFI_SSID = 10, ESP_ERR_NVS_NOT_FOUND = 11;
inline const char *esp_err_to_name(int) { return "test error"; }
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)

using TickType_t = uint32_t;
constexpr TickType_t portMAX_DELAY = UINT32_MAX;
constexpr int pdTRUE = 1, pdPASS = 1;
#define pdMS_TO_TICKS(ms) (ms)
using SemaphoreHandle_t = std::timed_mutex *;
inline SemaphoreHandle_t xSemaphoreCreateMutex() { return new std::timed_mutex; }
inline int xSemaphoreTake(SemaphoreHandle_t sem, TickType_t delay) {
    if (delay == portMAX_DELAY) { sem->lock(); return pdTRUE; }
    return sem->try_lock_for(std::chrono::milliseconds(delay));
}
inline void xSemaphoreGive(SemaphoreHandle_t sem) { sem->unlock(); }
using portMUX_TYPE = std::mutex;
#define portMUX_INITIALIZER_UNLOCKED {}
#define portENTER_CRITICAL(mux) (mux)->lock()
#define portEXIT_CRITICAL(mux) (mux)->unlock()

struct TestTask {
    std::thread thread;
    std::mutex mutex;
    std::condition_variable notified;
    unsigned notifications = 0;
};
using TaskHandle_t = TestTask *;
inline thread_local TaskHandle_t current_task = nullptr;
inline std::vector<TaskHandle_t> test_tasks;
inline int xTaskCreate(void (*entry)(void *), const char *, int, void *arg,
                       int, TaskHandle_t *out) {
    auto *task = new TestTask;
    *out = task;
    test_tasks.push_back(task);
    task->thread = std::thread([=] { current_task = task; entry(arg); });
    return pdPASS;
}
inline void xTaskNotifyGive(TaskHandle_t task) {
    std::lock_guard<std::mutex> lock(task->mutex);
    ++task->notifications;
    task->notified.notify_one();
}
inline unsigned ulTaskNotifyTake(int, TickType_t delay) {
    std::unique_lock<std::mutex> lock(current_task->mutex);
    auto ready = [] { return current_task->notifications != 0; };
    if (delay == portMAX_DELAY) current_task->notified.wait(lock, ready);
    else current_task->notified.wait_for(lock, std::chrono::milliseconds(delay), ready);
    unsigned count = current_task->notifications;
    current_task->notifications = 0;
    return count;
}
inline void vTaskDelay(TickType_t delay) { std::this_thread::sleep_for(std::chrono::milliseconds(delay)); }
inline void vTaskDelete(void *) {}

namespace system_status_test {
inline std::atomic<bool> radio_started = false, fail_stop = false, fail_start = false;
inline std::atomic<int> starts = 0, stops = 0, connects = 0, nvs_writes = 0,
    battery_reads = 0, ui_updates = 0;
inline bool saved_wifi_enabled = true;
inline std::timed_mutex gui_mutex;
inline std::mutex read_mutex;
inline std::condition_variable read_condition;
inline bool block_battery_read = false, battery_read_waiting = false;
}

namespace esp_brookesia::systems::phone {
class StatusBar {
public:
    enum class WifiState { DISCONNECTED, SIGNAL_1, SIGNAL_2, SIGNAL_3 };
    void hideBatteryIcon() {}
    void hideBatteryPercent() {}
    void setBatteryPercent(bool, int) {}
    void showBatteryIcon() {}
    void showBatteryPercent() {}
    void setWifiIconState(WifiState) { ++system_status_test::ui_updates; }
};
}
namespace esp_brookesia::gui {
class LvLock {
public:
    static LvLock &getInstance() { static LvLock lock; return lock; }
    bool lock(int ms) { return system_status_test::gui_mutex.try_lock_for(std::chrono::milliseconds(ms)); }
    void unlock() { system_status_test::gui_mutex.unlock(); }
};
}

using esp_event_base_t = int;
using esp_event_handler_instance_t = void *;
constexpr int WIFI_EVENT = 1, IP_EVENT = 2, ESP_EVENT_ANY_ID = -1;
constexpr int WIFI_EVENT_STA_START = 1, WIFI_EVENT_STA_CONNECTED = 2,
    WIFI_EVENT_STA_DISCONNECTED = 3, WIFI_EVENT_STA_STOP = 4,
    IP_EVENT_STA_GOT_IP = 5, IP_EVENT_STA_LOST_IP = 6;
using EventHandler = void (*)(void *, int, int32_t, void *);
inline EventHandler wifi_handler = nullptr;
inline void *wifi_handler_arg = nullptr;
inline int esp_event_handler_instance_register(int base, int, EventHandler fn,
                                               void *arg, void **out) {
    if (base == WIFI_EVENT) { wifi_handler = fn; wifi_handler_arg = arg; }
    *out = reinterpret_cast<void *>(1);
    return ESP_OK;
}
inline int esp_event_handler_instance_unregister(int, int, void *) { return ESP_OK; }
inline void emit_wifi(int event) { wifi_handler(wifi_handler_arg, WIFI_EVENT, event, nullptr); }
inline int esp_event_loop_create_default() { return ESP_OK; }
inline int esp_netif_init() { return ESP_OK; }
inline void *esp_netif_get_handle_from_ifkey(const char *) { return reinterpret_cast<void *>(1); }
inline void *esp_netif_create_default_wifi_sta() { return reinterpret_cast<void *>(1); }
struct wifi_config_t {};
struct wifi_init_config_t {};
struct wifi_ap_record_t { int rssi = -50; };
#define WIFI_INIT_CONFIG_DEFAULT() wifi_init_config_t{}
constexpr int WIFI_IF_STA = 0, WIFI_MODE_STA = 1;
inline int esp_wifi_init(wifi_init_config_t *) { return ESP_OK; }
inline int esp_wifi_set_mode(int) { return ESP_OK; }
inline int esp_wifi_set_config(int, wifi_config_t *) { return ESP_OK; }
inline int esp_wifi_scan_stop() { return ESP_OK; }
inline int esp_wifi_disconnect() { emit_wifi(WIFI_EVENT_STA_DISCONNECTED); return ESP_OK; }
inline int esp_wifi_connect() { ++system_status_test::connects; return ESP_OK; }
inline int esp_wifi_start() {
    using namespace system_status_test;
    ++starts;
    if (fail_start.exchange(false)) return ESP_FAIL;
    radio_started = true;
    emit_wifi(WIFI_EVENT_STA_START);
    return ESP_OK;
}
inline int esp_wifi_stop() {
    using namespace system_status_test;
    ++stops;
    if (fail_stop.exchange(false)) return ESP_FAIL;
    radio_started = false;
    emit_wifi(WIFI_EVENT_STA_STOP);
    return ESP_OK;
}
inline int esp_wifi_sta_get_ap_info(wifi_ap_record_t *) {
    return system_status_test::radio_started ? ESP_OK : ESP_FAIL;
}

using nvs_handle_t = int;
constexpr int NVS_READONLY = 0, NVS_READWRITE = 1;
inline int nvs_open(const char *, int, int *out) { *out = 1; return ESP_OK; }
inline int nvs_get_i32(int, const char *, int32_t *value) {
    *value = system_status_test::saved_wifi_enabled;
    return ESP_OK;
}
inline int nvs_set_i32(int, const char *, int32_t value) {
    ++system_status_test::nvs_writes;
    system_status_test::saved_wifi_enabled = value;
    return ESP_OK;
}
inline int nvs_commit(int) { return ESP_OK; }
inline void nvs_close(int) {}

using i2c_master_bus_handle_t = void *;
using i2c_master_dev_handle_t = void *;
constexpr int I2C_ADDR_BIT_LEN_7 = 0;
struct i2c_device_config_t {
    int dev_addr_length;
    int device_address;
    int scl_speed_hz;
    int scl_wait_us;
    struct {} flags;
};
inline uint8_t pmu_registers[256] = {0};
inline void *bsp_i2c_get_handle() { return reinterpret_cast<void *>(1); }
inline int i2c_master_bus_add_device(void *, const i2c_device_config_t *, void **out) {
    *out = reinterpret_cast<void *>(1);
    pmu_registers[3] = 0x4a;
    return ESP_OK;
}
inline int i2c_master_bus_rm_device(void *) { return ESP_OK; }
inline int i2c_master_transmit_receive(void *, const uint8_t *reg, size_t,
                                     uint8_t *data, size_t size, int) {
    if (*reg == 0) {
        using namespace system_status_test;
        ++battery_reads;
        std::unique_lock<std::mutex> lock(read_mutex);
        battery_read_waiting = true;
        read_condition.notify_all();
        read_condition.wait(lock, [] { return !block_battery_read; });
        battery_read_waiting = false;
    }
    std::memcpy(data, pmu_registers + *reg, size);
    return ESP_OK;
}
inline int i2c_master_transmit(void *, const uint8_t *data, size_t, int) {
    pmu_registers[data[0]] = data[1];
    return ESP_OK;
}
