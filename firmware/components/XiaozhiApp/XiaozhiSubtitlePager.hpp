#pragma once

#include <stddef.h>
#include <string>
#include <vector>

#include "lvgl.h"

namespace esp_brookesia::apps {

class XiaozhiSubtitlePager {
public:
    bool setText(const char *text, const lv_font_t *font, int32_t line_width);
    void clear();
    void advance();
    const char *current() const;
    size_t pageIndex() const { return _index; }
    size_t pageCount() const { return _pages.size(); }

private:
    std::string _source;
    std::vector<std::string> _pages;
    size_t _index = 0;
};

} // namespace esp_brookesia::apps
