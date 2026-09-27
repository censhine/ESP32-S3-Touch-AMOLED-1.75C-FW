#pragma once

#include <array>

#include "game.hpp"
#include "lvgl.h"

namespace roundwing {

struct ViewCallbacks {
  void (*exit)(void*) = nullptr;
  void (*save_progress)(int, void*) = nullptr;
  void* context = nullptr;
};

class GameView {
 public:
  GameView(lv_obj_t* parent, ViewCallbacks callbacks, int unlocked = 1);
  ~GameView();

  GameView(const GameView&) = delete;
  GameView& operator=(const GameView&) = delete;

  void update(float elapsed_seconds);
  void suspend();
  void tap();
  Game& game() { return game_; }

 private:
  struct GateView {
    lv_obj_t* upper = nullptr;
    lv_obj_t* lower = nullptr;
    lv_obj_t* upper_shine = nullptr;
    lv_obj_t* lower_shine = nullptr;
  };

  static void on_root_pressed(lv_event_t* event);
  static void on_pause_clicked(lv_event_t* event);
  static void on_action_clicked(lv_event_t* event);
  static void on_stage_clicked(lv_event_t* event);

  void render();
  void render_phase();
  void render_world();
  void render_progress();
  void action(int index);

  Game game_;
  ViewCallbacks callbacks_;
  lv_obj_t* root_ = nullptr;
  lv_obj_t* title_ = nullptr;
  lv_obj_t* progress_ = nullptr;
  lv_obj_t* pause_button_ = nullptr;
  lv_obj_t* world_ = nullptr;
  lv_obj_t* bird_ = nullptr;
  lv_obj_t* wing_ = nullptr;
  lv_obj_t* overlay_ = nullptr;
  lv_obj_t* modal_ = nullptr;
  lv_obj_t* modal_title_ = nullptr;
  lv_obj_t* modal_detail_ = nullptr;
  lv_obj_t* stage_title_ = nullptr;
  lv_obj_t* countdown_ = nullptr;
  std::array<lv_obj_t*, 3> action_buttons_{};
  std::array<lv_obj_t*, 3> action_labels_{};
  std::array<lv_obj_t*, Game::kLevels> stage_buttons_{};
  std::array<lv_obj_t*, 4> bounds_{};
  std::array<GateView, Game::kMaxGates> gates_{};
  Phase shown_phase_ = Phase::Ready;
  int saved_unlocked_ = 1;
  int shown_level_ = -1;
  int shown_unlocked_ = -1;
  int shown_passed_ = -1;
  int shown_goal_ = -1;
  int shown_countdown_ = -1;
};

}  // namespace roundwing
