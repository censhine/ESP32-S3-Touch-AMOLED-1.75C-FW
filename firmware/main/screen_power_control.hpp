#pragma once

#include "esp_err.h"
#include "esp_lcd_types.h"
#include "lvgl.h"

namespace esp_brookesia::systems::phone {
class Phone;
}

namespace brookesia::screen_power {

// Call once, with the LVGL lock held, after startup has enabled touch input.
esp_err_t start(esp_lcd_panel_handle_t panel, lv_indev_t *touch,
                esp_brookesia::systems::phone::Phone *phone);

} // namespace brookesia::screen_power
