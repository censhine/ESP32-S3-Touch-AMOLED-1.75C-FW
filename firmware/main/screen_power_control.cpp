#include "screen_power_control.hpp"

#include <cstring>

#include "bsp/esp-bsp.h"
#include "driver/gpio.h"
#include "esp_brookesia.hpp"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "short_press_button.hpp"

namespace brookesia::screen_power {
namespace {

constexpr char TAG[] = "ScreenPower";
constexpr uint32_t POLL_PERIOD_MS = 20;

struct Control {
    esp_lcd_panel_handle_t panel = nullptr;
    lv_indev_t *touch = nullptr;
    esp_brookesia::systems::phone::Phone *phone = nullptr;
    lv_timer_t *timer = nullptr;
    ShortPressButton button;
    bool screen_off = false;
};

Control control;

void poll_power_button(lv_timer_t *timer)
{
    auto &state = *static_cast<Control *>(lv_timer_get_user_data(timer));
    const auto *app = state.phone->getManager().getActiveApp();
    // Diagnostics must be able to exercise PWR without hiding their result.
    const bool testing_buttons = !state.screen_off && app != nullptr &&
        std::strcmp(app->getName(), "Button Test") == 0;
    if (!state.button.update(gpio_get_level(BSP_BUTTON_PWR_GPIO) != 0,
                             lv_tick_get(), testing_buttons)) {
        return;
    }

    const bool turn_on = state.screen_off;
    // This runs inside the adapter's LVGL lock, serialized with panel flushes.
    // DISPON/DISPOFF retains brightness and panel RAM; no deep sleep or reset.
    const esp_err_t result = esp_lcd_panel_disp_on_off(state.panel, turn_on);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Cannot switch display %s: %s", turn_on ? "on" : "off",
                 esp_err_to_name(result));
        return;
    }

    lv_indev_reset(state.touch, nullptr);
    if (turn_on) {
        // A finger already on the glass must be lifted before interacting.
        lv_indev_wait_release(state.touch);
    }
    lv_indev_enable(state.touch, turn_on);
    state.screen_off = !turn_on;
    ESP_LOGI(TAG, "POWER short press: screen %s", turn_on ? "on" : "off");
}

} // namespace

esp_err_t start(esp_lcd_panel_handle_t panel, lv_indev_t *touch,
                esp_brookesia::systems::phone::Phone *phone)
{
    if (panel == nullptr || touch == nullptr || phone == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (control.timer != nullptr) {
        return ESP_ERR_INVALID_STATE;
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

    control.panel = panel;
    control.touch = touch;
    control.phone = phone;
    control.button.update(gpio_get_level(BSP_BUTTON_PWR_GPIO) != 0, lv_tick_get());
    control.timer = lv_timer_create(poll_power_button, POLL_PERIOD_MS, &control);
    if (control.timer == nullptr) {
        control = {};
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "POWER GPIO%d ready: short press toggles display; BOOT unchanged",
             static_cast<int>(BSP_BUTTON_PWR_GPIO));
    return ESP_OK;
}

} // namespace brookesia::screen_power
