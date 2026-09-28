#include "screen_power_control.hpp"
#include "esp_brookesia.hpp"
#include "test_support.hpp"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {
using namespace screen_power_test;
esp_brookesia::systems::phone::Phone phone;
esp_brookesia::systems::phone::App app;
int checks = 0;

void require(bool condition, const char *message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void poll(uint32_t now) {
    state.now = now;
    require(state.poll_timer.callback != nullptr, "service installed a poll callback");
    state.poll_timer.callback(&state.poll_timer);
}
void start(uint32_t now = 10000) {
    state.now = now;
    phone.manager.active = &app;
    require(brookesia::screen_power::start(&state.panel, &state.touch, &phone) == ESP_OK, "start succeeds");
    poll(now + 40); // Arm the real ShortPressButton with a stable released input.
}
void advance(uint32_t elapsed) { poll(state.now + elapsed); }
void power_click() {
    state.gpio[3] = 1;
    advance(20);
    advance(40);
    state.gpio[3] = 0;
    advance(20);
    advance(40);
}
void require_off() {
    require(!state.panel_on, "display is off");
    require(!state.touch.enabled, "off screen disables touch input");
    require(!state.display.invalidation_enabled && state.display.invalidation_count == 0,
            "off screen disables invalidation exactly once");
    require(state.display.refresh_timer.paused, "off screen pauses the existing refresh timer");
    require(!state.poll_timer.paused, "power polling remains active while screen is off");
    require(!state.wrong_refresh_timer, "only the display refresh timer is paused or resumed");
}
void require_on() {
    require(state.panel_on && state.touch.enabled, "lit screen and touch are enabled");
    require(state.display.invalidation_enabled && state.display.invalidation_count == 1,
            "lit screen restores the original invalidation reference count");
    require(!state.display.refresh_timer.paused, "lit screen resumes refresh");
    require(!state.wrong_refresh_timer, "refresh operations target the existing display timer");
}
int event_index(Event event) {
    for (int i = 0; i < state.event_count; ++i) if (state.events[i] == event) return i;
    return -1;
}
struct RenderingSnapshot {
    bool panel_on = state.panel_on;
    bool touch_enabled = state.touch.enabled;
    bool wait_release = state.touch.wait_release;
    bool refresh_paused = state.display.refresh_timer.paused;
    int invalidation_count = state.display.invalidation_count;
    int touch_resets = state.touch_resets;
    int touch_waits = state.touch_waits;
    int refresh_pauses = state.refresh_pauses;
    int refresh_resumes = state.refresh_resumes;
    int refresh_readies = state.refresh_readies;
    int invalidations = state.display.system_layer.invalidations;
    void require_unchanged() const {
        require(state.panel_on == panel_on && state.touch.enabled == touch_enabled &&
                    state.touch.wait_release == wait_release && state.touch_resets == touch_resets &&
                    state.touch_waits == touch_waits, "failed panel command preserves panel and touch state");
        require(state.display.refresh_timer.paused == refresh_paused &&
                    state.display.invalidation_count == invalidation_count &&
                    state.refresh_pauses == refresh_pauses && state.refresh_resumes == refresh_resumes &&
                    state.refresh_readies == refresh_readies &&
                    state.display.system_layer.invalidations == invalidations,
                "failed panel command preserves every rendering operation");
    }
};
void test_idle_boundary() {
    start();
    state.display.activity = state.now;
    const auto activity = state.display.activity;
    poll(activity + 119999);
    require(state.panel_on && state.panel_commands == 0, "119999 ms idle keeps screen on");
    poll(activity + 120000);
    require(!state.panel_on && state.panel_commands == 1, "120000 ms idle turns screen off exactly once");
    require_off();
    require(state.touch_resets == 1, "idle-off resets touch state once");
    poll(activity + 240000);
    require(state.panel_commands == 1, "off screen does not repeat off commands");
}
void test_start_setup() {
    start();
    require(state.display.activity == 10000, "startup grants a fresh full idle window");
    require(state.activity_triggers >= 1, "startup reports activity to LVGL");
    require(state.timer_creates == 1 && state.poll_timer.period == 20, "exactly one 20ms power timer is created");
    bool pwr_configured = false;
    bool boot_configured = false;
    for (int i = 0; i < state.gpio_configs; ++i) {
        const auto &config = state.configs[i];
        pwr_configured |= config.pin_bit_mask == (1ULL << 3) && config.pull_up_en == GPIO_PULLUP_DISABLE;
        boot_configured |= config.pin_bit_mask == 1 && config.pull_up_en == GPIO_PULLUP_ENABLE;
    }
    require(pwr_configured && boot_configured, "PWR input has no pullup and BOOT input retains pullup");
    require_on();
    require(brookesia::screen_power::start(&state.panel, &state.touch, &phone) == ESP_ERR_INVALID_STATE,
            "duplicate start cannot create another timer");
    require(state.timer_creates == 1, "duplicate start keeps one timer");
}
void test_null_display() {
    state.touch.display = nullptr;
    require(brookesia::screen_power::start(&state.panel, &state.touch, &phone) == ESP_ERR_INVALID_ARG,
            "touch without a display is rejected");
    require(state.timer_creates == 0 && state.panel_commands == 0, "invalid input allocates no timer and sends no panel command");
}
void test_touch_activity() {
    start();
    state.now += 119000;
    lv_display_trigger_activity(&state.display); // Normal LVGL input activity.
    const auto touched = state.now;
    poll(touched + 119999);
    require(state.panel_commands == 0, "touch activity restarts the inactivity window");
    poll(touched + 120000);
    require_off();
}
void test_manual_off_on() {
    start();
    power_click();
    require_off();
    require(state.panel_commands == 1 && state.touch_resets == 1, "manual off performs one transition");
    state.event_count = 0;
    power_click();
    require_on();
    require(state.panel_commands == 2 && state.touch_resets == 2, "manual wake performs one transition");
    require(state.touch.wait_release && state.touch_waits == 1, "wake waits for a finger already touching to release");
    require(event_index(TouchReset) >= 0 && event_index(TouchReset) < event_index(TouchWait) &&
                event_index(TouchWait) < event_index(TouchEnable), "wake clears touch then waits for release before enabling input");
    require(event_index(InvalidationOn) < event_index(FullInvalidate) &&
                event_index(FullInvalidate) < event_index(RefreshReady), "wake invalidates the full system layer before requesting a refresh");
    require(state.display.system_layer.invalidations == 1 && state.refresh_resumes == 1 && state.refresh_readies == 1,
            "wake invalidates once and resumes/readies the existing refresh timer");
    require(state.heap_free_queries >= 2 && state.heap_largest_queries >= 2,
            "transitions sample free heap and largest free block");
}
void test_wake_window() {
    start();
    power_click();
    advance(300000);
    power_click();
    const auto woke = state.now;
    require(state.display.activity == woke, "POWER wake resets LVGL inactivity");
    poll(woke + 119999);
    require_on();
    poll(woke + 120000);
    require_off();
    require(state.panel_commands == 3, "wake receives an independent full 120-second window");
}
void test_boot_activity() {
    start();
    poll(state.display.activity + 119980);
    state.gpio[0] = 0;
    advance(20);
    require_on();
    advance(180000);
    require_on();
    require(state.panel_commands == 0, "holding BOOT never toggles display or times out");
    state.gpio[0] = 1;
    advance(20);
    const auto released = state.now;
    require(state.display.activity == released, "BOOT release counts as activity");
    poll(released + 119999);
    require_on();
    poll(released + 120000);
    require_off();
    state.gpio[0] = 0;
    advance(20);
    state.gpio[0] = 1;
    advance(40);
    require(state.panel_commands == 1, "BOOT remains unable to wake the display");
}
void test_power_activity() {
    start();
    poll(state.display.activity + 119980);
    state.gpio[3] = 1;
    advance(20);
    advance(40);
    advance(180000);
    require_on();
    state.gpio[3] = 0;
    advance(20);
    const auto released = state.now;
    require(state.display.activity == released, "PWR release counts as activity");
    advance(40);
    require(state.panel_commands == 0, "long PWR hold does not toggle");
    poll(released + 119999);
    require_on();
    poll(released + 120000);
    require_off();
}
void test_button_test() {
    app.name = "Button Test";
    start();
    power_click();
    require_on();
    require(state.panel_commands == 0, "Button Test inhibits PWR screen toggle");
    const auto activity = state.display.activity;
    require(activity >= 10100, "PWR interaction in Button Test still updates activity");
    poll(activity + 119999);
    require_on();
    poll(activity + 120000);
    require_off();
    advance(20);
    advance(40);
    power_click();
    require_on();
    require(state.panel_commands == 2, "Button Test can idle off and then wake with POWER");
}
void test_failed_manual_off() {
    start();
    const RenderingSnapshot before;
    state.panel_result = ESP_FAIL;
    power_click();
    before.require_unchanged();
    require(state.panel_commands == 1, "manual command failure is attempted once");
    state.panel_result = ESP_OK;
    power_click();
    require_off();
    require(state.panel_commands == 2, "failed off leaves controller state ready for a later off");
}
void test_failed_wake() {
    start();
    power_click();
    const RenderingSnapshot before;
    state.panel_result = ESP_FAIL;
    power_click();
    before.require_unchanged();
    state.panel_result = ESP_OK;
    power_click();
    require_on();
    require(state.panel_commands == 3, "failed wake leaves controller state off until successful wake");
}
void test_failed_idle() {
    start();
    const auto deadline = state.display.activity + 120000;
    const RenderingSnapshot before;
    state.panel_result = ESP_FAIL;
    poll(deadline);
    before.require_unchanged();
    require(state.panel_commands == 1, "idle-off command failure is attempted once");
    require(state.display.activity == deadline, "failed idle-off starts a fresh retry window");
    poll(deadline + 20);
    poll(deadline + 119999);
    require(state.panel_commands == 1, "failed idle-off does not flood panel commands every poll");
    state.panel_result = ESP_OK;
    poll(deadline + 120000);
    require_off();
    require(state.panel_commands == 2, "idle-off retries after a full 120 seconds");
}
void test_repeated_cycles() {
    start();
    auto *timer = &state.poll_timer;
    void *user_data = timer->user_data;
    for (int i = 0; i < 20000; ++i) {
        state.event_count = 0;
        power_click();
        require_off();
        power_click();
        require_on();
    }
    require(state.panel_commands == 40000, "20000 cycles perform exactly 40000 transitions");
    require(state.timer_creates == 1 && state.poll_timer.user_data == user_data,
            "repeated transitions reuse the same timer and control object");
    require(state.refresh_pauses == 20000 && state.refresh_resumes == 20000 && state.refresh_readies == 20000,
            "refresh timer pause/resume operations stay balanced over 20000 cycles");
    require(state.display.system_layer.invalidations == 20000 && state.touch_waits == 20000,
            "each wake refreshes the screen and waits for touch release exactly once");
}
void test_tick_wrap() {
    start(0xffff0000U);
    const auto activity = state.display.activity;
    poll(activity + 119999U);
    require_on();
    poll(activity + 120000U);
    require_off();
    power_click();
    const auto woke = state.now;
    poll(woke + 119999U);
    require_on();
    poll(woke + 120000U);
    require_off();
}
}

int main(int argc, char **argv) {
    const std::string scenario = argc > 1 ? argv[1] : "idle_boundary";
    if (scenario == "idle_boundary") test_idle_boundary();
    else if (scenario == "start_setup") test_start_setup();
    else if (scenario == "null_display") test_null_display();
    else if (scenario == "touch_activity") test_touch_activity();
    else if (scenario == "manual_off_on") test_manual_off_on();
    else if (scenario == "wake_window") test_wake_window();
    else if (scenario == "boot_activity") test_boot_activity();
    else if (scenario == "power_activity") test_power_activity();
    else if (scenario == "button_test") test_button_test();
    else if (scenario == "failed_manual_off") test_failed_manual_off();
    else if (scenario == "failed_wake") test_failed_wake();
    else if (scenario == "failed_idle") test_failed_idle();
    else if (scenario == "repeated_cycles") test_repeated_cycles();
    else if (scenario == "tick_wrap") test_tick_wrap();
    else { std::cerr << "Unknown scenario\n"; return 2; }
    std::cout << "PASS: " << scenario << " (" << checks << " checks)\n";
}
