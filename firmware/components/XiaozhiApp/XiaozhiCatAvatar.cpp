#include "XiaozhiCatAvatar.hpp"

#include "assets/cat_avatar_assets.h"

#include <cstring>
#include <initializer_list>

namespace esp_brookesia::apps {
namespace {

lv_obj_t *layer(lv_obj_t *parent, const lv_image_dsc_t *source, int x, int y)
{
    auto *image = lv_image_create(parent);
    lv_image_set_src(image, source);
    lv_obj_set_pos(image, x, y);
    lv_obj_remove_flag(image, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_image_set_pivot(image, 140 - x, 120 - y);
    return image;
}

} // namespace

XiaozhiCatAvatar::~XiaozhiCatAvatar()
{
    destroy();
}

bool XiaozhiCatAvatar::create(lv_obj_t *parent)
{
    if (!parent) return false;
    destroy();
    root_ = lv_obj_create(parent);
    if (!root_) return false;
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, 280, 224);
    lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_add_event_cb(root_, rootEvent, LV_EVENT_DELETE, this);

    face_ = layer(root_, &xiaozhi_cat_face, 10, 17);
    left_eye_ = layer(root_, &xiaozhi_cat_eye_center, 58, 82);
    right_eye_ = layer(root_, &xiaozhi_cat_eye_center, 150, 82);
    left_blush_ = layer(root_, &xiaozhi_cat_blush, 39, 145);
    right_blush_ = layer(root_, &xiaozhi_cat_blush, 207, 145);
    mouth_ = layer(root_, &xiaozhi_cat_mouth_closed, 118, 159);
    timer_ = lv_timer_create(timerCallback, 120, this);
    if (!timer_) {
        destroy();
        return false;
    }
    ticks_ = 0;
    gaze_ = 0;
    eye_source_ = nullptr;
    mouth_source_ = nullptr;
    tilt_ = 1000;
    refresh();
    if (paused_) lv_timer_pause(timer_);
    return true;
}

void XiaozhiCatAvatar::destroy()
{
    if (timer_) {
        lv_timer_delete(timer_);
        timer_ = nullptr;
    }
    if (root_) lv_obj_delete(root_);
    // rootEvent clears every pointer even if a parent deleted the root first.
}

void XiaozhiCatAvatar::setActivity(XiaozhiCatActivity activity)
{
    if (activity_ == activity) return;
    activity_ = activity;
    ticks_ = 0;
    gaze_ = 0;
    refresh();
}

void XiaozhiCatAvatar::setEmotion(const char *name)
{
    Emotion next = Emotion::Neutral;
    if (name) {
        if (!std::strcmp(name, "happy") || !std::strcmp(name, "smile") ||
            !std::strcmp(name, "laughing") || !std::strcmp(name, "excited")) {
            next = Emotion::Happy;
        } else if (!std::strcmp(name, "curious") || !std::strcmp(name, "surprised") ||
                   !std::strcmp(name, "thinking")) {
            next = Emotion::Curious;
        } else if (!std::strcmp(name, "sleepy") || !std::strcmp(name, "sleep") ||
                   !std::strcmp(name, "sad")) {
            next = Emotion::Sleepy;
        }
    }
    if (emotion_ == next) return;
    emotion_ = next;
    ticks_ = 0;
    refresh();
}

void XiaozhiCatAvatar::setPaused(bool paused)
{
    paused_ = paused;
    if (!timer_) return;
    if (paused) lv_timer_pause(timer_);
    else lv_timer_resume(timer_);
}

void XiaozhiCatAvatar::setTilt(int angle)
{
    if (tilt_ == angle) return;
    tilt_ = angle;
    for (auto *image : {face_, left_eye_, right_eye_, left_blush_, right_blush_, mouth_}) {
        if (image) lv_image_set_rotation(image, angle);
    }
}

void XiaozhiCatAvatar::refresh()
{
    if (!root_) return;
    const bool sleeping = activity_ == XiaozhiCatActivity::Sleeping || emotion_ == Emotion::Sleepy;
    const bool happy = emotion_ == Emotion::Happy && !sleeping;
    const bool blinking = !sleeping && !happy && ticks_ % 48 >= 46;
    const lv_image_dsc_t *eyes = &xiaozhi_cat_eye_center;
    if (sleeping || blinking) eyes = &xiaozhi_cat_eye_closed;
    else if (happy) eyes = &xiaozhi_cat_eye_happy;
    else if (activity_ == XiaozhiCatActivity::Thinking) eyes = &xiaozhi_cat_eye_up;
    else if (gaze_ == 1) eyes = &xiaozhi_cat_eye_left;
    else if (gaze_ == 2 || emotion_ == Emotion::Curious) eyes = &xiaozhi_cat_eye_right;
    if (eye_source_ != eyes) {
        eye_source_ = eyes;
        lv_image_set_src(left_eye_, eyes);
        lv_image_set_src(right_eye_, eyes);
    }

    const bool mouth_open = activity_ == XiaozhiCatActivity::Speaking ? (ticks_ / 3) % 2 == 0 : happy;
    const auto *mouth = mouth_open ? &xiaozhi_cat_mouth_open : &xiaozhi_cat_mouth_closed;
    if (mouth_source_ != mouth) {
        mouth_source_ = mouth;
        lv_image_set_src(mouth_, mouth);
    }
    int angle = 0;
    if (activity_ == XiaozhiCatActivity::Thinking) angle = -35;
    else if (emotion_ == Emotion::Curious) angle = 35;
    else if (happy) angle = -20;
    setTilt(angle);
}

void XiaozhiCatAvatar::timerCallback(lv_timer_t *timer)
{
    auto *self = static_cast<XiaozhiCatAvatar *>(lv_timer_get_user_data(timer));
    ++self->ticks_;
    if (self->ticks_ % 15 == 0) self->gaze_ = (self->gaze_ + 1) % 3;
    // Only changes of mouth, gaze, or eyelids invalidate images.
    if ((self->activity_ == XiaozhiCatActivity::Speaking && self->ticks_ % 3 == 0) ||
        self->ticks_ % 15 == 0 || self->ticks_ % 48 == 46 || self->ticks_ % 48 == 0) {
        self->refresh();
    }
}

void XiaozhiCatAvatar::rootEvent(lv_event_t *event)
{
    auto *self = static_cast<XiaozhiCatAvatar *>(lv_event_get_user_data(event));
    if (self->timer_) {
        lv_timer_delete(self->timer_);
        self->timer_ = nullptr;
    }
    self->root_ = nullptr;
    self->face_ = nullptr;
    self->left_eye_ = nullptr;
    self->right_eye_ = nullptr;
    self->left_blush_ = nullptr;
    self->right_blush_ = nullptr;
    self->mouth_ = nullptr;
    self->eye_source_ = nullptr;
    self->mouth_source_ = nullptr;
    self->tilt_ = 1000;
}

} // namespace esp_brookesia::apps
