#include "WifiPasswordKeyboard.hpp"
#include "../platform/native/pointer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using esp_brookesia::apps::settings_ui::WifiPasswordKeyboard;
constexpr int kScreenSize = 466;
int checks = 0;
roundwing::PointerState input;
lv_indev_t* pointer = nullptr;
lv_display_t* display = nullptr;
std::vector<uint32_t> pixels(kScreenSize * kScreenSize);
std::filesystem::path artifacts = "artifacts";

void require(bool condition, const char* message) {
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

struct Callbacks {
    unsigned submits = 0;
    unsigned cancels = 0;
    std::string submitted;
    WifiPasswordKeyboard* hide_on_submit = nullptr;
};

void submit(const char* password, void* context) {
    auto& callbacks = *static_cast<Callbacks*>(context);
    ++callbacks.submits;
    callbacks.submitted = password;
    if (callbacks.hide_on_submit) callbacks.hide_on_submit->hide();
}

void cancel(void* context) {
    ++static_cast<Callbacks*>(context)->cancels;
}

void read_pointer(lv_indev_t*, lv_indev_data_t* data) {
    data->point = {input.x, input.y};
    data->state = input.accepted ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

void press(int x, int y, bool down, uint32_t milliseconds = 35) {
    input.update(x, y, down);
    lv_tick_inc(milliseconds);
    lv_obj_update_layout(lv_screen_active());
    lv_indev_read(pointer);
}

lv_obj_t* find_label(lv_obj_t* parent, const std::string& text) {
    if (!lv_obj_is_visible(parent)) return nullptr;
    if (lv_obj_check_type(parent, &lv_label_class) && text == lv_label_get_text(parent)) return parent;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto* result = find_label(lv_obj_get_child(parent, i), text)) return result;
    return nullptr;
}

lv_obj_t* find_textarea(lv_obj_t* parent) {
    if (!lv_obj_is_visible(parent)) return nullptr;
    if (lv_obj_check_type(parent, &lv_textarea_class)) return parent;
    for (uint32_t i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto* result = find_textarea(lv_obj_get_child(parent, i))) return result;
    return nullptr;
}

lv_obj_t* field() {
    auto* textarea = find_textarea(lv_screen_active());
    require(textarea != nullptr, "visible password field exists");
    return textarea;
}

std::string password() {
    return lv_textarea_get_text(field());
}

lv_obj_t* button(const std::string& text) {
    auto* label = find_label(lv_screen_active(), text);
    require(label != nullptr, "requested button label exists");
    auto* object = lv_obj_get_parent(label);
    require(lv_obj_check_type(object, &lv_button_class), "requested label belongs to a button");
    return object;
}

lv_point_t center(lv_obj_t* object) {
    lv_obj_update_layout(object);
    lv_area_t area;
    lv_obj_get_coords(object, &area);
    return {(area.x1 + area.x2) / 2, (area.y1 + area.y2) / 2};
}

void click(const std::string& text) {
    const auto position = center(button(text));
    press(position.x, position.y, true);
    press(position.x, position.y, false);
}

void type(const std::string& characters) {
    for (char character : characters) click(std::string(1, character));
}

void next_page() {
    for (const auto* label : {"1/2 >", "2/2 >", "1/3 >", "2/3 >", "3/3 >", "1/1 >"}) {
        if (find_label(lv_screen_active(), label)) {
            click(label);
            return;
        }
    }
    require(false, "page navigation control exists");
}

void check_touch_bounds(lv_obj_t* object, unsigned& buttons) {
    if (!lv_obj_is_visible(object)) return;
    if (lv_obj_check_type(object, &lv_button_class)) {
        ++buttons;
        lv_area_t area;
        lv_obj_get_coords(object, &area);
        require(lv_obj_get_width(object) >= 44 && lv_obj_get_height(object) >= 44,
                "every button has a touch target at least 44 by 44 pixels");
        for (const int x : {area.x1, area.x2}) {
            for (const int y : {area.y1, area.y2}) {
                require((x - 233) * (x - 233) + (y - 233) * (y - 233) <= 223 * 223,
                        "all four button corners fit the circular safe area");
            }
        }
        auto* parent = lv_obj_get_parent(object);
        lv_area_t parent_area;
        lv_obj_get_coords(parent, &parent_area);
        require(area.x1 >= parent_area.x1 && area.y1 >= parent_area.y1 &&
                    area.x2 <= parent_area.x2 && area.y2 <= parent_area.y2,
                "button is fully inside its parent");
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        check_touch_bounds(lv_obj_get_child(object, i), buttons);
}

void check_layout() {
    lv_obj_update_layout(lv_screen_active());
    unsigned buttons = 0;
    check_touch_bounds(lv_screen_active(), buttons);
    require(buttons >= 11, "keyboard exposes character, mode, reveal and action buttons");
}

void screenshot(const std::string& name) {
    check_layout();
    lv_refr_now(display);
    std::filesystem::create_directories(artifacts);
    std::ofstream output(artifacts / ("wifi-keyboard-" + name + ".ppm"), std::ios::binary);
    output << "P6\n" << kScreenSize << ' ' << kScreenSize << "\n255\n";
    for (const uint32_t pixel : pixels) {
        const char rgb[] = {static_cast<char>((pixel >> 16) & 0xff),
                            static_cast<char>((pixel >> 8) & 0xff),
                            static_cast<char>(pixel & 0xff)};
        output.write(rgb, sizeof(rgb));
    }
    require(output.good(), "actual LVGL framebuffer screenshot was written");
}

lv_obj_t* make_parent(int top) {
    auto* parent = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(parent);
    lv_obj_set_size(parent, kScreenSize, kScreenSize - top);
    lv_obj_set_pos(parent, 0, top);
    lv_obj_remove_flag(parent, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE));
    lv_obj_update_layout(parent);
    return parent;
}

void check_masking() {
    auto* textarea = field();
    require(lv_textarea_get_password_mode(textarea), "password is masked by default");
    require(lv_textarea_get_password_show_time(textarea) == 0, "last typed character is never revealed automatically");
    auto* label = lv_textarea_get_label(textarea);
    require(password().empty() || password() != lv_label_get_text(label), "rendered password text is masked");
}

void run_interaction_scenario(int top) {
    const std::string suffix = top == 0 ? "full" : "page";
    auto* parent = make_parent(top);
    const auto children = lv_obj_get_child_count(parent);
    Callbacks callbacks;
    {
        WifiPasswordKeyboard keyboard(parent, submit, cancel, &callbacks);
        require(!keyboard.visible(), "new keyboard starts hidden");
        keyboard.show("Lab Network");
        require(keyboard.visible(), "show exposes the keyboard");
        require(find_label(lv_screen_active(), "Lab Network") != nullptr, "SSID is shown");
        require(password().empty(), "new entry starts empty");
        check_masking();
        screenshot(suffix + "-lower-1");

        type("ab");
        click("a/A");
        type("C");
        next_page();
        type("Z");
        click("123");
        type("7.-_");
        click("#+=");
        type("!");
        click("Space");
        require(password() == "abCZ7.-_! ", "mode, case and page switches preserve entered text");
        click(LV_SYMBOL_BACKSPACE);
        require(password() == "abCZ7.-_!", "backspace removes exactly one character");
        check_masking();
        click("Show");
        require(!lv_textarea_get_password_mode(field()), "Show reveals the password explicitly");
        require(password() == lv_label_get_text(lv_textarea_get_label(field())), "revealed display matches typed text");
        click("Hide");
        check_masking();

        keyboard.show("Lab Network");
        require(password().empty(), "show clears an earlier draft");
        type("abcdefghijklm");
        next_page();
        screenshot(suffix + "-lower-2");
        type("nopqrstuvwxyz");
        require(password() == "abcdefghijklmnopqrstuvwxyz", "both lowercase pages provide the full alphabet");
        click("Connect");
        require(callbacks.submits == 1 && callbacks.submitted == "abcdefghijklmnopqrstuvwxyz",
                "connect callback receives the complete typed password exactly once");

        keyboard.show("Lab Network");
        click("a/A");
        screenshot(suffix + "-upper-1");
        type("ABCDEFGHIJKLM");
        next_page();
        screenshot(suffix + "-upper-2");
        type("NOPQRSTUVWXYZ");
        require(password() == "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "both uppercase pages provide the full alphabet");

        keyboard.show("Lab Network");
        click("123");
        screenshot(suffix + "-numbers");
        type("1234567890.-_");
        require(password() == "1234567890.-_", "numeric mode provides ten digits and dot, dash, underscore");
        click("Space");
        click(LV_SYMBOL_BACKSPACE);
        require(password() == "1234567890.-_", "space and backspace work in numeric mode");

        keyboard.show("Lab Network");
        click("#+=");
        const std::string punctuation = "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
        for (std::size_t offset = 0; offset < punctuation.size(); offset += 13) {
            screenshot(suffix + "-symbols-" + std::to_string(offset / 13 + 1));
            type(punctuation.substr(offset, 13));
            click("Space");
            click(LV_SYMBOL_BACKSPACE);
            if (offset + 13 < punctuation.size()) next_page();
        }
        require(password() == punctuation, "symbol pages provide every ASCII punctuation character");
        next_page();
        require(find_label(lv_screen_active(), "!") != nullptr, "symbol pages wrap to the first page");

        keyboard.show("Lab Network");
        const auto before_invalid = callbacks.submits;
        click("Connect");
        require(keyboard.visible() && callbacks.submits == before_invalid, "empty password remains open without submission");
        type("abcdefg");
        click("Connect");
        require(keyboard.visible() && callbacks.submits == before_invalid, "seven-character password cannot submit");
        require(find_label(lv_screen_active(), "At least 8 characters") != nullptr, "short password has inline guidance");
        screenshot(suffix + "-short-validation");
        type("h");
        click("Connect");
        require(callbacks.submits == before_invalid + 1 && callbacks.submitted == "abcdefgh", "eight-character password can submit");

        keyboard.show("Lab Network");
        next_page();
        type(std::string(63, 'z'));
        click("Connect");
        require(callbacks.submits == before_invalid + 2 && callbacks.submitted == std::string(63, 'z'),
                "63-character nonhex passphrase reaches callback without truncation");

        keyboard.show("Lab Network");
        next_page();
        type(std::string(65, 'z'));
        require(password() == std::string(64, 'z'), "the 65th character is ignored");
        click("Connect");
        require(keyboard.visible() && callbacks.submits == before_invalid + 2, "64-character nonhex password remains open");
        require(find_label(lv_screen_active(), "64 characters must be hex") != nullptr, "invalid raw PSK has inline guidance");
        screenshot(suffix + "-hex-validation");
        click(LV_SYMBOL_BACKSPACE);
        require(password() == std::string(63, 'z'), "backspace works at the character limit");
        click("Connect");
        require(callbacks.submits == before_invalid + 3, "corrected 63-character password can submit");

        keyboard.show("Lab Network");
        type(std::string(64, 'a'));
        click("Connect");
        require(callbacks.submits == before_invalid + 4 && callbacks.submitted == std::string(64, 'a'),
                "64-character hexadecimal raw PSK reaches callback without truncation");

        keyboard.show("Lab Network");
        const auto position = center(button("a"));
        press(position.x, position.y, true);
        for (int repeat = 0; repeat < 12; ++repeat) press(position.x, position.y, true, 150);
        require(password().size() <= 1, "holding a key does not repeat input");
        press(position.x, position.y, false);
        require(password() == "a", "held key contributes exactly one character after release");
        click(LV_SYMBOL_BACKSPACE);
        press(position.x, position.y, true);
        press(233, 451, true);
        press(233, 451, false);
        require(password().empty(), "dragging out before release contributes no character");
        click(LV_SYMBOL_BACKSPACE);
        require(password().empty(), "backspace on an empty field is safe");

        type("abcdefgh");
        click("Show");
        click("Cancel");
        require(!keyboard.visible() && callbacks.cancels == 1, "cancel hides the keyboard and calls back once");
        keyboard.show("Another Network");
        require(password().empty(), "cancel then reopen clears the password");
        check_masking();
        require(find_label(lv_screen_active(), "Another Network") != nullptr, "reopen updates SSID");
        type("a");
        keyboard.hide();
        require(!keyboard.visible(), "explicit hide closes the keyboard");
        keyboard.show("Lab Network");
        require(password().empty(), "hide then reopen clears the password");
    }
    require(lv_obj_get_child_count(parent) == children, "destruction releases all keyboard LVGL objects");
    lv_obj_delete(parent);
}

void run_lifecycle_scenarios() {
    const auto children = lv_obj_get_child_count(lv_screen_active());
    Callbacks callbacks;
    for (int iteration = 0; iteration < 100; ++iteration) {
        auto* parent = make_parent(iteration % 2 == 0 ? 0 : 80);
        auto keyboard = std::make_unique<WifiPasswordKeyboard>(parent, submit, cancel, &callbacks);
        keyboard->show("Lifecycle Fixture");
        click("a");
        if (iteration % 2 == 0) {
            keyboard.reset();
            require(lv_obj_get_child_count(parent) == 0, "widget destruction leaves parent empty");
            lv_obj_delete(parent);
        } else {
            lv_obj_delete(parent);
            require(!keyboard->visible(), "parent deletion invalidates the keyboard safely");
            keyboard->hide();
            keyboard.reset();
        }
        require(lv_obj_get_child_count(lv_screen_active()) == children, "repeated create/show/destroy leaves no orphan objects");
    }
    require(callbacks.submits == 0 && callbacks.cancels == 0, "destruction does not synthesize submit or cancel callbacks");
}

void run_callback_hide_scenario() {
    auto* parent = make_parent(80);
    Callbacks callbacks;
    {
        WifiPasswordKeyboard keyboard(parent, submit, cancel, &callbacks);
        callbacks.hide_on_submit = &keyboard;
        keyboard.show("Callback Fixture");
        type("abcdefgh");
        click("Connect");
        require(callbacks.submits == 1 && callbacks.submitted == "abcdefgh" && !keyboard.visible(),
                "submit callback can copy the draft and synchronously hide the keyboard");
        keyboard.show("Callback Fixture");
        require(password().empty(), "callback-driven hide clears the draft before reopen");
        check_masking();
    }
    lv_obj_delete(parent);
}
} // namespace

int main(int argc, char** argv) {
    if (argc > 1) artifacts = argv[1];
    lv_init();
    display = lv_display_create(kScreenSize, kScreenSize);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display, pixels.data(), nullptr, pixels.size() * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, [](lv_display_t* current, const lv_area_t*, uint8_t*) { lv_display_flush_ready(current); });
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x000000), 0);
    pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_mode(pointer, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(pointer, read_pointer);
    run_interaction_scenario(0);
    run_interaction_scenario(80);
    run_lifecycle_scenarios();
    run_callback_hide_scenario();
    lv_indev_delete(pointer);
    lv_display_delete(display);
    lv_deinit();
    std::cout << "PASS: " << checks << " Wi-Fi keyboard pointer, validation, geometry and lifecycle checks\n";
}
