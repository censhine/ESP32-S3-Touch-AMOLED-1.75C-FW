/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "XiaozhiUi.hpp"

#include "esp_lv_adapter.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "bsp/esp-bsp.h"
#include "cbin_font.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "font_awesome.h"

LV_FONT_DECLARE(font_puhui_basic_30_4);
LV_FONT_DECLARE(font_awesome_30_4);

namespace esp_brookesia::apps {

namespace {

constexpr char TAG[] = "XiaozhiUi";
constexpr char COMMON_FONT_PATH[] = BSP_SPIFFS_MOUNT_POINT "/xiaozhi/font.bin";
constexpr size_t MAX_COMMON_FONT_SIZE = 4 * 1024 * 1024;
constexpr int SUBTITLE_SCALE = 205; // 30 px glyphs render at approximately 24 px.
constexpr int STATUS_SCALE = 180;
constexpr int SUBTITLE_LOGICAL_WIDTH = 366;
constexpr int STATUS_LOGICAL_WIDTH = 214;
constexpr uint32_t PAGE_PERIOD_MS = 6000;

void prepareObject(lv_obj_t *object)
{
    lv_obj_remove_style_all(object);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(object, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(object, 0, LV_PART_MAIN);
    lv_obj_clear_flag(object, LV_OBJ_FLAG_SCROLLABLE);
}

void place(lv_obj_t *object, int x, int y, int width, int height)
{
    lv_obj_set_size(object, width, height);
    lv_obj_set_pos(object, x, y);
}

void setHidden(lv_obj_t *object, bool hidden)
{
    if (!object || lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    if (hidden) lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
}

void setLabelTextIfChanged(lv_obj_t *label, const char *text)
{
    if (!label) return;
    const char *value = text ? text : "";
    if (strcmp(lv_label_get_text(label), value) != 0) lv_label_set_text(label, value);
}

size_t nextCodepoint(const std::string &text, size_t offset)
{
    const unsigned char first = static_cast<unsigned char>(text[offset]);
    const size_t size = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
    return offset + size < text.size() ? offset + size : text.size();
}

std::string shortStatus(const char *status, const lv_font_t *font)
{
    const char *source = status ? status : "";
    struct Alias { const char *long_text; const char *short_text; };
    static constexpr Alias aliases[] = {
        {"需要网络连接", "请先联网"}, {"正在准备小智", "准备中"},
        {"等待设备激活", "待激活"}, {"小智正在说话", "小智在说"},
        {"Network connection required", "Need Wi-Fi"},
        {"Preparing Xiaozhi", "Preparing"},
        {"Waiting for device activation", "Activate"},
        {"Xiaozhi is speaking", "Speaking"},
        {"Service unavailable", "Error"},
    };
    for (const auto &alias : aliases) {
        if (strcmp(source, alias.long_text) == 0) return alias.short_text;
    }
    std::string result(source);
    lv_point_t size{};
    lv_text_get_size(&size, result.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
    if (size.x <= STATUS_LOGICAL_WIDTH - 12) return result;
    result.clear();
    const std::string original(source);
    for (size_t i = 0; i < original.size();) {
        const size_t next = nextCodepoint(original, i);
        const std::string candidate = original.substr(0, next) + "...";
        lv_text_get_size(&size, candidate.c_str(), font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_EXPAND);
        if (size.x > STATUS_LOGICAL_WIDTH - 12) break;
        result = original.substr(0, next);
        i = next;
    }
    return result + "...";
}

void scaleLabel(lv_obj_t *label, int scale)
{
    lv_obj_set_style_transform_pivot_x(label, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(label, 0, LV_PART_MAIN);
    lv_obj_set_style_transform_scale(label, scale, LV_PART_MAIN);
}

} // namespace

XiaozhiUi::~XiaozhiUi()
{
    destroy();
    releaseTextFont();
}

bool XiaozhiUi::preload()
{
    _text_font = &font_puhui_basic_30_4;
    if (_cbin_font) _text_font = _cbin_font;
    else (void)loadTextFont();
    return true;
}

bool XiaozhiUi::create(lv_obj_t *parent, ActionCallback action_callback,
                       void *action_context, bool multiline_subtitle)
{
    if (!parent) return false;
    destroyView();
    (void)action_callback;
    (void)action_context;
    (void)multiline_subtitle;
    _text_font = _cbin_font ? _cbin_font : &font_puhui_basic_30_4;

    _root = lv_obj_create(parent);
    prepareObject(_root);
    lv_obj_set_size(_root, lv_pct(100), lv_pct(100));
    lv_obj_align(_root, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(_root, lv_color_hex(0xFFF9ED), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_root, lv_color_hex(0x3B3531), LV_PART_MAIN);
    lv_obj_set_style_text_font(_root, _text_font, LV_PART_MAIN);
    lv_obj_clear_flag(_root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_root, onRootDelete, LV_EVENT_DELETE, this);

    _top_bar = lv_obj_create(_root);
    prepareObject(_top_bar);
    place(_top_bar, 118, 35, 230, 36);
    lv_obj_set_style_radius(_top_bar, 18, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_top_bar, lv_color_hex(0xDCEFE1), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_top_bar, LV_OPA_COVER, LV_PART_MAIN);

    _network_label = lv_label_create(_root);
    place(_network_label, 130, 40, 36, 36);
    lv_obj_set_style_text_font(_network_label, &font_awesome_30_4, LV_PART_MAIN);
    lv_obj_set_style_text_align(_network_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    scaleLabel(_network_label, SUBTITLE_SCALE);

    _status_label = lv_label_create(_root);
    place(_status_label, 169, 40, STATUS_LOGICAL_WIDTH, 40);
    lv_label_set_long_mode(_status_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(_status_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_align(_status_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_status_label, lv_color_hex(0x3E7159), LV_PART_MAIN);
    scaleLabel(_status_label, STATUS_SCALE);

    if (!_avatar.create(_root)) {
        destroyView();
        return false;
    }
    lv_obj_set_pos(_avatar.root(), 93, 85);
    _avatar.setActivity(_activity);
    _avatar.setPaused(_paused);

    _subtitle_bar = lv_obj_create(_root);
    prepareObject(_subtitle_bar);
    place(_subtitle_bar, 83, 307, 300, 96);
    lv_obj_add_flag(_subtitle_bar, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(_subtitle_bar, onSubtitleTap, LV_EVENT_CLICKED, this);

    _subtitle_label = lv_label_create(_root);
    place(_subtitle_label, 83, 307, SUBTITLE_LOGICAL_WIDTH, 120);
    lv_label_set_long_mode(_subtitle_label, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_font(_subtitle_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_align(_subtitle_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(_subtitle_label, 2, LV_PART_MAIN);
    scaleLabel(_subtitle_label, SUBTITLE_SCALE);

    _page_indicator = lv_label_create(_root);
    place(_page_indicator, 168, 413, 130, 24);
    lv_obj_set_style_text_font(_page_indicator, LV_FONT_DEFAULT, LV_PART_MAIN);
    lv_obj_set_style_text_align(_page_indicator, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_page_indicator, lv_color_hex(0x7A8F79), LV_PART_MAIN);

    _activation_box = lv_obj_create(_root);
    prepareObject(_activation_box);
    place(_activation_box, 66, 132, 334, 180);
    _activation_code_label = lv_label_create(_activation_box);
    place(_activation_code_label, 18, 20, 298, 50);
    lv_obj_set_style_text_font(_activation_code_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_align(_activation_code_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    _activation_message_label = lv_label_create(_activation_box);
    place(_activation_message_label, 19, 76, 370, 120);
    lv_label_set_long_mode(_activation_message_label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(_activation_message_label, _text_font, LV_PART_MAIN);
    lv_obj_set_style_text_align(_activation_message_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_style_text_color(_activation_message_label, lv_color_hex(0x5D554C), LV_PART_MAIN);
    scaleLabel(_activation_message_label, SUBTITLE_SCALE);
    setHidden(_activation_box, true);

    _subtitle_timer = lv_timer_create(onSubtitleTimer, PAGE_PERIOD_MS, this);
    lv_timer_pause(_subtitle_timer);
    setNetworkReady(false);
    setStatus("");
    clearChatMessages();
    return true;
}

void XiaozhiUi::destroy()
{
    destroyView();
}

void XiaozhiUi::destroyView()
{
    if (_subtitle_timer) {
        lv_timer_delete(_subtitle_timer);
        _subtitle_timer = nullptr;
    }
    _avatar.destroy();
    if (_root && lv_obj_is_valid(_root)) lv_obj_delete(_root);
    _root = nullptr;
    _top_bar = nullptr;
    _network_label = nullptr;
    _status_label = nullptr;
    _activation_box = nullptr;
    _activation_code_label = nullptr;
    _activation_message_label = nullptr;
    _subtitle_bar = nullptr;
    _subtitle_label = nullptr;
    _page_indicator = nullptr;
    _pager.clear();
    _network_state_valid = false;
    _activation_state_valid = false;
    _activation_visible = false;
}

void XiaozhiUi::onRootDelete(lv_event_t *event)
{
    auto *self = static_cast<XiaozhiUi *>(lv_event_get_user_data(event));
    if (self->_subtitle_timer) {
        lv_timer_delete(self->_subtitle_timer);
        self->_subtitle_timer = nullptr;
    }
    self->_root = nullptr;
    self->_network_label = nullptr;
    self->_status_label = nullptr;
    self->_subtitle_label = nullptr;
    self->_subtitle_bar = nullptr;
    self->_page_indicator = nullptr;
    self->_activation_box = nullptr;
    self->_activation_code_label = nullptr;
    self->_activation_message_label = nullptr;
}

void XiaozhiUi::setNetworkReady(bool ready)
{
    if (!_network_label || (_network_state_valid && _network_ready == ready)) return;
    lv_label_set_text(_network_label, ready ? FONT_AWESOME_WIFI : FONT_AWESOME_WIFI_SLASH);
    lv_obj_set_style_text_color(_network_label,
                                lv_color_hex(ready ? 0x3E7159 : 0x9B9188), LV_PART_MAIN);
    _network_ready = ready;
    _network_state_valid = true;
}

void XiaozhiUi::setStatus(const char *status)
{
    if (!_status_label) return;
    const std::string value = shortStatus(status, _text_font);
    setLabelTextIfChanged(_status_label, value.c_str());
}

void XiaozhiUi::setChatMessage(const char *role, const char *text)
{
    (void)role;
    if (!_subtitle_label) return;
    const bool changed = _pager.setText(text, _text_font, SUBTITLE_LOGICAL_WIDTH - 6);
    showSubtitlePage();
    if (changed && _subtitle_timer) lv_timer_reset(_subtitle_timer);
}

void XiaozhiUi::showSubtitlePage()
{
    if (!_subtitle_label) return;
    setLabelTextIfChanged(_subtitle_label, _pager.current());
    const bool has_text = _pager.pageCount() > 0 && !_activation_visible;
    setHidden(_subtitle_bar, !has_text);
    setHidden(_subtitle_label, !has_text);
    const bool multiple = _pager.pageCount() > 1 && !_activation_visible;
    setHidden(_page_indicator, !multiple);
    if (multiple) {
        char page[24];
        snprintf(page, sizeof(page), "%u / %u",
                 static_cast<unsigned>(_pager.pageIndex() + 1),
                 static_cast<unsigned>(_pager.pageCount()));
        setLabelTextIfChanged(_page_indicator, page);
    }
    if (_subtitle_timer) {
        if (multiple && !_paused) lv_timer_resume(_subtitle_timer);
        else lv_timer_pause(_subtitle_timer);
    }
}

void XiaozhiUi::onSubtitleTimer(lv_timer_t *timer)
{
    auto *self = static_cast<XiaozhiUi *>(lv_timer_get_user_data(timer));
    self->_pager.advance();
    self->showSubtitlePage();
}

void XiaozhiUi::onSubtitleTap(lv_event_t *event)
{
    auto *self = static_cast<XiaozhiUi *>(lv_event_get_user_data(event));
    if (self->_pager.pageCount() <= 1 || self->_activation_visible) return;
    self->_pager.advance();
    self->showSubtitlePage();
    lv_timer_reset(self->_subtitle_timer);
}

void XiaozhiUi::clearChatMessages()
{
    setChatMessage("system", "");
}

void XiaozhiUi::setEmotion(const char *emotion)
{
    _avatar.setEmotion(emotion && emotion[0] ? emotion : "neutral");
}

void XiaozhiUi::setActivity(XiaozhiCatActivity activity)
{
    _activity = activity;
    _avatar.setActivity(activity);
}

void XiaozhiUi::setPaused(bool paused)
{
    _paused = paused;
    _avatar.setPaused(paused);
    if (_subtitle_timer) {
        if (paused || _pager.pageCount() <= 1 || _activation_visible) lv_timer_pause(_subtitle_timer);
        else lv_timer_resume(_subtitle_timer);
    }
}

void XiaozhiUi::setActivation(const char *code, const char *message, bool visible)
{
    if (!_activation_box) return;
    setLabelTextIfChanged(_activation_code_label, code);
    setLabelTextIfChanged(_activation_message_label, message);
    if (!_activation_state_valid || _activation_visible != visible) {
        setHidden(_avatar.root(), visible);
        setHidden(_activation_box, !visible);
        _activation_visible = visible;
        _activation_state_valid = true;
        showSubtitlePage();
    }
}

bool XiaozhiUi::loadTextFont()
{
    if (_cbin_font) {
        _text_font = _cbin_font;
        return true;
    }
    FILE *file = fopen(COMMON_FONT_PATH, "rb");
    if (!file) {
        ESP_LOGW(TAG, "Common font is unavailable: %s", COMMON_FONT_PATH);
        return false;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return false;
    }
    long file_size = ftell(file);
    if (file_size <= 0 || static_cast<size_t>(file_size) > MAX_COMMON_FONT_SIZE ||
        fseek(file, 0, SEEK_SET) != 0) {
        ESP_LOGW(TAG, "Common font has an invalid size: %ld", file_size);
        fclose(file);
        return false;
    }
    size_t size = static_cast<size_t>(file_size);
    auto *data = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (!data) {
        ESP_LOGW(TAG, "No PSRAM for common font (%u bytes)", static_cast<unsigned>(size));
        fclose(file);
        return false;
    }
    size_t read_size = fread(data, 1, size, file);
    fclose(file);
    if (read_size != size) {
        ESP_LOGW(TAG, "Common font read was incomplete (%u/%u)",
                 static_cast<unsigned>(read_size), static_cast<unsigned>(size));
        heap_caps_free(data);
        return false;
    }
    if (esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGW(TAG, "Unable to lock LVGL while decoding the common font");
        heap_caps_free(data);
        return false;
    }
    lv_font_t *font = cbin_font_create(data);
    esp_lv_adapter_unlock();
    if (!font) {
        ESP_LOGW(TAG, "Common font could not be decoded");
        heap_caps_free(data);
        return false;
    }
    _font_data = data;
    _cbin_font = font;
    _text_font = font;
    ESP_LOGI(TAG, "Loaded common font from PSRAM (%u bytes)", static_cast<unsigned>(size));
    return true;
}

void XiaozhiUi::releaseTextFont()
{
    if (_cbin_font && esp_lv_adapter_lock(-1) != ESP_OK) {
        ESP_LOGE(TAG, "Unable to lock LVGL while releasing the common font");
        return;
    }
    if (_cbin_font) {
        cbin_font_delete(_cbin_font);
        _cbin_font = nullptr;
        esp_lv_adapter_unlock();
    }
    if (_font_data) {
        heap_caps_free(_font_data);
        _font_data = nullptr;
    }
}

} // namespace esp_brookesia::apps
