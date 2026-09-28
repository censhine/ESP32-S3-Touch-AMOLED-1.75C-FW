#pragma once

#include "lvgl.h"

namespace esp_brookesia::apps::settings_ui {

// All methods, including destruction, must run on the LVGL thread.
class WifiPasswordKeyboard {
public:
    WifiPasswordKeyboard(lv_obj_t *parent, void (*submit)(const char *, void *),
                         void (*cancel)(void *), void *context);
    ~WifiPasswordKeyboard();

    WifiPasswordKeyboard(const WifiPasswordKeyboard &) = delete;
    WifiPasswordKeyboard &operator=(const WifiPasswordKeyboard &) = delete;

    void show(const char *ssid);
    void hide();
    bool visible() const;

private:
    enum class Mode { Alphabet, Digits, Symbols };
    static constexpr unsigned CharacterSlots = 13;
    static constexpr unsigned KeyCount = 15;

    lv_obj_t *root_ = nullptr;
    lv_obj_t *title_ = nullptr;
    lv_obj_t *password_ = nullptr;
    lv_obj_t *visibility_button_ = nullptr;
    lv_obj_t *alphabet_button_ = nullptr;
    lv_obj_t *digits_button_ = nullptr;
    lv_obj_t *symbols_button_ = nullptr;
    lv_obj_t *page_button_ = nullptr;
    lv_obj_t *keys_[KeyCount] = {};
    lv_obj_t *cancel_button_ = nullptr;
    lv_obj_t *submit_button_ = nullptr;
    lv_obj_t *pressed_button_ = nullptr;
    void (*submit_)(const char *, void *) = nullptr;
    void (*cancel_)(void *) = nullptr;
    void *context_ = nullptr;
    Mode mode_ = Mode::Alphabet;
    unsigned page_ = 0;
    unsigned alphabet_page_ = 0;
    bool uppercase_ = false;
    bool masked_ = true;
    bool press_cancelled_ = false;
    bool showing_guidance_ = false;
    char ssid_[33] = {};
    char characters_[CharacterSlots] = {};

    lv_obj_t *create_button(const char *text, const lv_font_t *font);
    void layout();
    void reset();
    void refresh_keys();
    void activate(lv_obj_t *button);
    void set_guidance(const char *message);
    void clear_guidance();
    unsigned page_count() const;
    static void set_button_text(lv_obj_t *button, const char *text);
    static bool pointer_inside(lv_obj_t *button, lv_indev_t *input);
    static void button_event(lv_event_t *event);
    static void root_event(lv_event_t *event);
    static void password_event(lv_event_t *event);
};

} // namespace esp_brookesia::apps::settings_ui
