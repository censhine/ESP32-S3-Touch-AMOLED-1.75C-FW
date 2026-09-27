#pragma once

#include <array>

namespace roundwing {

enum class Phase { Ready, Playing, Paused, Countdown, Failed, Cleared, Complete };

struct Gate {
  float x;
  float gap_y;
  bool active;
  bool scored;
};

struct Level {
  int goal;
  float gap;
  float speed;
  float spacing;
  const char* name;
};

class Game {
 public:
  static constexpr int kLevels = 4, kMaxGates = 3;
  static constexpr float kBirdX = 150, kTop = 126, kBottom = 354;
  static constexpr float kLeft = 60, kRight = 406, kBirdRadius = 9;
  static constexpr float kGateWidth = 38;

  Game();
  void select_level(int index);
  void flap();
  void pause();
  void resume();
  void restart();
  void next_level();
  void advance(float seconds);

  Phase phase() const { return phase_; }
  int level_index() const { return level_index_; }
  int passed() const { return passed_; }
  int unlocked_levels() const { return unlocked_levels_; }
  void set_unlocked_levels(int count);
  float bird_y() const { return bird_y_; }
  float velocity_y() const { return velocity_y_; }
  float countdown_left() const { return countdown_left_; }
  const Level& level() const;
  const std::array<Gate, kMaxGates>& gates() const { return gates_; }
  static const Level& level_definition(int index);

 private:
  void reset();
  void step();
  void spawn_gate(float x);
  bool hit_gate(const Gate& gate) const;

  Phase phase_ = Phase::Ready;
  int level_index_ = 0;
  int unlocked_levels_ = 1;
  int passed_ = 0;
  int spawned_ = 0;
  int last_gate_slot_ = 0;
  float bird_y_ = 233;
  float velocity_y_ = 0;
  float countdown_left_ = 0;
  double accumulator_ = 0;
  std::array<Gate, kMaxGates> gates_{};
};

}  // namespace roundwing
