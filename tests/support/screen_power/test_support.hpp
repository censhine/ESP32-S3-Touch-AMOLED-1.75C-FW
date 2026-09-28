#pragma once

#include <cstddef>
#include <cstdint>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
constexpr esp_err_t ESP_FAIL = -1;
constexpr esp_err_t ESP_ERR_INVALID_ARG = 0x102;
constexpr esp_err_t ESP_ERR_INVALID_STATE = 0x103;
constexpr esp_err_t ESP_ERR_NO_MEM = 0x101;
inline const char *esp_err_to_name(esp_err_t) { return "test error"; }

struct TestPanel {};
using esp_lcd_panel_handle_t = TestPanel *;
struct lv_timer_t;
using lv_timer_cb_t = void (*)(lv_timer_t *);
struct lv_timer_t {
    lv_timer_cb_t callback = nullptr;
    void *user_data = nullptr;
    uint32_t period = 0;
    bool paused = false;
};
struct lv_obj_t { int invalidations = 0; };
struct lv_display_t {
    uint32_t activity = 0;
    bool invalidation_enabled = true;
    int invalidation_count = 1;
    lv_timer_t refresh_timer;
    lv_obj_t system_layer;
};
struct lv_indev_t {
    lv_display_t *display = nullptr;
    bool enabled = true;
    bool wait_release = false;
};
using gpio_num_t = int;
constexpr gpio_num_t GPIO_NUM_0 = 0;
constexpr gpio_num_t GPIO_NUM_3 = 3;
constexpr int GPIO_MODE_INPUT = 1;
constexpr int GPIO_PULLUP_DISABLE = 0;
constexpr int GPIO_PULLUP_ENABLE = 1;
constexpr int GPIO_PULLDOWN_DISABLE = 0;
constexpr int GPIO_INTR_DISABLE = 0;
struct gpio_config_t {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
};
constexpr unsigned MALLOC_CAP_8BIT = 1;
constexpr unsigned MALLOC_CAP_INTERNAL = 2;
constexpr unsigned MALLOC_CAP_SPIRAM = 4;

namespace screen_power_test {
enum Event { PanelOff, PanelOn, TouchReset, TouchWait, TouchDisable, TouchEnable,
             InvalidationOff, InvalidationOn, RefreshPause, RefreshResume,
             RefreshReady, FullInvalidate };
struct State {
    uint32_t now = 0;
    int gpio[4] = {1, 0, 0, 0};
    int gpio_configs = 0;
    gpio_config_t configs[4]{};
    esp_err_t gpio_result = ESP_OK;
    esp_err_t panel_result = ESP_OK;
    bool panel_on = true;
    int panel_commands = 0;
    int timer_creates = 0;
    int touch_resets = 0;
    int touch_waits = 0;
    int activity_triggers = 0;
    int refresh_pauses = 0;
    int refresh_resumes = 0;
    int refresh_readies = 0;
    int heap_free_queries = 0;
    int heap_largest_queries = 0;
    bool fail_timer_create = false;
    bool wrong_refresh_timer = false;
    int logs = 0;
    Event events[32]{};
    int event_count = 0;
    lv_timer_t poll_timer;
    lv_display_t display;
    lv_indev_t touch{&display, true, false};
    TestPanel panel;
};
inline State state;
inline void record(Event event) {
    if (state.event_count < 32) state.events[state.event_count++] = event;
}
inline void log(const char *, const char *, ...) { ++state.logs; }
}

inline uint32_t lv_tick_get() { return screen_power_test::state.now; }
inline uint32_t lv_tick_elaps(uint32_t start) { return lv_tick_get() - start; }
inline lv_timer_t *lv_timer_create(lv_timer_cb_t callback, uint32_t period, void *data) {
    auto &s = screen_power_test::state;
    ++s.timer_creates;
    if (s.fail_timer_create) return nullptr;
    s.poll_timer = {callback, data, period, false};
    return &s.poll_timer;
}
inline void *lv_timer_get_user_data(lv_timer_t *timer) { return timer->user_data; }
inline lv_display_t *lv_indev_get_display(lv_indev_t *touch) { return touch->display; }
inline uint32_t lv_display_get_inactive_time(lv_display_t *display) { return lv_tick_get() - display->activity; }
inline void lv_display_trigger_activity(lv_display_t *display) {
    ++screen_power_test::state.activity_triggers;
    display->activity = lv_tick_get();
}
inline lv_timer_t *lv_display_get_refr_timer(lv_display_t *display) { return &display->refresh_timer; }
inline void lv_display_enable_invalidation(lv_display_t *display, bool enabled) {
    display->invalidation_count += enabled ? 1 : -1;
    display->invalidation_enabled = display->invalidation_count > 0;
    screen_power_test::record(enabled ? screen_power_test::InvalidationOn : screen_power_test::InvalidationOff);
}
inline lv_obj_t *lv_display_get_layer_sys(lv_display_t *display) { return &display->system_layer; }
inline void lv_obj_invalidate(lv_obj_t *obj) {
    ++obj->invalidations;
    screen_power_test::record(screen_power_test::FullInvalidate);
}
inline void lv_timer_pause(lv_timer_t *timer) {
    auto &s = screen_power_test::state;
    s.wrong_refresh_timer |= timer != &s.display.refresh_timer;
    timer->paused = true;
    ++s.refresh_pauses;
    screen_power_test::record(screen_power_test::RefreshPause);
}
inline void lv_timer_resume(lv_timer_t *timer) {
    auto &s = screen_power_test::state;
    s.wrong_refresh_timer |= timer != &s.display.refresh_timer;
    timer->paused = false;
    ++s.refresh_resumes;
    screen_power_test::record(screen_power_test::RefreshResume);
}
inline void lv_timer_ready(lv_timer_t *timer) {
    auto &s = screen_power_test::state;
    s.wrong_refresh_timer |= timer != &s.display.refresh_timer;
    ++s.refresh_readies;
    screen_power_test::record(screen_power_test::RefreshReady);
}
inline void lv_indev_reset(lv_indev_t *, lv_obj_t *) {
    ++screen_power_test::state.touch_resets;
    screen_power_test::record(screen_power_test::TouchReset);
}
inline void lv_indev_wait_release(lv_indev_t *touch) {
    touch->wait_release = true;
    ++screen_power_test::state.touch_waits;
    screen_power_test::record(screen_power_test::TouchWait);
}
inline void lv_indev_enable(lv_indev_t *touch, bool enabled) {
    touch->enabled = enabled;
    screen_power_test::record(enabled ? screen_power_test::TouchEnable : screen_power_test::TouchDisable);
}
inline int gpio_get_level(gpio_num_t pin) { return screen_power_test::state.gpio[pin]; }
inline esp_err_t gpio_config(const gpio_config_t *config) {
    auto &s = screen_power_test::state;
    if (s.gpio_configs < 4) s.configs[s.gpio_configs] = *config;
    ++s.gpio_configs;
    return s.gpio_result;
}
inline esp_err_t esp_lcd_panel_disp_on_off(esp_lcd_panel_handle_t, bool on) {
    auto &s = screen_power_test::state;
    ++s.panel_commands;
    if (s.panel_result != ESP_OK) return s.panel_result;
    s.panel_on = on;
    screen_power_test::record(on ? screen_power_test::PanelOn : screen_power_test::PanelOff);
    return ESP_OK;
}
inline std::size_t heap_caps_get_free_size(unsigned) { ++screen_power_test::state.heap_free_queries; return 65536; }
inline std::size_t heap_caps_get_largest_free_block(unsigned) { ++screen_power_test::state.heap_largest_queries; return 32768; }

#define ESP_LOGI(...) screen_power_test::log(__VA_ARGS__)
#define ESP_LOGE(...) screen_power_test::log(__VA_ARGS__)
#define ESP_LOGW(...) screen_power_test::log(__VA_ARGS__)
