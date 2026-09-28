#include "XiaozhiSubtitlePager.hpp"

#include <algorithm>

namespace esp_brookesia::apps {

namespace {

size_t nextCodepoint(const std::string &text, size_t offset)
{
    const unsigned char first = static_cast<unsigned char>(text[offset]);
    size_t length = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
    return std::min(offset + length, text.size());
}

bool isSentenceEnd(const std::string &text, size_t begin, size_t end)
{
    const std::string glyph = text.substr(begin, end - begin);
    return glyph == "." || glyph == "!" || glyph == "?" || glyph == ";" ||
           glyph == "。" || glyph == "！" || glyph == "？" || glyph == "；";
}

bool isWordBreak(const std::string &text, size_t begin, size_t end)
{
    const std::string glyph = text.substr(begin, end - begin);
    return glyph == " " || glyph == "\t" || glyph == "," || glyph == "." ||
           glyph == "，" || glyph == "。" || glyph == "、";
}

int32_t textWidth(const std::string &text, const lv_font_t *font)
{
    lv_point_t size{};
    lv_text_get_size(&size, text.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    return size.x;
}

} // namespace

bool XiaozhiSubtitlePager::setText(const char *text, const lv_font_t *font, int32_t line_width)
{
    const std::string value = text ? text : "";
    if (value == _source && !_pages.empty()) {
        return false;
    }

    clear();
    _source = value;
    if (value.empty() || !font || line_width <= 0) {
        return true;
    }

    size_t offset = 0;
    while (offset < value.size()) {
        std::string page;
        for (int line = 0; line < 3 && offset < value.size(); ++line) {
            const size_t start = offset;
            size_t end = offset;
            size_t last_break = start;
            size_t last_sentence = start;
            while (end < value.size() && value[end] != '\n' && value[end] != '\r') {
                const size_t next = nextCodepoint(value, end);
                if (textWidth(value.substr(start, next - start), font) > line_width && end > start) {
                    break;
                }
                if (isWordBreak(value, end, next)) {
                    last_break = next;
                }
                if (isSentenceEnd(value, end, next)) {
                    last_sentence = next;
                }
                end = next;
            }
            if (end == start && end < value.size() && value[end] != '\n' && value[end] != '\r') {
                end = nextCodepoint(value, end);
            }
            // Keep Latin words together when a useful break is available.
            if (end < value.size() && value[end] != '\n' && value[end] != '\r' &&
                last_break > start + (end - start) / 2) {
                end = last_break;
            }
            // On the final line, a nearby sentence boundary makes the next page start cleanly.
            if (line == 2 && last_sentence > start + (end - start) * 2 / 3) {
                end = last_sentence;
            }
            if (line > 0) {
                page += '\n';
            }
            page.append(value, start, end - start);
            offset = end;
            if (offset < value.size() && value[offset] == '\r') ++offset;
            if (offset < value.size() && value[offset] == '\n') ++offset;
        }
        _pages.push_back(std::move(page));
    }
    return true;
}

void XiaozhiSubtitlePager::clear()
{
    _source.clear();
    _pages.clear();
    _index = 0;
}

void XiaozhiSubtitlePager::advance()
{
    if (_pages.size() > 1) {
        _index = (_index + 1) % _pages.size();
    }
}

const char *XiaozhiSubtitlePager::current() const
{
    return _pages.empty() ? "" : _pages[_index].c_str();
}

} // namespace esp_brookesia::apps
