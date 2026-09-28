#include "XiaozhiUi.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

LV_FONT_DECLARE(font_puhui_30_4);

namespace {

int checks = 0;
lv_point_t pointer_pos{233, 350};
bool pointer_down = false;

void require(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void readPointer(lv_indev_t *, lv_indev_data_t *data)
{
    data->point = pointer_pos;
    data->state = pointer_down ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void tap(lv_indev_t *input, int x, int y)
{
    pointer_pos = {x, y};
    pointer_down = true;
    lv_tick_inc(35);
    lv_indev_read(input);
    pointer_down = false;
    lv_tick_inc(35);
    lv_indev_read(input);
    lv_timer_handler();
}

lv_obj_t *findLabel(lv_obj_t *parent, const std::string &text)
{
    if (lv_obj_check_type(parent, &lv_label_class) && lv_label_get_text(parent) == text)
        return parent;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        if (auto *result = findLabel(lv_obj_get_child(parent, i), text)) return result;
    }
    return nullptr;
}

std::string indicatorText(lv_obj_t *parent)
{
    if (lv_obj_check_type(parent, &lv_label_class)) {
        const std::string text = lv_label_get_text(parent);
        if (text.find(" / ") != std::string::npos) return text;
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i) {
        const std::string result = indicatorText(lv_obj_get_child(parent, i));
        if (!result.empty()) return result;
    }
    return {};
}

int indicatorPage(lv_obj_t *parent)
{
    int page = 0;
    const std::string text = indicatorText(parent);
    if (!text.empty()) std::sscanf(text.c_str(), "%d", &page);
    return page;
}

void savePreview(const std::vector<uint32_t> &pixels)
{
    std::ofstream file("artifacts/xiaozhi-ui.ppm", std::ios::binary);
    require(file.good(), "preview output opens");
    file << "P6\n466 466\n255\n";
    for (uint32_t pixel : pixels) {
        const char rgb[3] = {
            static_cast<char>((pixel >> 16) & 0xFF),
            static_cast<char>((pixel >> 8) & 0xFF),
            static_cast<char>(pixel & 0xFF),
        };
        file.write(rgb, 3);
    }
}

void checkRenderedSubtitle(const std::vector<uint32_t> &pixels)
{
    int row_pixels[3] = {};
    for (int y = 310; y <= 402; ++y) {
        for (int x = 0; x < 466; ++x) {
            const uint32_t pixel = pixels[y * 466 + x];
            const int red = (pixel >> 16) & 0xFF;
            const int green = (pixel >> 8) & 0xFF;
            const int blue = pixel & 0xFF;
            if (red >= 115 || green >= 110 || blue >= 105) continue;
            require(x >= 83 && x <= 383, "rendered subtitle stays within safe chord");
            const int dx = x - 233;
            const int dy = y - 233;
            require(dx * dx + dy * dy < 233 * 233,
                    "rendered subtitle stays within round display");
            const int band = y < 340 ? 0 : y < 372 ? 1 : 2;
            ++row_pixels[band];
        }
    }
    for (int count : row_pixels) require(count > 40, "each subtitle line renders pixels");
}

bool validUtf8(const std::string &text)
{
    for (size_t i = 0; i < text.size();) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        const size_t count = byte < 0x80 ? 1 : byte < 0xE0 ? 2 : byte < 0xF0 ? 3 : 4;
        if (i + count > text.size()) return false;
        for (size_t n = 1; n < count; ++n)
            if ((static_cast<unsigned char>(text[i + n]) & 0xC0) != 0x80) return false;
        i += count;
    }
    return true;
}

std::string withoutBreaks(const std::string &text)
{
    std::string result;
    for (char ch : text) if (ch != '\n' && ch != '\r') result += ch;
    return result;
}

void testPager()
{
    using esp_brookesia::apps::XiaozhiSubtitlePager;
    XiaozhiSubtitlePager pager;
    const std::string story =
        "今天我们来聊聊一个很有意思的话题。天气不错，我们可以去公园散步，"
        "然后读一本 English book，喝一杯热茶。接下来看看 tomorrow's weather "
        "forecast! 这段文字还会继续，以便检查很多页面是否完整，标点是否留在前一页。"
        "最后一页也必须能回看。";
    require(pager.setText(story.c_str(), &font_puhui_30_4, 360), "new text accepted");
    require(pager.pageCount() > 1, "long CJK and Latin text paginates");
    std::string recovered;
    const size_t count = pager.pageCount();
    for (size_t i = 0; i < count; ++i) {
        const std::string page = pager.current();
        require(validUtf8(page), "each page ends at a UTF-8 boundary");
        require(std::count(page.begin(), page.end(), '\n') <= 2, "at most three lines per page");
        recovered += withoutBreaks(page);
        size_t start = 0;
        for (;;) {
            const size_t end = page.find('\n', start);
            const std::string line = page.substr(start, end - start);
            lv_point_t size{};
            lv_text_get_size(&size, line.c_str(), &font_puhui_30_4, 0, 0,
                             LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
            require(size.x <= 360, "line fits measured font width");
            if (end == std::string::npos) break;
            start = end + 1;
        }
        pager.advance();
    }
    require(recovered == withoutBreaks(story), "all Unicode content survives pagination");
    require(pager.pageIndex() == 0, "advance wraps for review");
    pager.advance();
    require(!pager.setText(story.c_str(), &font_puhui_30_4, 360), "identical refresh ignored");
    require(pager.pageIndex() == 1, "identical refresh keeps page");
    require(pager.setText("第一行\n第二行\n第三行", &font_puhui_30_4, 360), "explicit lines accepted");
    require(pager.pageCount() == 1 &&
            std::string(pager.current()) == "第一行\n第二行\n第三行",
            "three explicit lines remain complete");
    pager.clear();
    require(pager.pageCount() == 0, "clear removes pages");
}

} // namespace

int main()
{
    using esp_brookesia::apps::XiaozhiUi;
    lv_init();
    auto *display = lv_display_create(466, 466);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    std::vector<uint32_t> pixels(466 * 466);
    lv_display_set_buffers(display, pixels.data(), nullptr, pixels.size() * sizeof(uint32_t),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, [](lv_display_t *d, const lv_area_t *, uint8_t *) {
        lv_display_flush_ready(d);
    });
    auto *input = lv_indev_create();
    lv_indev_set_type(input, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(input, readPointer);

    testPager();
    const auto original_children = lv_obj_get_child_count(lv_screen_active());
    XiaozhiUi ui;
    require(ui.preload(), "preload succeeds with basic host font");
    require(ui.create(lv_screen_active(), nullptr, nullptr, true), "view creates");
    ui.setNetworkReady(true);
    ui.setStatus("小智在听");
    lv_obj_update_layout(lv_screen_active());
    auto *wifi = findLabel(ui.root(), "\xef\x87\xab");
    auto *status = findLabel(ui.root(), "小智在听");
    require(wifi && status, "Wi-Fi and status visible");
    lv_area_t wifi_area{}, status_area{};
    lv_obj_get_coords(wifi, &wifi_area);
    lv_obj_get_coords(status, &status_area);
    require(wifi_area.x2 < status_area.x1, "Wi-Fi and status regions disjoint");
    require(status_area.x1 >= 118 && status_area.x1 < 348, "status sits in pill");

    const std::string three_lines =
        "今天阳光好，一起散步。\n"
        "秋风吹来，看看云彩。\n"
        "坐下喝茶，聊聊故事。";
    ui.setChatMessage("assistant", three_lines.c_str());
    lv_obj_update_layout(lv_screen_active());
    auto *subtitle = findLabel(ui.root(), three_lines);
    if (!subtitle) {
        // The pager inserts deliberate line breaks before rendering.
        for (uint32_t i = 0; i < lv_obj_get_child_count(ui.root()); ++i) {
            auto *child = lv_obj_get_child(ui.root(), i);
            if (lv_obj_check_type(child, &lv_label_class) &&
                withoutBreaks(lv_label_get_text(child)) == three_lines) subtitle = child;
        }
    }
    require(subtitle != nullptr, "subtitle renders all three lines");
    const std::string subtitle_text = lv_label_get_text(subtitle);
    require(std::count(subtitle_text.begin(), subtitle_text.end(), '\n') == 2,
            "subtitle contains three complete lines");
    require(lv_obj_get_style_transform_scale_x(subtitle, LV_PART_MAIN) == 205,
            "full Chinese font is rendered at 24px scale");
    lv_point_t measured_text{};
    lv_text_get_size(&measured_text, subtitle_text.c_str(), &font_puhui_30_4,
                     0, 2, lv_obj_get_width(subtitle), LV_TEXT_FLAG_NONE);
    require(measured_text.y <= lv_obj_get_height(subtitle),
            "three measured font lines fit without vertical clipping");
    lv_area_t subtitle_area{};
    lv_obj_get_coords(subtitle, &subtitle_area);
    require(subtitle_area.x1 == 83 && subtitle_area.y1 == 307,
            "subtitle starts at approved safe rectangle");
    lv_obj_get_transformed_area(subtitle, &subtitle_area,
                                LV_OBJ_POINT_TRANSFORM_FLAG_RECURSIVE);
    for (int x : {subtitle_area.x1, subtitle_area.x2}) {
        for (int y : {subtitle_area.y1, subtitle_area.y2}) {
            const int dx = x - 233;
            const int dy = y - 233;
            require(dx * dx + dy * dy < 233 * 233,
                    "transformed subtitle box stays inside round display");
        }
    }
    require(indicatorText(ui.root()).empty(), "single page has no indicator");
    lv_obj_invalidate(ui.root());
    lv_timer_handler();
    savePreview(pixels);
    checkRenderedSubtitle(pixels);

    const std::string long_text = withoutBreaks(three_lines) +
        withoutBreaks(three_lines) + withoutBreaks(three_lines);
    ui.setChatMessage("assistant", long_text.c_str());
    require(indicatorPage(ui.root()) == 1, "multiple pages show current page indicator");
    tap(input, 20, 233);
    require(indicatorPage(ui.root()) == 1, "edge gesture does not advance pages");
    tap(input, 233, 350);
    require(indicatorPage(ui.root()) == 2, "subtitle tap advances page");
    ui.setChatMessage("assistant", long_text.c_str());
    require(indicatorPage(ui.root()) == 2, "repeated UI refresh keeps current page");
    lv_tick_inc(6001);
    lv_timer_handler();
    require(indicatorPage(ui.root()) == 3, "timer advances without another text update");
    ui.setPaused(true);
    lv_tick_inc(6001);
    lv_timer_handler();
    require(indicatorPage(ui.root()) == 3, "pause holds current page");
    ui.setPaused(false);
    ui.setActivation("482916", "请在控制面板输入激活码。", true);
    require(findLabel(ui.root(), "482916") != nullptr, "activation code remains visible");
    ui.setActivation("", "", false);
    require(indicatorPage(ui.root()) == 3, "subtitle page survives activation mode");

    ui.destroy();
    require(lv_obj_get_child_count(lv_screen_active()) == original_children,
            "destroy removes UI tree");
    for (int i = 0; i < 100; ++i) {
        require(ui.create(lv_screen_active(), nullptr, nullptr, true), "recreate succeeds");
        ui.setChatMessage("assistant", long_text.c_str());
        ui.destroy();
    }
    auto *container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(container, 466, 466);
    require(ui.create(container, nullptr, nullptr, true), "nested view creates");
    ui.setChatMessage("assistant", long_text.c_str());
    lv_obj_delete(container);
    ui.destroy();
    lv_tick_inc(6001);
    lv_timer_handler();
    require(lv_obj_get_child_count(lv_screen_active()) == original_children,
            "parent-first delete clears timers and pointers");

    lv_indev_delete(input);
    lv_display_delete(display);
    lv_deinit();
    std::cout << "PASS: " << checks << " Xiaozhi LVGL layout and pager checks\n";
}
