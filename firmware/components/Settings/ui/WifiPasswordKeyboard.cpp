#include "WifiPasswordKeyboard.hpp"

#include <cstdio>
#include <cstring>

namespace esp_brookesia::apps::settings_ui {
namespace {

constexpr int DesignSize = 466;
constexpr uint32_t Background = 0x000000;
constexpr uint32_t Surface = 0x2C2C2E;
constexpr uint32_t Border = 0x48484C;
constexpr uint32_t Accent = 0x00BFFF;
constexpr uint32_t SecondaryText = 0xA7A7AD;
constexpr const char *Alphabet = "abcdefghijklmnopqrstuvwxyz";
constexpr const char *Digits = "1234567890.-_";
constexpr const char *Symbols = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
constexpr const char *PrintableAscii =
    " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";

bool is_hex_password(const char *text)
{
    for (; *text != '\0'; ++text) {
        if (!((*text >= '0' && *text <= '9') ||
              (*text >= 'a' && *text <= 'f') ||
              (*text >= 'A' && *text <= 'F'))) {
            return false;
        }
    }
    return true;
}

} // namespace

WifiPasswordKeyboard::WifiPasswordKeyboard(lv_obj_t *parent,
                                         void (*submit)(const char *, void *),
                                         void (*cancel)(void *), void *context)
    : submit_(submit), cancel_(cancel), context_(context)
{
    if (!parent) {
        return;
    }

    root_ = lv_obj_create(parent);
    lv_obj_remove_style_all(root_);
    lv_obj_set_size(root_, lv_pct(100), lv_pct(100));
    lv_obj_set_pos(root_, 0, 0);
    lv_obj_set_style_bg_color(root_, lv_color_hex(Background), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(root_, static_cast<lv_obj_flag_t>(
        LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
        LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_EVENT_BUBBLE));
    lv_obj_add_flag(root_, static_cast<lv_obj_flag_t>(
        LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK |
        LV_OBJ_FLAG_FLOATING | LV_OBJ_FLAG_HIDDEN));

    title_ = lv_label_create(root_);
    lv_obj_set_style_text_font(title_, &lv_font_montserrat_18, LV_PART_MAIN);
    lv_obj_set_style_text_color(title_, lv_color_hex(SecondaryText), LV_PART_MAIN);
    lv_obj_set_style_text_align(title_, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(title_, LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(title_, "");

    password_ = lv_textarea_create(root_);
    lv_textarea_set_one_line(password_, true);
    lv_textarea_set_max_length(password_, 64);
    lv_textarea_set_accepted_chars(password_, PrintableAscii);
    lv_textarea_set_password_show_time(password_, 0);
    lv_textarea_set_password_mode(password_, true);
    lv_textarea_set_placeholder_text(password_, "Password (8-64)");
    lv_textarea_set_text(password_, "");
    lv_obj_remove_flag(password_, static_cast<lv_obj_flag_t>(
        LV_OBJ_FLAG_GESTURE_BUBBLE | LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_SCROLL_ON_FOCUS));
    lv_obj_add_flag(password_, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_scrollbar_mode(password_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(password_, lv_color_hex(0x19191B), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(password_, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(password_, lv_color_hex(Border), LV_PART_MAIN);
    lv_obj_set_style_border_width(password_, 1, LV_PART_MAIN);
    lv_obj_set_style_outline_width(password_, 0, LV_PART_MAIN | LV_STATE_FOCUSED);
    lv_obj_set_style_radius(password_, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_hor(password_, 12, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(password_, 10, LV_PART_MAIN);
    lv_obj_set_style_text_font(password_, &lv_font_montserrat_22, LV_PART_MAIN);
    lv_obj_set_style_text_color(password_, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_color(password_, lv_color_hex(SecondaryText),
                                LV_PART_TEXTAREA_PLACEHOLDER);
    lv_obj_set_style_bg_color(password_, lv_color_hex(Accent), LV_PART_CURSOR);
    lv_obj_add_event_cb(password_, password_event, LV_EVENT_VALUE_CHANGED, this);

    visibility_button_ = create_button("Show", &lv_font_montserrat_18);
    alphabet_button_ = create_button("a/A", &lv_font_montserrat_22);
    digits_button_ = create_button("123", &lv_font_montserrat_22);
    symbols_button_ = create_button("#+=", &lv_font_montserrat_22);
    page_button_ = create_button("1/2 >", &lv_font_montserrat_18);
    for (unsigned i = 0; i < KeyCount; ++i) {
        keys_[i] = create_button("", i == CharacterSlots ?
                                &lv_font_montserrat_18 : &lv_font_montserrat_28);
    }
    set_button_text(keys_[CharacterSlots], "Space");
    set_button_text(keys_[CharacterSlots + 1], LV_SYMBOL_BACKSPACE);
    cancel_button_ = create_button("Cancel", &lv_font_montserrat_18);
    submit_button_ = create_button("Connect", &lv_font_montserrat_18);
    lv_obj_set_style_bg_color(submit_button_, lv_color_hex(Accent), LV_PART_MAIN);
    lv_obj_set_style_text_color(submit_button_, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_color(submit_button_, lv_color_hex(Accent), LV_PART_MAIN);

    lv_obj_add_event_cb(root_, root_event, LV_EVENT_ALL, this);
    lv_obj_update_layout(parent);
    layout();
    reset();
}

WifiPasswordKeyboard::~WifiPasswordKeyboard()
{
    if (root_) {
        lv_obj_delete(root_);
    }
}

lv_obj_t *WifiPasswordKeyboard::create_button(const char *text, const lv_font_t *font)
{
    lv_obj_t *button = lv_button_create(root_);
    lv_obj_remove_style_all(button);
    lv_obj_remove_flag(button, static_cast<lv_obj_flag_t>(
        LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_GESTURE_BUBBLE |
        LV_OBJ_FLAG_EVENT_BUBBLE | LV_OBJ_FLAG_SCROLLABLE));
    // Keep a drag attached to the initial key; button_event cancels on exit.
    lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_set_ext_click_area(button, 0);
    lv_obj_set_style_radius(button, 10, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(Surface), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_hex(Border), LV_PART_MAIN);
    lv_obj_set_style_text_color(button, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(button, font, LV_PART_MAIN);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x166680), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(button, lv_color_hex(Accent), LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x10404F), LV_STATE_CHECKED);
    lv_obj_set_style_border_color(button, lv_color_hex(Accent), LV_STATE_CHECKED);
    lv_obj_set_style_text_color(button, lv_color_hex(Accent), LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(button, lv_color_hex(0x171719), LV_STATE_DISABLED);
    lv_obj_set_style_border_color(button, lv_color_hex(0x232327), LV_STATE_DISABLED);
    lv_obj_set_style_text_color(button, lv_color_hex(0x5A5A60), LV_STATE_DISABLED);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_center(label);
    lv_obj_add_event_cb(button, button_event, LV_EVENT_ALL, this);
    return button;
}

void WifiPasswordKeyboard::set_button_text(lv_obj_t *button, const char *text)
{
    lv_label_set_text(lv_obj_get_child(button, 0), text);
}

void WifiPasswordKeyboard::layout()
{
    if (!root_ || !submit_button_) {
        return;
    }
    const int width = lv_obj_get_width(root_);
    const int height = lv_obj_get_height(root_);
    if (width <= 0 || height <= 0) {
        return;
    }
    const int top_crop = width > height ? width - height : 0;
    const int vertical_offset = height > width ? (height - width) / 2 : -top_crop;
    const auto scale = [width](int value) { return (value * width + DesignSize / 2) / DesignSize; };
    const auto place = [&](lv_obj_t *object, int x, int y, int w, int h) {
        lv_obj_set_pos(object, scale(x), scale(y) + vertical_offset);
        lv_obj_set_size(object, scale(w), scale(h));
    };

    // Design coordinates are physical panel positions. A status bar removes
    // pixels from the top of the app, not from the bottom of the circular panel.
    place(title_, 75, 80, 316, 22);
    if (lv_obj_get_y(title_) < 0) {
        lv_obj_set_y(title_, 0);
    }
    place(password_, 61, 106, 274, 48);
    place(visibility_button_, 341, 106, 64, 48);
    place(alphabet_button_, 61, 160, 76, 44);
    place(digits_button_, 143, 160, 70, 44);
    place(symbols_button_, 219, 160, 70, 44);
    place(page_button_, 295, 160, 110, 44);
    for (unsigned i = 0; i < KeyCount; ++i) {
        place(keys_[i], 61 + static_cast<int>(i % 5) * 70,
              210 + static_cast<int>(i / 5) * 56, 64, 50);
    }
    place(cancel_button_, 124, 382, 104, 44);
    place(submit_button_, 238, 382, 104, 44);
}

void WifiPasswordKeyboard::reset()
{
    pressed_button_ = nullptr;
    press_cancelled_ = false;
    showing_guidance_ = false;
    mode_ = Mode::Alphabet;
    page_ = 0;
    alphabet_page_ = 0;
    uppercase_ = false;
    masked_ = true;
    lv_textarea_set_text(password_, "");
    lv_textarea_set_password_mode(password_, true);
    lv_textarea_set_password_show_time(password_, 0);
    set_button_text(visibility_button_, "Show");
    lv_label_set_text(title_, ssid_);
    lv_obj_set_style_text_color(title_, lv_color_hex(SecondaryText), LV_PART_MAIN);
    refresh_keys();
}

void WifiPasswordKeyboard::show(const char *ssid)
{
    if (!root_) {
        return;
    }
    std::snprintf(ssid_, sizeof(ssid_), "%s", ssid ? ssid : "Wi-Fi password");
    reset();
    lv_obj_remove_flag(root_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(root_, -1);
    lv_obj_update_layout(root_);
    layout();
}

void WifiPasswordKeyboard::hide()
{
    if (!root_) {
        return;
    }
    lv_obj_add_flag(root_, LV_OBJ_FLAG_HIDDEN);
    ssid_[0] = '\0';
    reset();
}

bool WifiPasswordKeyboard::visible() const
{
    return root_ && !lv_obj_has_flag(root_, LV_OBJ_FLAG_HIDDEN);
}

unsigned WifiPasswordKeyboard::page_count() const
{
    if (mode_ == Mode::Alphabet) {
        return 2;
    }
    if (mode_ == Mode::Symbols) {
        return (std::strlen(Symbols) + CharacterSlots - 1) / CharacterSlots;
    }
    return 1;
}

void WifiPasswordKeyboard::refresh_keys()
{
    const char *source = mode_ == Mode::Alphabet ? Alphabet :
                         mode_ == Mode::Digits ? Digits : Symbols;
    const unsigned length = std::strlen(source);
    for (unsigned i = 0; i < CharacterSlots; ++i) {
        const unsigned index = page_ * CharacterSlots + i;
        char character = index < length ? source[index] : '\0';
        if (mode_ == Mode::Alphabet && uppercase_ && character != '\0') {
            character = static_cast<char>(character - 'a' + 'A');
        }
        characters_[i] = character;
        const char label[] = {character, '\0'};
        set_button_text(keys_[i], label);
        if (character) {
            lv_obj_remove_state(keys_[i], LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(keys_[i], LV_STATE_DISABLED);
        }
    }
    const auto select = [](lv_obj_t *button, bool selected) {
        if (selected) {
            lv_obj_add_state(button, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(button, LV_STATE_CHECKED);
        }
    };
    select(alphabet_button_, mode_ == Mode::Alphabet);
    select(digits_button_, mode_ == Mode::Digits);
    select(symbols_button_, mode_ == Mode::Symbols);
    char page_label[16];
    std::snprintf(page_label, sizeof(page_label), "%u/%u >", page_ + 1, page_count());
    set_button_text(page_button_, page_label);
    if (page_count() == 1) {
        lv_obj_add_state(page_button_, LV_STATE_DISABLED);
    } else {
        lv_obj_remove_state(page_button_, LV_STATE_DISABLED);
    }
}

void WifiPasswordKeyboard::set_guidance(const char *message)
{
    showing_guidance_ = true;
    lv_label_set_text(title_, message);
    lv_obj_set_style_text_color(title_, lv_color_hex(0xFFC166), LV_PART_MAIN);
}

void WifiPasswordKeyboard::clear_guidance()
{
    if (showing_guidance_) {
        showing_guidance_ = false;
        lv_label_set_text(title_, ssid_);
        lv_obj_set_style_text_color(title_, lv_color_hex(SecondaryText), LV_PART_MAIN);
    }
}

void WifiPasswordKeyboard::activate(lv_obj_t *button)
{
    if (button == cancel_button_) {
        // The callback can destroy this widget; do not access members after it.
        const auto callback = cancel_;
        void *context = context_;
        hide();
        if (callback) {
            callback(context);
        }
        return;
    }
    if (button == submit_button_) {
        const char *text = lv_textarea_get_text(password_);
        const auto length = std::strlen(text);
        if (length < 8) {
            set_guidance("At least 8 characters");
        } else if (length == 64 && !is_hex_password(text)) {
            set_guidance("64 characters must be hex");
        } else if (submit_) {
            // The owner copies the draft before hiding or destroying us.
            submit_(text, context_);
        }
        return;
    }
    if (button == visibility_button_) {
        masked_ = !masked_;
        lv_textarea_set_password_mode(password_, masked_);
        set_button_text(visibility_button_, masked_ ? "Show" : "Hide");
        return;
    }
    if (button == alphabet_button_) {
        if (mode_ == Mode::Alphabet) {
            uppercase_ = !uppercase_;
        } else {
            mode_ = Mode::Alphabet;
            page_ = alphabet_page_;
        }
        refresh_keys();
        return;
    }
    if (button == digits_button_ || button == symbols_button_) {
        if (mode_ == Mode::Alphabet) {
            alphabet_page_ = page_;
        }
        mode_ = button == digits_button_ ? Mode::Digits : Mode::Symbols;
        page_ = 0;
        refresh_keys();
        return;
    }
    if (button == page_button_) {
        page_ = (page_ + 1) % page_count();
        if (mode_ == Mode::Alphabet) {
            alphabet_page_ = page_;
        }
        refresh_keys();
        return;
    }
    if (button == keys_[CharacterSlots + 1]) {
        lv_textarea_delete_char(password_);
        return;
    }
    if (button == keys_[CharacterSlots]) {
        lv_textarea_add_char(password_, ' ');
        return;
    }
    for (unsigned i = 0; i < CharacterSlots; ++i) {
        if (button == keys_[i] && characters_[i]) {
            lv_textarea_add_char(password_, characters_[i]);
            return;
        }
    }
}

bool WifiPasswordKeyboard::pointer_inside(lv_obj_t *button, lv_indev_t *input)
{
    if (!input || lv_indev_get_type(input) != LV_INDEV_TYPE_POINTER) {
        return true;
    }
    lv_point_t point;
    lv_area_t area;
    lv_indev_get_point(input, &point);
    lv_obj_get_coords(button, &area);
    return point.x >= area.x1 && point.x <= area.x2 &&
           point.y >= area.y1 && point.y <= area.y2;
}

void WifiPasswordKeyboard::button_event(lv_event_t *event)
{
    auto *self = static_cast<WifiPasswordKeyboard *>(lv_event_get_user_data(event));
    if (!self->visible()) {
        return;
    }
    auto *button = static_cast<lv_obj_t *>(lv_event_get_target(event));
    const lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_PRESSED) {
        self->pressed_button_ = button;
        self->press_cancelled_ = false;
    } else if ((code == LV_EVENT_PRESSING || code == LV_EVENT_RELEASED) &&
               self->pressed_button_ == button) {
        if (!pointer_inside(button, lv_event_get_indev(event))) {
            self->press_cancelled_ = true;
            lv_obj_remove_state(button, LV_STATE_PRESSED);
        }
    } else if (code == LV_EVENT_PRESS_LOST) {
        self->press_cancelled_ = true;
        self->pressed_button_ = nullptr;
    } else if (code == LV_EVENT_CLICKED && self->pressed_button_ == button) {
        const bool accepted = !self->press_cancelled_;
        self->pressed_button_ = nullptr;
        if (accepted) {
            self->activate(button);
        }
    }
}

void WifiPasswordKeyboard::root_event(lv_event_t *event)
{
    auto *self = static_cast<WifiPasswordKeyboard *>(lv_event_get_user_data(event));
    if (lv_event_get_code(event) == LV_EVENT_DELETE) {
        // The parent can be deleted before this C++ owner is destroyed.
        self->root_ = nullptr;
        self->pressed_button_ = nullptr;
    } else if (lv_event_get_code(event) == LV_EVENT_SIZE_CHANGED) {
        self->layout();
    }
}

void WifiPasswordKeyboard::password_event(lv_event_t *event)
{
    auto *self = static_cast<WifiPasswordKeyboard *>(lv_event_get_user_data(event));
    if (self->root_) {
        self->clear_guidance();
    }
}

} // namespace esp_brookesia::apps::settings_ui
