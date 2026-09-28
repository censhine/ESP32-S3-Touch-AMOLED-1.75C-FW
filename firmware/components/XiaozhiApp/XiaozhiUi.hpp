/*
 * SPDX-FileCopyrightText: 2026 Waveshare
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"
#include "XiaozhiCatAvatar.hpp"
#include "XiaozhiSubtitlePager.hpp"

namespace esp_brookesia::apps {

class XiaozhiUi {
public:
    using ActionCallback = void (*)(void *context);

    XiaozhiUi() = default;
    ~XiaozhiUi();

    XiaozhiUi(const XiaozhiUi &) = delete;
    XiaozhiUi &operator=(const XiaozhiUi &) = delete;

    bool preload();
    bool create(
        lv_obj_t *parent,
        ActionCallback action_callback,
        void *action_context,
        bool multiline_subtitle = false
    );
    void destroy();

    void setNetworkReady(bool ready);
    void setStatus(const char *status);
    void setChatMessage(const char *role, const char *text);
    void clearChatMessages();
    void setEmotion(const char *emotion);
    void setActivation(const char *code, const char *message, bool visible);
    void setActivity(XiaozhiCatActivity activity);
    void setPaused(bool paused);

    lv_obj_t *root() const
    {
        return _root;
    }

private:
    bool loadTextFont();
    void destroyView();
    void releaseTextFont();
    void showSubtitlePage();
    static void onSubtitleTimer(lv_timer_t *timer);
    static void onSubtitleTap(lv_event_t *event);
    static void onRootDelete(lv_event_t *event);

    lv_obj_t *_root = nullptr;
    lv_obj_t *_top_bar = nullptr;
    lv_obj_t *_network_label = nullptr;
    lv_obj_t *_status_label = nullptr;
    lv_obj_t *_activation_box = nullptr;
    lv_obj_t *_activation_code_label = nullptr;
    lv_obj_t *_activation_message_label = nullptr;
    lv_obj_t *_subtitle_bar = nullptr;
    lv_obj_t *_subtitle_label = nullptr;
    lv_obj_t *_page_indicator = nullptr;
    lv_timer_t *_subtitle_timer = nullptr;

    XiaozhiCatAvatar _avatar;
    XiaozhiSubtitlePager _pager;

    uint8_t *_font_data = nullptr;
    lv_font_t *_cbin_font = nullptr;
    const lv_font_t *_text_font = nullptr;

    bool _network_state_valid = false;
    bool _network_ready = false;
    bool _activation_state_valid = false;
    bool _activation_visible = false;
    bool _paused = false;
    XiaozhiCatActivity _activity = XiaozhiCatActivity::Idle;
};

} // namespace esp_brookesia::apps
