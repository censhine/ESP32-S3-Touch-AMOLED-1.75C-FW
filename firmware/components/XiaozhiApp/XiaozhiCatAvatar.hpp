#pragma once

#include "lvgl.h"

namespace esp_brookesia::apps {

enum class XiaozhiCatActivity { Idle, Listening, Thinking, Speaking, Sleeping, Error };

// All methods run under the caller's LVGL lock.
class XiaozhiCatAvatar {
public:
    XiaozhiCatAvatar() = default;
    ~XiaozhiCatAvatar();

    XiaozhiCatAvatar(const XiaozhiCatAvatar &) = delete;
    XiaozhiCatAvatar &operator=(const XiaozhiCatAvatar &) = delete;

    bool create(lv_obj_t *parent); // Fixed 280 x 224; caller positions root.
    void destroy();
    void setActivity(XiaozhiCatActivity activity);
    void setEmotion(const char *name);
    void setPaused(bool paused);
    lv_obj_t *root() const { return root_; }

private:
    enum class Emotion { Neutral, Happy, Curious, Sleepy };

    lv_obj_t *root_ = nullptr;
    lv_obj_t *face_ = nullptr;
    lv_obj_t *left_eye_ = nullptr;
    lv_obj_t *right_eye_ = nullptr;
    lv_obj_t *left_blush_ = nullptr;
    lv_obj_t *right_blush_ = nullptr;
    lv_obj_t *mouth_ = nullptr;
    lv_timer_t *timer_ = nullptr;
    XiaozhiCatActivity activity_ = XiaozhiCatActivity::Idle;
    Emotion emotion_ = Emotion::Neutral;
    unsigned ticks_ = 0;
    unsigned gaze_ = 0;
    bool paused_ = false;
    const lv_image_dsc_t *eye_source_ = nullptr;
    const lv_image_dsc_t *mouth_source_ = nullptr;
    int tilt_ = 1000;

    void refresh();
    void setTilt(int angle);
    static void timerCallback(lv_timer_t *timer);
    static void rootEvent(lv_event_t *event);
};

} // namespace esp_brookesia::apps
