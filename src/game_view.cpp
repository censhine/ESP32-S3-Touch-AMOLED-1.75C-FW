#include "game_view.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "fonts.h"

namespace roundwing {
namespace {

constexpr int kSize = 466;
lv_color_t color(unsigned hex) { return lv_color_hex(hex); }

lv_obj_t* block(lv_obj_t* parent, int x, int y, int width, int height,
                unsigned fill, int radius = 0) {
  auto* object = lv_obj_create(parent);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, width, height);
  lv_obj_set_style_radius(object, radius, 0);
  lv_obj_set_style_bg_color(object, color(fill), 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(object, 0, 0);
  lv_obj_set_style_pad_all(object, 0, 0);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(object, LV_OBJ_FLAG_CLICKABLE);
  return object;
}

lv_obj_t* label(lv_obj_t* parent, int x, int y, int width, int height,
                const char* text, const lv_font_t* font, unsigned ink) {
  auto* object = lv_label_create(parent);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, width, height);
  lv_obj_set_style_text_font(object, font, 0);
  lv_obj_set_style_text_color(object, color(ink), 0);
  lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
  lv_label_set_long_mode(object, LV_LABEL_LONG_CLIP);
  lv_label_set_text(object, text);
  return object;
}

lv_obj_t* button(lv_obj_t* parent, int x, int y, int width, int height,
                 unsigned fill, int radius) {
  auto* object = block(parent, x, y, width, height, fill, radius);
  lv_obj_add_flag(object, LV_OBJ_FLAG_CLICKABLE);
  return object;
}

void visible(lv_obj_t* object, bool show) {
  if (show && lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_remove_flag(object, LV_OBJ_FLAG_HIDDEN);
  } else if (!show && !lv_obj_has_flag(object, LV_OBJ_FLAG_HIDDEN)) {
    lv_obj_add_flag(object, LV_OBJ_FLAG_HIDDEN);
  }
}

}  // namespace

GameView::GameView(lv_obj_t* parent, ViewCallbacks callbacks, int unlocked)
    : callbacks_(callbacks) {
  game_.set_unlocked_levels(unlocked);
  saved_unlocked_ = game_.unlocked_levels();

  root_ = block(parent, 0, 0, kSize, kSize, 0x101d30, LV_RADIUS_CIRCLE);
  lv_obj_center(root_);
  lv_obj_add_flag(root_, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_clip_corner(root_, true, 0);
  lv_obj_add_event_cb(root_, on_root_pressed, LV_EVENT_PRESSED, this);

  bounds_[0] = block(root_, 59, 125, 348, 2, 0x365467, 1);
  bounds_[1] = block(root_, 59, 353, 348, 2, 0x365467, 1);
  bounds_[2] = block(root_, 59, 125, 2, 230, 0x273c50, 1);
  bounds_[3] = block(root_, 405, 125, 2, 230, 0x273c50, 1);

  world_ = block(root_, 60, 126, 346, 228, 0x101d30);
  lv_obj_set_style_bg_opa(world_, LV_OPA_TRANSP, 0);
  lv_obj_remove_flag(world_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  for (auto& gate : gates_) {
    gate.upper = block(world_, 346, 0, 38, 38, 0x38c9b7, 7);
    gate.lower = block(world_, 346, 190, 38, 38, 0x38c9b7, 7);
    gate.upper_shine = block(world_, 353, 0, 3, 38, 0x86e6d7, 2);
    gate.lower_shine = block(world_, 353, 190, 3, 38, 0x86e6d7, 2);
    visible(gate.upper, false);
    visible(gate.lower, false);
    visible(gate.upper_shine, false);
    visible(gate.lower_shine, false);
  }

  bird_ = block(world_, 76, 96, 27, 22, 0xffdb58, 11);
  lv_obj_add_flag(bird_, LV_OBJ_FLAG_OVERFLOW_VISIBLE);
  block(bird_, 17, 4, 8, 8, 0xffffff, 4);
  block(bird_, 21, 6, 3, 3, 0x172534, 2);
  block(bird_, 23, 13, 9, 6, 0xf38b45, 3);
  wing_ = block(bird_, 5, 11, 13, 9, 0xeeb73b, 5);

  title_ = label(root_, 100, 59, 180, 40, "圆翼闯关", &roundwing_font_30, 0xf5f5e6);
  progress_ = label(root_, 99, 98, 182, 29, "", &roundwing_font_22, 0x8de1d2);
  pause_button_ = button(root_, 284, 48, 72, 72, 0x253d50, 36);
  block(pause_button_, 25, 22, 7, 28, 0xf5f5e6, 2);
  block(pause_button_, 40, 22, 7, 28, 0xf5f5e6, 2);
  lv_obj_add_event_cb(pause_button_, on_pause_clicked, LV_EVENT_CLICKED, this);

  overlay_ = block(root_, 0, 0, kSize, kSize, 0x0d1727, LV_RADIUS_CIRCLE);
  lv_obj_set_style_bg_opa(overlay_, LV_OPA_70, 0);
  modal_ = block(root_, 72, 128, 322, 160, 0x203649, 8);
  lv_obj_set_style_border_width(modal_, 2, 0);
  lv_obj_set_style_border_color(modal_, color(0x50717b), 0);
  modal_title_ = label(modal_, 19, 8, 284, 40, "", &roundwing_font_30, 0xffde6f);
  modal_detail_ = label(modal_, 20, 49, 282, 30, "", &roundwing_font_22, 0xe2efeb);

  for (int i = 0; i < 3; ++i) {
    action_buttons_[i] = button(modal_, 9 + 104 * i, 82, 96, 72,
                                i == 0 ? 0x36bba9 : 0x355368, 7);
    action_labels_[i] = label(action_buttons_[i], 0, 22, 96, 29, "",
                              &roundwing_font_22, 0xffffff);
    lv_obj_add_event_cb(action_buttons_[i], on_action_clicked, LV_EVENT_CLICKED, this);
  }

  stage_title_ = label(root_, 143, 291, 180, 29, "选择关卡", &roundwing_font_22, 0xc9e0df);
  for (int i = 0; i < Game::kLevels; ++i) {
    stage_buttons_[i] = button(root_, 89 + 74 * i, 328, 64, 64, 0x365368, 7);
    char number[8];
    std::snprintf(number, sizeof(number), "%d", i + 1);
    label(stage_buttons_[i], 0, 18, 64, 29, number, &roundwing_font_22, 0xffffff);
    lv_obj_add_event_cb(stage_buttons_[i], on_stage_clicked, LV_EVENT_CLICKED, this);
  }
  countdown_ = label(root_, 153, 210, 160, 48, "", &roundwing_font_30, 0xffde6f);
  shown_phase_ = Phase::Countdown;
  render();
}

GameView::~GameView() {
  if (root_) lv_obj_delete(root_);
}

void GameView::update(float elapsed_seconds) {
  game_.advance(elapsed_seconds);
  if (game_.unlocked_levels() != saved_unlocked_) {
    saved_unlocked_ = game_.unlocked_levels();
    if (callbacks_.save_progress) callbacks_.save_progress(saved_unlocked_, callbacks_.context);
  }
  render();
}

void GameView::suspend() {
  game_.pause();
  render();
}

void GameView::tap() {
  if (game_.phase() == Phase::Ready || game_.phase() == Phase::Playing) {
    game_.flap();
    render();
  }
}

void GameView::on_root_pressed(lv_event_t* event) {
  static_cast<GameView*>(lv_event_get_user_data(event))->tap();
}

void GameView::on_pause_clicked(lv_event_t* event) {
  auto* self = static_cast<GameView*>(lv_event_get_user_data(event));
  self->suspend();
}

void GameView::on_action_clicked(lv_event_t* event) {
  auto* self = static_cast<GameView*>(lv_event_get_user_data(event));
  auto* target = lv_event_get_target_obj(event);
  for (int i = 0; i < 3; ++i) {
    if (target == self->action_buttons_[i]) {
      self->action(i);
      return;
    }
  }
}

void GameView::on_stage_clicked(lv_event_t* event) {
  auto* self = static_cast<GameView*>(lv_event_get_user_data(event));
  if (self->game_.phase() != Phase::Ready) return;
  auto* target = lv_event_get_target_obj(event);
  for (int i = 0; i < Game::kLevels; ++i) {
    if (target == self->stage_buttons_[i] && i < self->game_.unlocked_levels()) {
      self->game_.select_level(i);
      self->render();
      return;
    }
  }
}

void GameView::action(int index) {
  switch (game_.phase()) {
    case Phase::Ready:
      if (index == 0) game_.flap();
      if (index == 2 && callbacks_.exit) {
        callbacks_.exit(callbacks_.context);
        return;
      }
      break;
    case Phase::Paused:
      if (index == 0) game_.resume();
      if (index == 1) game_.restart();
      if (index == 2 && callbacks_.exit) {
        callbacks_.exit(callbacks_.context);
        return;
      }
      break;
    case Phase::Failed:
      if (index == 0) game_.restart();
      if (index == 2 && callbacks_.exit) {
        callbacks_.exit(callbacks_.context);
        return;
      }
      break;
    case Phase::Cleared:
      if (index == 0) game_.next_level();
      if (index == 1) game_.restart();
      if (index == 2 && callbacks_.exit) {
        callbacks_.exit(callbacks_.context);
        return;
      }
      break;
    case Phase::Complete:
      if (index == 0) game_.restart();
      if (index == 2 && callbacks_.exit) {
        callbacks_.exit(callbacks_.context);
        return;
      }
      break;
    default:
      break;
  }
  render();
}

void GameView::render_progress() {
  if (shown_level_ == game_.level_index() && shown_passed_ == game_.passed() &&
      shown_goal_ == game_.level().goal) return;
  char text[40];
  std::snprintf(text, sizeof(text), "第%d关  %d/%d", game_.level_index() + 1,
                game_.passed(), game_.level().goal);
  lv_label_set_text(progress_, text);
  shown_passed_ = game_.passed();
  shown_goal_ = game_.level().goal;
}

void GameView::render_world() {
  const int bird_y = static_cast<int>(std::lround(game_.bird_y()));
  lv_obj_set_pos(bird_, 76, bird_y - 137);
  lv_obj_set_y(wing_, game_.velocity_y() < 0 ? 6 : 11);
  for (int i = 0; i < Game::kMaxGates; ++i) {
    const Gate& model = game_.gates()[i];
    auto& gate = gates_[i];
    visible(gate.upper, model.active);
    visible(gate.lower, model.active);
    visible(gate.upper_shine, model.active);
    visible(gate.lower_shine, model.active);
    if (!model.active) continue;
    const int x = static_cast<int>(std::lround(model.x)) - 60;
    const int upper_end = static_cast<int>(std::lround(model.gap_y - game_.level().gap / 2));
    const int lower_start = static_cast<int>(std::lround(model.gap_y + game_.level().gap / 2));
    lv_obj_set_pos(gate.upper, x, 0);
    lv_obj_set_size(gate.upper, 38, std::max(0, upper_end - 126));
    lv_obj_set_pos(gate.upper_shine, x + 7, 0);
    lv_obj_set_size(gate.upper_shine, 3, std::max(0, upper_end - 126));
    lv_obj_set_pos(gate.lower, x, lower_start - 126);
    lv_obj_set_size(gate.lower, 38, std::max(0, 354 - lower_start));
    lv_obj_set_pos(gate.lower_shine, x + 7, lower_start - 126);
    lv_obj_set_size(gate.lower_shine, 3, std::max(0, 354 - lower_start));
  }
}

void GameView::render_phase() {
  const Phase phase = game_.phase();
  const bool phase_changed = shown_phase_ != phase;
  shown_phase_ = phase;
  const bool ready = phase == Phase::Ready;
  const bool modal = ready || phase == Phase::Paused || phase == Phase::Failed ||
                     phase == Phase::Cleared || phase == Phase::Complete;
  for (auto* bound : bounds_) visible(bound, !modal);
  visible(overlay_, modal || phase == Phase::Countdown);
  visible(modal_, modal);
  visible(stage_title_, ready);
  visible(pause_button_, phase == Phase::Playing || phase == Phase::Countdown);
  visible(countdown_, phase == Phase::Countdown);
  for (int i = 0; i < Game::kLevels; ++i) visible(stage_buttons_[i], ready);

  if (ready && (shown_level_ != game_.level_index() ||
                shown_unlocked_ != game_.unlocked_levels() || phase_changed)) {
    shown_level_ = game_.level_index();
    shown_unlocked_ = game_.unlocked_levels();
    for (int i = 0; i < Game::kLevels; ++i) {
      const unsigned fill = i == shown_level_ ? 0x2db4a5 :
                            i < shown_unlocked_ ? 0x365368 : 0x2b3946;
      lv_obj_set_style_bg_color(stage_buttons_[i], color(fill), 0);
      lv_obj_set_style_opa(stage_buttons_[i], i < shown_unlocked_ ? LV_OPA_COVER : LV_OPA_60, 0);
    }
  }

  if (phase == Phase::Countdown) {
    const int digit = std::max(1, static_cast<int>(std::ceil(game_.countdown_left())));
    if (shown_countdown_ != digit) {
      char text[20];
      std::snprintf(text, sizeof(text), "%d", digit);
      lv_label_set_text(countdown_, text);
      shown_countdown_ = digit;
    }
  } else {
    shown_countdown_ = -1;
  }
  if (!modal || !phase_changed) return;
  const char* heading = "";
  const char* detail = "";
  const char* actions[3] = {"", "", "桌面"};
  switch (phase) {
    case Phase::Ready:
      heading = "准备好了吗";
      detail = "轻点起飞  穿过光门";
      actions[0] = "开始";
      break;
    case Phase::Paused:
      heading = "暂停";
      detail = "保持节奏";
      actions[0] = "继续";
      actions[1] = "重开";
      break;
    case Phase::Failed:
      heading = "飞行结束";
      detail = "再试一次";
      actions[0] = "再试";
      break;
    case Phase::Cleared:
      heading = "通关成功";
      detail = "漂亮飞行";
      actions[0] = "下一关";
      actions[1] = "重开";
      break;
    case Phase::Complete:
      heading = "全部通关";
      detail = "漂亮飞行";
      actions[0] = "重开";
      break;
    default:
      break;
  }
  lv_label_set_text(modal_title_, heading);
  lv_label_set_text(modal_detail_, detail);
  for (int i = 0; i < 3; ++i) {
    visible(action_buttons_[i], actions[i][0] != '\0');
    lv_label_set_text(action_labels_[i], actions[i]);
  }
}

void GameView::render() {
  render_world();
  render_progress();
  render_phase();
}

}  // namespace roundwing
