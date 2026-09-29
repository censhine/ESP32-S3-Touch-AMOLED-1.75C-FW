#include "screen_power_control.hpp"

#include <cstring>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_brookesia.hpp"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "short_press_button.hpp"

namespace brookesia::screen_power {
namespace {

constexpr char TAG[] = "ScreenPower";
constexpr uint32_t POLL_PERIOD_MS = 20;
constexpr uint32_t IDLE_TIMEOUT_MS = 120 * 1000;

struct Control {
    esp_lcd_panel_handle_t panel = nullptr;
    lv_indev_t *touch = nullptr;
    lv_display_t *display = nullptr;
    esp_brookesia::systems::phone::Phone *phone = nullptr;
    lv_timer_t *timer = nullptr;
    ShortPressButton button;
    bool screen_off = false;
    uint32_t off_since_ms = 0;
    bool panel_needs_recovery = false;
    int saved_brightness = 0;
    bool power_was_pressed = false;
    bool boot_was_pressed = false;
};

Control control;

bool set_screen_on(Control &state, bool turn_on, const char *reason)
{
    if (turn_on && state.panel_needs_recovery) {
        esp_err_t restored = esp_lcd_panel_disp_sleep(state.panel, false);
        if (restored == ESP_OK) restored = bsp_display_brightness_set(state.saved_brightness);
        if (restored != ESP_OK) {
            ESP_LOGE(TAG, "Panel standby recovery failed: %s", esp_err_to_name(restored));
            return false;
        }
    }
    // This runs inside the adapter's LVGL lock, serialized with panel flushes.
    // DISPON/DISPOFF retains brightness and panel RAM; no deep sleep or reset.
    const esp_err_t result = esp_lcd_panel_disp_on_off(state.panel, turn_on);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Cannot switch display %s: %s", turn_on ? "on" : "off",
                 esp_err_to_name(result));
        return false;
    }
    if (turn_on) state.panel_needs_recovery = false;

    lv_indev_reset(state.touch, nullptr);
    if (turn_on) {
        // A finger already on the glass must be lifted before interacting.
        lv_indev_wait_release(state.touch);
    }
    lv_indev_enable(state.touch, turn_on);

    // LVGL automatically resumes refresh on invalidation. Suppress both while
    // dark, keeping application timers and our POWER polling timer alive.
    // Every successful off/on pair balances LVGL's invalidation counter once.
    lv_timer_t *refresh = lv_display_get_refr_timer(state.display);
    lv_display_enable_invalidation(state.display, turn_on);
    if (turn_on) {
        lv_display_trigger_activity(state.display);
        lv_obj_invalidate(lv_display_get_layer_sys(state.display));
        if (refresh != nullptr) {
            lv_timer_resume(refresh);
            lv_timer_ready(refresh);
        }
    } else if (refresh != nullptr) {
        lv_timer_pause(refresh);
    }
    state.screen_off = !turn_on;
    if (!turn_on) {
        state.off_since_ms = lv_tick_get();
    }
    ESP_LOGI(TAG, "%s: screen %s; heap internal=%zu largest=%zu psram=%zu",
             reason, turn_on ? "on" : "off",
             heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    return true;
}

void poll_power_button(lv_timer_t *timer)
{
    auto &state = *static_cast<Control *>(lv_timer_get_user_data(timer));
    const bool power_pressed = gpio_get_level(BSP_BUTTON_PWR_GPIO) != 0;
    const bool boot_pressed = gpio_get_level(BSP_BUTTON_BOOT_GPIO) == 0;
    // Count held keys and their release as activity, without consuming BOOT or
    // altering its existing app-specific actions. LVGL tracks touch itself.
    if (!state.screen_off && (power_pressed || boot_pressed ||
        power_pressed != state.power_was_pressed || boot_pressed != state.boot_was_pressed)) {
        lv_display_trigger_activity(state.display);
    }
    state.power_was_pressed = power_pressed;
    state.boot_was_pressed = boot_pressed;

    const auto *app = state.phone->getManager().getActiveApp();
    // Diagnostics must be able to exercise PWR without hiding their result.
    const bool testing_buttons = !state.screen_off && app != nullptr &&
        std::strcmp(app->getName(), "Button Test") == 0;
    if (state.button.update(power_pressed, lv_tick_get(), testing_buttons)) {
        set_screen_on(state, state.screen_off, "POWER short press");
    } else if (!state.screen_off &&
               lv_display_get_inactive_time(state.display) >= IDLE_TIMEOUT_MS) {
        if (!set_screen_on(state, false, "Idle 120s")) {
            // A hardware error must not cause a 50 Hz command/log retry loop.
            lv_display_trigger_activity(state.display);
        }
    }
}

} // namespace

bool is_off()
{
    return control.timer != nullptr && control.screen_off;
}

uint32_t off_duration_ms()
{
    return is_off() ? lv_tick_get() - control.off_since_ms : 0;
}

esp_err_t sleep_panel_for_standby()
{
    if (!is_off() || control.panel_needs_recovery) return ESP_ERR_INVALID_STATE;
    control.saved_brightness = bsp_display_brightness_get();
    // The driver may accept SLPIN before a subsequent deep-standby command
    // fails. Always require recovery after any attempted panel sleep.
    control.panel_needs_recovery = true;
    return esp_lcd_panel_disp_sleep(control.panel, true);
}

bool needs_standby_recovery()
{
    return control.panel_needs_recovery;
}

bool wake_from_standby()
{
    if (control.timer == nullptr) {
        return false;
    }
    // GPIO wake occurs on the press edge. Consume that entire press, including
    // its release, so the ordinary short-press handler cannot blank it again.
    control.button = ShortPressButton{};
    control.power_was_pressed = gpio_get_level(BSP_BUTTON_PWR_GPIO) != 0;
    control.boot_was_pressed = gpio_get_level(BSP_BUTTON_BOOT_GPIO) == 0;
    control.button.update(control.power_was_pressed, lv_tick_get());
    return !control.screen_off || set_screen_on(control, true, "Standby wake");
}

esp_err_t start(esp_lcd_panel_handle_t panel, lv_indev_t *touch,
                esp_brookesia::systems::phone::Phone *phone)
{
    if (panel == nullptr || touch == nullptr || phone == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (control.timer != nullptr) {
        return ESP_ERR_INVALID_STATE;
    }
    lv_display_t *display = lv_indev_get_display(touch);
    if (display == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }

    // GPIO3 is active high through the board's external conditioning circuit.
    // Polling also remains compatible with Button Test's input reconfiguration.
    const gpio_config_t config = {
        .pin_bit_mask = 1ULL << BSP_BUTTON_PWR_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t result = gpio_config(&config);
    if (result != ESP_OK) {
        return result;
    }
    // Match Gravitysphere and Button Test. GPIO reads require input enabled;
    // an unconfigured GPIO0 can otherwise look like a permanently held BOOT.
    const gpio_config_t boot_config = {
        .pin_bit_mask = 1ULL << BSP_BUTTON_BOOT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    const esp_err_t boot_result = gpio_config(&boot_config);
    if (boot_result != ESP_OK) {
        return boot_result;
    }

    control.panel = panel;
    control.touch = touch;
    control.display = display;
    control.phone = phone;
    control.power_was_pressed = gpio_get_level(BSP_BUTTON_PWR_GPIO) != 0;
    control.boot_was_pressed = gpio_get_level(BSP_BUTTON_BOOT_GPIO) == 0;
    control.button.update(control.power_was_pressed, lv_tick_get());
    control.timer = lv_timer_create(poll_power_button, POLL_PERIOD_MS, &control);
    if (control.timer == nullptr) {
        control = {};
        return ESP_ERR_NO_MEM;
    }
    lv_display_trigger_activity(display);
    ESP_LOGI(TAG, "POWER GPIO%d ready: short press toggles display; idle timeout=120s; BOOT unchanged",
             static_cast<int>(BSP_BUTTON_PWR_GPIO));
    return ESP_OK;
}

} // namespace brookesia::screen_power
