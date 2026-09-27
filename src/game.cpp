#include "game.hpp"

#include <algorithm>
#include <cmath>

namespace roundwing {
namespace {

constexpr float kGravity = 600;
constexpr float kFlapVelocity = -220;
constexpr float kTerminalVelocity = 300;
constexpr double kStep = 1.0 / 120.0;
constexpr float kCountdown = 0.8f;

constexpr std::array<Level, Game::kLevels> kLevelDefs{{
    {6, 132, 100, 190, "First Flight"},
    {8, 120, 110, 185, "Changing Winds"},
    {10, 108, 120, 180, "High Passage"},
    {12, 100, 130, 180, "Round Wing"},
}};

constexpr std::array<std::array<float, 12>, Game::kLevels> kGapCenters{{
    {{233, 213, 233, 253, 233, 233}},
    {{233, 201, 233, 265, 233, 209, 241, 233}},
    {{233, 193, 225, 269, 233, 201, 245, 277, 237, 209}},
    {{233, 187, 233, 279, 233, 193, 245, 279, 225, 187, 239, 233}},
}};

bool circle_hits_rect(float cx, float cy, float radius, float left,
                      float top, float right, float bottom) {
  if (left >= right || top >= bottom) return false;
  const float dx = cx - std::clamp(cx, left, right);
  const float dy = cy - std::clamp(cy, top, bottom);
  return dx * dx + dy * dy <= radius * radius;
}

}  // namespace

Game::Game() { reset(); }

const Level& Game::level_definition(int index) {
  return kLevelDefs[std::clamp(index, 0, kLevels - 1)];
}

const Level& Game::level() const { return level_definition(level_index_); }

void Game::reset() {
  phase_ = Phase::Ready;
  passed_ = 0;
  spawned_ = 0;
  last_gate_slot_ = 0;
  bird_y_ = 233;
  velocity_y_ = 0;
  countdown_left_ = 0;
  accumulator_ = 0;
  gates_.fill({0, 0, false, false});
  spawn_gate(kRight);
}

void Game::select_level(int index) {
  level_index_ = std::clamp(index, 0, kLevels - 1);
  reset();
}

void Game::set_unlocked_levels(int count) {
  unlocked_levels_ = std::clamp(count, 1, kLevels);
}

void Game::flap() {
  if (phase_ == Phase::Ready) phase_ = Phase::Playing;
  if (phase_ == Phase::Playing) velocity_y_ = kFlapVelocity;
}

void Game::pause() {
  if (phase_ == Phase::Playing || phase_ == Phase::Countdown)
    phase_ = Phase::Paused;
}

void Game::resume() {
  if (phase_ != Phase::Paused) return;
  phase_ = Phase::Countdown;
  countdown_left_ = kCountdown;
  accumulator_ = 0;
}

void Game::restart() { reset(); }

void Game::next_level() {
  if (phase_ != Phase::Cleared) return;
  ++level_index_;
  reset();
}

void Game::spawn_gate(float x) {
  if (spawned_ >= level().goal) return;
  for (int slot = 0; slot < kMaxGates; ++slot) {
    Gate& gate = gates_[slot];
    if (!gate.active) {
      gate = {x, kGapCenters[level_index_][spawned_], true, false};
      last_gate_slot_ = slot;
      ++spawned_;
      return;
    }
  }
}

bool Game::hit_gate(const Gate& gate) const {
  const float left = gate.x + 2;
  const float right = gate.x + kGateWidth - 2;
  const float gap_top = gate.gap_y - level().gap / 2;
  const float gap_bottom = gate.gap_y + level().gap / 2;
  return circle_hits_rect(kBirdX, bird_y_, kBirdRadius, left, kTop + 2,
                          right, gap_top - 2) ||
         circle_hits_rect(kBirdX, bird_y_, kBirdRadius, left, gap_bottom + 2,
                          right, kBottom - 2);
}

void Game::step() {
  velocity_y_ = std::min(velocity_y_ + kGravity * static_cast<float>(kStep),
                         kTerminalVelocity);
  bird_y_ += velocity_y_ * static_cast<float>(kStep);
  if (bird_y_ - kBirdRadius <= kTop ||
      bird_y_ + kBirdRadius >= kBottom) {
    phase_ = Phase::Failed;
    return;
  }

  for (Gate& gate : gates_) {
    if (!gate.active) continue;
    gate.x -= level().speed * static_cast<float>(kStep);
    if (hit_gate(gate)) {
      phase_ = Phase::Failed;
      return;
    }
  }

  for (Gate& gate : gates_) {
    if (!gate.active) continue;
    if (!gate.scored && gate.x + kGateWidth < kBirdX - kBirdRadius) {
      gate.scored = true;
      ++passed_;
      if (passed_ == level().goal) {
        if (level_index_ == kLevels - 1) {
          phase_ = Phase::Complete;
          unlocked_levels_ = kLevels;
        } else {
          phase_ = Phase::Cleared;
          unlocked_levels_ = std::max(unlocked_levels_, level_index_ + 2);
        }
        return;
      }
    }
    if (gate.x + kGateWidth < kLeft) gate.active = false;
  }

  if (spawned_ < level().goal &&
      gates_[last_gate_slot_].x <= kRight - level().spacing)
    spawn_gate(gates_[last_gate_slot_].x + level().spacing);
}

void Game::advance(float seconds) {
  if (!std::isfinite(seconds) || seconds <= 0) return;
  if (seconds > 0.15f) {
    pause();
    return;
  }
  if (phase_ == Phase::Countdown) {
    countdown_left_ = std::max(0.0f, countdown_left_ - seconds);
    if (countdown_left_ == 0) phase_ = Phase::Playing;
    return;
  }
  if (phase_ != Phase::Playing) return;
  accumulator_ += seconds;
  while (accumulator_ >= kStep && phase_ == Phase::Playing) {
    accumulator_ -= kStep;
    step();
  }
  if (phase_ != Phase::Playing) accumulator_ = 0;
}

}  // namespace roundwing
