#pragma once

#include "screen_power_control.hpp"

namespace brookesia::power {
// Call after screen_power::start, without holding the LVGL lock.
esp_err_t start(esp_lcd_panel_handle_t panel, lv_display_t *display,
                esp_brookesia::systems::phone::Phone *phone);
}
