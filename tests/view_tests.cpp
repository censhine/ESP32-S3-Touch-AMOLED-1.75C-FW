#include "game_view.hpp"
#include "../platform/native/pointer.hpp"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
int checks = 0;
void require(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
roundwing::PointerState input;
void read_pointer(lv_indev_t*, lv_indev_data_t* data) {
    data->point = {input.x, input.y};
    data->state = input.accepted ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}
int exit_calls = 0, saves = 0, progress = 0;
void exit_app(void*) { ++exit_calls; }
void save(int value, void*) { ++saves; progress = value; }

lv_obj_t* find_label(lv_obj_t* parent, const std::string& text) {
    if (lv_obj_has_flag(parent, LV_OBJ_FLAG_HIDDEN)) return nullptr;
    if (lv_obj_check_type(parent, &lv_label_class) && text == lv_label_get_text(parent)) return parent;
    for (unsigned i = 0; i < lv_obj_get_child_count(parent); ++i)
        if (auto* result = find_label(lv_obj_get_child(parent, i), text)) return result;
    return nullptr;
}
void press(lv_indev_t* pointer, int x, int y, bool down) {
    input.update(x, y, down);
    lv_tick_inc(35);
    lv_obj_update_layout(lv_screen_active());
    lv_indev_read(pointer);
}
void click(lv_indev_t* pointer, const std::string& text) {
    auto* label = find_label(lv_screen_active(), text);
    require(label != nullptr, ("visible button: " + text).c_str());
    lv_area_t area;
    lv_obj_update_layout(label);
    lv_obj_get_coords(lv_obj_get_parent(label), &area);
    const int x = (area.x1 + area.x2) / 2, y = (area.y1 + area.y2) / 2;
    press(pointer, x, y, true);
    press(pointer, x, y, false);
}
void fail(roundwing::GameView& view) {
    for (int i = 0; i < 400 && view.game().phase() == roundwing::Phase::Playing; ++i) view.update(1.0f / 120);
    require(view.game().phase() == roundwing::Phase::Failed, "no flap eventually fails");
}
void replay(roundwing::GameView& view, const std::string& path) {
    std::ifstream file(path);
    require(file.good(), "core-generated replay exists");
    std::set<int> taps;
    int frame;
    while (file >> frame) taps.insert(frame);
    for (int i = 0; i < 20000; ++i) {
        if (taps.count(i)) view.tap();
        view.update(1.0f / 120);
        if (view.game().phase() != roundwing::Phase::Ready && view.game().phase() != roundwing::Phase::Playing) break;
    }
}
void check_touch_bounds(lv_obj_t* object) {
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) return;
    if (lv_obj_has_flag(object, LV_OBJ_FLAG_CLICKABLE) && lv_obj_get_width(object) < 466) {
        lv_area_t area;
        lv_obj_get_coords(object, &area);
        require(lv_obj_get_width(object) >= 64 && lv_obj_get_height(object) >= 64, "touch target at least 64px");
        for (int x : {area.x1, area.x2}) for (int y : {area.y1, area.y2})
            require((x - 233) * (x - 233) + (y - 233) * (y - 233) <= 223 * 223, "touch target inside circular safe area");
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(object); ++i) check_touch_bounds(lv_obj_get_child(object, i));
}
} // namespace

int main() {
    using roundwing::Phase;
    lv_init();
    auto* display = lv_display_create(466, 466);
    std::vector<uint32_t> buffer(466 * 466);
    lv_display_set_buffers(display, buffer.data(), nullptr, buffer.size() * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(display, [](lv_display_t* d, const lv_area_t*, uint8_t*) { lv_display_flush_ready(d); });
    auto* pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(pointer, read_pointer);
    const auto children = lv_obj_get_child_count(lv_screen_active());
    {
        roundwing::GameView view(lv_screen_active(), {exit_app, save, nullptr});
        lv_obj_update_layout(lv_screen_active());
        check_touch_bounds(lv_screen_active());
        click(pointer, "2");
        require(view.game().level_index() == 0, "locked stage cannot be selected");
        click(pointer, "开始");
        require(view.game().phase() == Phase::Playing, "pointer starts game");
        view.update(0.1f);
        const float velocity = view.game().velocity_y(), y = view.game().bird_y();
        press(pointer, 320, 84, true);
        press(pointer, 320, 84, false);
        require(view.game().phase() == Phase::Paused, "pause button pauses");
        require(view.game().velocity_y() == velocity && view.game().bird_y() == y, "pause press does not flap");
        check_touch_bounds(lv_screen_active());
        view.update(0.1f);
        require(view.game().bird_y() == y, "paused view does not move");
        click(pointer, "继续");
        require(view.game().phase() == Phase::Countdown, "resume uses countdown");
        view.update(0.1f);
        require(view.game().bird_y() == y, "countdown freezes physics");
        for (int i = 0; i < 10; ++i) view.update(0.08f);
        require(view.game().phase() == Phase::Playing, "countdown resumes play");
        press(pointer, 233, 233, true);
        view.update(0.05f);
        const float after_press = view.game().velocity_y();
        press(pointer, 233, 233, true);
        view.update(0.05f);
        require(view.game().velocity_y() > after_press, "holding does not flap repeatedly");
        const float before_drag = view.game().velocity_y();
        press(pointer, 0, 0, true);
        press(pointer, 233, 233, true);
        require(view.game().velocity_y() == before_drag, "dragging outside and back does not flap");
        press(pointer, 233, 233, false);
        press(pointer, 0, 0, true);
        press(pointer, 233, 233, true);
        require(view.game().velocity_y() == before_drag, "press originating outside stays rejected");
        press(pointer, 233, 233, false);
        for (int retry = 0; retry < 3; ++retry) {
            fail(view);
            require(find_label(lv_screen_active(), "飞行结束"), "failure title shown on every retry");
            click(pointer, "再试");
            require(view.game().phase() == Phase::Ready, "retry is ready until deliberate start");
            click(pointer, "开始");
        }
        view.suspend();
        click(pointer, "桌面");
        require(exit_calls == 1, "exit callback exactly once");
    }
    require(lv_obj_get_child_count(lv_screen_active()) == children, "exit releases all view objects");
    {
        roundwing::GameView view(lv_screen_active(), {exit_app, save, nullptr});
        replay(view, "artifacts/replay-level-1.txt");
        require(view.game().phase() == Phase::Cleared, "shared view completes real first-stage replay");
        require(saves == 1 && progress == 2, "unlock persists once");
        for (int i = 0; i < 10; ++i) view.update(0.1f);
        require(saves == 1, "victory frames do not repeat flash writes");
        click(pointer, "下一关");
        require(view.game().level_index() == 1 && view.game().phase() == Phase::Ready, "next button advances one stage");
        require(find_label(lv_screen_active(), "第2关  0/8"), "level HUD updates");
        lv_obj_update_layout(lv_screen_active());
        check_touch_bounds(lv_screen_active());
    }
    for (int i = 0; i < 100; ++i) {
        roundwing::GameView view(lv_screen_active(), {}, 4);
        view.tap();
        view.update(0.03f);
        view.suspend();
    }
    require(lv_obj_get_child_count(lv_screen_active()) == children, "repeated open/close releases objects");
    lv_indev_delete(pointer);
    lv_display_delete(display);
    lv_deinit();
    std::cout << "PASS: " << checks << " LVGL interaction/lifecycle checks\n";
}
