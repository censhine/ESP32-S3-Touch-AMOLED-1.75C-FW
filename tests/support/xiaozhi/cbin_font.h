#pragma once

#include <stdint.h>
#include "lvgl.h"

inline lv_font_t *cbin_font_create(const uint8_t *) { return nullptr; }
inline void cbin_font_delete(lv_font_t *) {}
