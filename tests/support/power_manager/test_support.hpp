#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

using esp_err_t = int;
constexpr int ESP_OK = 0, ESP_FAIL = -1, ESP_ERR_INVALID_ARG = 1;
constexpr int ESP_ERR_INVALID_STATE = 2, ESP_ERR_TIMEOUT = 3, ESP_ERR_NO_MEM = 4;
using esp_lcd_panel_handle_t = void *;
using TaskHandle_t = void *;
using portMUX_TYPE = int;
constexpr int portMUX_INITIALIZER_UNLOCKED = 0, pdPASS = 1;
#define IRAM_ATTR
#define pdMS_TO_TICKS(ms) (ms)
struct lv_display_t {};
struct lv_indev_t {};
struct lv_event_t { void *param; };
struct lv_timer_t { void (*callback)(lv_timer_t *); uint32_t period; };
inline lv_timer_t *lv_timer_create(void (*cb)(lv_timer_t *), uint32_t period, void *) {
    static lv_timer_t timer; timer = {cb, period}; return &timer;
}
inline void lv_timer_delete(lv_timer_t *) {}
struct esp_pm_config_t { int max_freq_mhz; int min_freq_mhz; bool light_sleep_enable; };
constexpr int BSP_BUTTON_PWR_GPIO = 3, BSP_BUTTON_BOOT_GPIO = 0;
constexpr int GPIO_INTR_HIGH_LEVEL = 1, GPIO_INTR_DISABLE = 0, ESP_INTR_FLAG_IRAM = 1;
constexpr int ESP_SLEEP_WAKEUP_GPIO = 1, BSP_EXTRA_AUDIO_OWNER_NONE = 0;
constexpr int MALLOC_CAP_INTERNAL = 1, MALLOC_CAP_8BIT = 2, MALLOC_CAP_SPIRAM = 4;
enum { STORAGE_SERVICE_STATE_UNMOUNTED, STORAGE_SERVICE_STATE_MOUNTED,
       STORAGE_SERVICE_STATE_MOUNTING, STORAGE_SERVICE_STATE_EJECTING };
struct storage_service_info_t { unsigned active_leases; int state; };
namespace power_manager_test {
struct Fake {
    std::vector<std::string> calls;
    std::string fail;
    int locks = 0, critical = 0, power = 0, boot = 1, owner = 0;
    unsigned running = 0, leases = 0;
    int mount = STORAGE_SERVICE_STATE_MOUNTED;
    bool status_down = false, paused = false, dark = true, recovery = false;
    bool gpio_wake = false, sleep_wake = false, lock_error = false;
    bool app_during_pause = false, power_during_panel = false;
    uint32_t now = 600000, idle = 600000, dark_ms = 10000;
    int64_t time_us = 0;
    void (*isr)(void *) = nullptr;
} inline f;
inline int step(const char *name, int expected_locks = -1) {
    f.calls.emplace_back(name);
    if (expected_locks >= 0 && f.locks != expected_locks) f.lock_error = true;
    return f.fail == name ? ESP_FAIL : ESP_OK;
}
inline void log(const char *, const char *, ...) {}
}
#define ESP_LOGI(...) power_manager_test::log(__VA_ARGS__)
#define ESP_LOGW(...) power_manager_test::log(__VA_ARGS__)
#define ESP_LOGE(...) power_manager_test::log(__VA_ARGS__)
inline void portENTER_CRITICAL(portMUX_TYPE *) { ++power_manager_test::f.critical; }
inline void portEXIT_CRITICAL(portMUX_TYPE *) { --power_manager_test::f.critical; }
#define portENTER_CRITICAL_ISR portENTER_CRITICAL
#define portEXIT_CRITICAL_ISR portEXIT_CRITICAL
inline const char *esp_err_to_name(int) { return "fake"; }
inline uint32_t lv_tick_get() { return power_manager_test::f.now; }
inline uint32_t lv_display_get_inactive_time(lv_display_t *) { return power_manager_test::f.idle; }
inline void *lv_event_get_param(lv_event_t *event) { return event->param; }
inline size_t heap_caps_get_free_size(int) { return 1024; }
namespace esp_brookesia::systems::base {
struct App { int getId() { return 1; } const char *getName() { return "Fake app"; } };
struct Context {
    enum class AppEventType { START, STOP };
    struct AppEventData { int id; AppEventType type; void *data; };
};
}
namespace esp_brookesia::systems::phone {
struct Manager {
    base::App app;
    unsigned getRunningAppCount() { return power_manager_test::f.running; }
    base::App *getRunningAppByIdenx(unsigned) { return getRunningAppById(1); }
    base::App *getRunningAppById(int) { return getRunningAppCount() ? &app : nullptr; }
    base::App *getActiveApp() { return getRunningAppById(1); }
};
struct Recents { bool checkVisible() { return false; } };
struct Display { Recents *getRecentsScreen() { return nullptr; } };
class Phone {
public:
    Manager manager; Display display;
    Manager &getManager() { return manager; } Display &getDisplay() { return display; }
    bool sendAppEvent(const base::Context::AppEventData *) {
        if (power_manager_test::step("app_stop")) return false;
        power_manager_test::f.running = 0; return true;
    }
    bool registerAppEventCallback(void (*)(lv_event_t *), void *) { return true; }
    void unregisterAppEventCallback(void (*)(lv_event_t *), void *) {}
};
}
namespace esp_brookesia::gui {
struct LvLock {
    static LvLock &getInstance() { static LvLock lock; return lock; }
    bool lock(int) { if (power_manager_test::step("lock", 0)) return false; ++power_manager_test::f.locks; return true; }
    void unlock() { power_manager_test::step("unlock", 1); --power_manager_test::f.locks; }
};
}
inline int gpio_get_level(int pin) { return pin == BSP_BUTTON_PWR_GPIO ? power_manager_test::f.power : power_manager_test::f.boot; }
inline int gpio_wakeup_enable(int, int) { power_manager_test::f.gpio_wake = true; return power_manager_test::step("gpio_arm"); }
inline int gpio_wakeup_disable(int) { power_manager_test::f.gpio_wake = false; return power_manager_test::step("gpio_disarm"); }
inline int gpio_isr_handler_add(int, void (*isr)(void *), void *) { power_manager_test::f.isr = isr; return power_manager_test::step("isr_add"); }
inline int gpio_isr_handler_remove(int) { power_manager_test::f.isr = nullptr; return power_manager_test::step("isr_remove"); }
inline int gpio_intr_disable(int) { return power_manager_test::step("isr_disable"); }
inline int gpio_set_intr_type(int, int) { return power_manager_test::step("intr_disable"); }
inline int gpio_install_isr_service(int) { return ESP_OK; }
inline int esp_sleep_enable_gpio_wakeup() { power_manager_test::f.sleep_wake = true; return power_manager_test::step("sleep_arm"); }
inline int esp_sleep_disable_wakeup_source(int) { power_manager_test::f.sleep_wake = false; return power_manager_test::step("sleep_disarm"); }
inline int esp_sleep_get_wakeup_cause() { return ESP_SLEEP_WAKEUP_GPIO; }
inline int esp_light_sleep_start() { power_manager_test::f.time_us += 5000000; return power_manager_test::step("sleep", 0); }
inline int64_t esp_timer_get_time() { return power_manager_test::f.time_us; }
inline int esp_pm_configure(const esp_pm_config_t *) { return ESP_OK; }
inline void vTaskDelay(unsigned ms) { power_manager_test::step("delay", 0); power_manager_test::f.time_us += ms * 1000; }
inline int xTaskCreate(void (*)(void *), const char *, unsigned, void *, int, TaskHandle_t *task) { *task = reinterpret_cast<void *>(1); return pdPASS; }
inline int esp_lv_adapter_pause(unsigned) {
    using namespace power_manager_test;
    if (f.app_during_pause) f.running = 1;
    const int result = step("pause", 0); f.paused = result == ESP_OK; return result;
}
inline int esp_lv_adapter_resume() { power_manager_test::f.paused = false; return power_manager_test::step("resume", 0); }
inline int bsp_extra_audio_session_get_owner() { return power_manager_test::f.owner; }
inline int bsp_extra_codec_dev_stop() { return power_manager_test::step("codec_stop", 1); }
inline int bsp_audio_suspend() { return power_manager_test::step("audio_suspend", 1); }
inline int storage_service_get_info(storage_service_info_t *info) { *info = {power_manager_test::f.leases, power_manager_test::f.mount}; return ESP_OK; }
inline int storage_service_safe_eject() { power_manager_test::f.mount = STORAGE_SERVICE_STATE_UNMOUNTED; return power_manager_test::step("eject"); }
namespace brookesia::system_status {
struct Snapshot { bool battery_valid = true; int battery_percent = 75; };
inline bool get_snapshot(Snapshot &) { return true; }
inline int set_standby(bool down) { power_manager_test::f.status_down = down; return power_manager_test::step(down ? "status_down" : "status_up", 0); }
}
namespace brookesia::screen_power {
inline bool is_off() { return power_manager_test::f.dark; }
inline uint32_t off_duration_ms() { return power_manager_test::f.dark_ms; }
inline bool needs_standby_recovery() { return power_manager_test::f.recovery; }
inline int sleep_panel_for_standby() {
    using namespace power_manager_test;
    f.recovery = true;
    if (f.power_during_panel && f.isr) { f.power = 1; f.isr(nullptr); f.power = 0; }
    return step("panel_sleep", 1);
}
inline bool wake_from_standby() {
    const bool ok = power_manager_test::step("panel_wake", 1) == ESP_OK;
    power_manager_test::f.recovery = !ok; return ok;
}
}
