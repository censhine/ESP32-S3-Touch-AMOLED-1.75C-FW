#include "XiaozhiCatAvatar.hpp"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {

using esp_brookesia::apps::XiaozhiCatActivity;
using esp_brookesia::apps::XiaozhiCatAvatar;
constexpr int kScreen = 466;
std::vector<uint32_t> pixels(kScreen * kScreen);
lv_display_t *display = nullptr;
int checks = 0;

void require(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void advance(unsigned milliseconds)
{
    while (milliseconds) {
        const unsigned step = milliseconds < 120 ? milliseconds : 120;
        lv_tick_inc(step);
        lv_timer_handler();
        milliseconds -= step;
    }
}

uint64_t checksum()
{
    lv_refr_now(display);
    uint64_t value = 1469598103934665603ULL;
    for (uint32_t pixel : pixels) value = (value ^ pixel) * 1099511628211ULL;
    return value;
}

uint64_t eye_checksum()
{
    uint64_t value = 1469598103934665603ULL;
    for (int y = 170; y < 240; ++y) {
        for (int x = 150; x < 316; ++x) {
            value = (value ^ pixels[y * kScreen + x]) * 1099511628211ULL;
        }
    }
    return value;
}

void screenshot(const std::filesystem::path &directory, const char *name)
{
    lv_refr_now(display);
    std::filesystem::create_directories(directory);
    std::ofstream output(directory / (std::string("xiaozhi-cat-") + name + ".ppm"), std::ios::binary);
    output << "P6\n" << kScreen << ' ' << kScreen << "\n255\n";
    for (uint32_t pixel : pixels) {
        const char rgb[] = {static_cast<char>((pixel >> 16) & 255),
                            static_cast<char>((pixel >> 8) & 255),
                            static_cast<char>(pixel & 255)};
        output.write(rgb, sizeof(rgb));
    }
    require(output.good(), "LVGL framebuffer preview is saved");
}

void assert_artwork()
{
    unsigned amber = 0, green = 0, cream = 0, ink = 0;
    for (int y = 95; y < 305; ++y) {
        for (int x = 103; x < 363; ++x) {
            const uint32_t pixel = pixels[y * kScreen + x];
            const int r = (pixel >> 16) & 255;
            const int g = (pixel >> 8) & 255;
            const int b = pixel & 255;
            if (r > 170 && g > 95 && g < 225 && b < 165) ++amber;
            if (g > r + 22 && g > b - 10 && g > 65 && g < 190) ++green;
            if (r > 240 && g > 235 && b > 215) ++cream;
            if (r < 125 && g < 110 && b < 100) ++ink;
        }
    }
    require(amber > 8000, "ginger face raster is visible");
    require(green > 700, "both mint-green eye rasters are visible");
    require(cream > 1000, "cream muzzle and eye whites are visible");
    require(ink > 400, "face outline and features are visible");
}

void run(const std::filesystem::path &directory)
{
    auto *parent = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(parent);
    lv_obj_set_size(parent, kScreen, kScreen);
    const uint32_t initial_children = lv_obj_get_child_count(parent);
    XiaozhiCatAvatar cat;
    require(cat.create(parent), "avatar creates under a valid parent");
    require(cat.root() != nullptr, "root is available");
    require(lv_obj_get_width(cat.root()) == 280 && lv_obj_get_height(cat.root()) == 224,
            "root keeps the fixed 280 x 224 contract");
    lv_obj_set_pos(cat.root(), 93, 85);
    const uint64_t idle = checksum();
    screenshot(directory, "idle");
    assert_artwork();

    cat.setActivity(XiaozhiCatActivity::Listening);
    advance(1800);
    require(checksum() != idle, "eye saccade changes actual LVGL rendering");
    screenshot(directory, "listening");
    const uint64_t attentive = checksum();
    advance(3720);
    const uint64_t blink = checksum();
    require(blink != attentive, "timed blink changes the visible eyelids");
    screenshot(directory, "blink");
    advance(240);
    require(checksum() != blink, "blink returns to an open-eye state");
    cat.setActivity(XiaozhiCatActivity::Thinking);
    const uint64_t thinking = checksum();
    require(thinking != idle, "thinking has an upward gaze and head tilt");
    screenshot(directory, "thinking");

    cat.setActivity(XiaozhiCatActivity::Speaking);
    const uint64_t speaking_open = checksum();
    advance(360);
    require(checksum() != speaking_open, "speaking changes the mouth raster");
    screenshot(directory, "speaking");
    cat.setEmotion("happy");
    const uint64_t happy = checksum();
    const uint64_t happy_eyes = eye_checksum();
    require(happy != speaking_open, "happy uses distinct smile eyes");
    screenshot(directory, "happy");
    advance(360);
    require(checksum() != happy, "happy speaking still animates the mouth");
    require(eye_checksum() == happy_eyes, "happy eyes stay unchanged while speaking mouth moves");
    cat.setActivity(XiaozhiCatActivity::Idle);
    const uint64_t happy_idle = checksum();
    advance(360);
    require(checksum() == happy_idle, "happy mouth stays open outside speaking");
    cat.setEmotion("curious");
    const uint64_t curious = checksum();
    require(curious != happy, "curious uses directed gaze");
    screenshot(directory, "curious");
    cat.setEmotion("unsupported-emotion");
    require(checksum() != curious, "unsupported emotion falls back to neutral");

    cat.setActivity(XiaozhiCatActivity::Sleeping);
    const uint64_t sleeping = checksum();
    require(sleeping != curious, "sleeping closes the eyes");
    screenshot(directory, "sleeping");
    cat.setActivity(XiaozhiCatActivity::Error);
    require(checksum() != sleeping, "error wakes the face");
    cat.setActivity(XiaozhiCatActivity::Speaking);
    cat.setPaused(true);
    const uint64_t paused = checksum();
    advance(5000);
    require(checksum() == paused, "paused timer leaves visible image unchanged");
    cat.setPaused(false);
    advance(360);
    require(checksum() != paused, "resuming restarts mouth movement");

    cat.destroy();
    require(cat.root() == nullptr && lv_obj_get_child_count(parent) == initial_children,
            "destroy removes all avatar objects");
    cat.destroy();
    require(cat.create(parent), "avatar can be created again");
    lv_obj_delete(parent);
    require(cat.root() == nullptr, "parent deletion clears the avatar root pointer");
    cat.setPaused(true);
    cat.setActivity(XiaozhiCatActivity::Thinking);
    cat.destroy();

    for (int i = 0; i < 20; ++i) {
        auto *owner = lv_obj_create(lv_screen_active());
        XiaozhiCatAvatar repeated;
        require(repeated.create(owner), "repeated avatar creation succeeds");
        if (i % 2) repeated.destroy();
        lv_obj_delete(owner);
        require(repeated.root() == nullptr, "both deletion orders leave no dangling root");
    }
}

} // namespace

int main(int argc, char **argv)
{
    const std::filesystem::path directory = argc > 1 ? argv[1] : "artifacts";
    lv_init();
    display = lv_display_create(kScreen, kScreen);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    lv_display_set_buffers(display, pixels.data(), nullptr, pixels.size() * sizeof(uint32_t),
                           LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, [](lv_display_t *current, const lv_area_t *, uint8_t *) {
        lv_display_flush_ready(current);
    });
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0xFFF9ED), 0);
    run(directory);
    lv_display_delete(display);
    lv_deinit();
    std::cout << "PASS: " << checks << " avatar artwork, animation and lifetime checks\n";
}
