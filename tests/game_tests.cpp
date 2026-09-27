#include "game.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

using roundwing::Game;
using roundwing::Phase;

namespace {

int checks = 0;

void require(bool condition, const char* message) {
  ++checks;
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

void require_same(const Game& a, const Game& b, const char* message) {
  require(a.phase() == b.phase() && a.passed() == b.passed() &&
              a.bird_y() == b.bird_y() && a.velocity_y() == b.velocity_y() &&
              a.countdown_left() == b.countdown_left(), message);
  for (int i = 0; i < Game::kMaxGates; ++i) {
    const auto& x = a.gates()[i];
    const auto& y = b.gates()[i];
    require(x.x == y.x && x.gap_y == y.gap_y && x.active == y.active &&
                x.scored == y.scored, message);
  }
}

void test_definitions_and_transitions() {
  constexpr std::array<int, 4> goals{{6, 8, 10, 12}};
  constexpr std::array<float, 4> gaps{{132, 120, 108, 100}};
  constexpr std::array<float, 4> speeds{{100, 110, 120, 130}};
  constexpr std::array<float, 4> spacing{{190, 185, 180, 180}};
  Game game;
  require(game.phase() == Phase::Ready && game.bird_y() == 233 &&
              game.gates()[0].x == 406 && game.gates()[0].gap_y == 233,
          "initial ready state and first gate");
  for (int i = 0; i < 4; ++i) {
    game.select_level(i);
    const auto& level = game.level();
    require(level.goal == goals[i] && level.gap == gaps[i] &&
                level.speed == speeds[i] && level.spacing == spacing[i],
            "level constants");
    require(game.phase() == Phase::Ready && game.level_index() == i &&
                game.passed() == 0, "select resets level");
  }
  game.select_level(-9);
  require(game.level_index() == 0, "level lower clamp");
  game.select_level(90);
  require(game.level_index() == 3, "level upper clamp");
  game.set_unlocked_levels(0);
  require(game.unlocked_levels() == 1, "unlock lower clamp");
  game.set_unlocked_levels(99);
  require(game.unlocked_levels() == 4, "unlock upper clamp");
  game.next_level();
  require(game.level_index() == 3, "next only after clear");
  game.flap();
  require(game.phase() == Phase::Playing && game.velocity_y() == -220,
          "first tap starts with upward velocity");
  game.restart();
  require(game.phase() == Phase::Ready && game.bird_y() == 233 &&
              game.velocity_y() == 0, "restart resets physics");
}

void test_time_and_pause() {
  Game base;
  base.flap();
  Game invalid = base;
  invalid.advance(0);
  invalid.advance(-1);
  invalid.advance(std::numeric_limits<float>::infinity());
  invalid.advance(std::numeric_limits<float>::quiet_NaN());
  require_same(invalid, base, "invalid deltas ignored");

  Game whole = base;
  Game split = base;
  whole.advance(0.1f);
  split.advance(0.05f);
  split.advance(0.05f);
  require_same(whole, split, "fixed-step delta partition");

  Game paused = whole;
  paused.advance(0.151f);
  require(paused.phase() == Phase::Paused, "long stall pauses");
  Game frozen = paused;
  paused.advance(0.1f);
  paused.flap();
  require_same(paused, frozen, "paused simulation and tap freeze");
  paused.resume();
  require(paused.phase() == Phase::Countdown &&
              std::abs(paused.countdown_left() - 0.8f) < 0.0001f,
          "resume begins countdown");
  const float y = paused.bird_y();
  const float x = paused.gates()[0].x;
  for (int i = 0; i < 7; ++i) paused.advance(0.1f);
  require(paused.phase() == Phase::Countdown && paused.bird_y() == y &&
              paused.gates()[0].x == x, "countdown freezes world");
  paused.pause();
  require(paused.phase() == Phase::Paused, "countdown can pause");
  paused.resume();
  for (int i = 0; i < 8; ++i) paused.advance(0.1f);
  require(paused.phase() == Phase::Playing && paused.bird_y() == y,
          "countdown completes without physics");
  paused.advance(1.0f / 120);
  require(paused.bird_y() != y, "physics resumes on next advance");
}

void test_boundaries() {
  Game bottom;
  bottom.flap();
  for (int i = 0; i < 180 && bottom.phase() == Phase::Playing; ++i)
    bottom.advance(1.0f / 120);
  require(bottom.phase() == Phase::Failed &&
              bottom.bird_y() + Game::kBirdRadius >= Game::kBottom,
          "bottom contact fails");
  Game top;
  top.flap();
  for (int i = 0; i < 180 && top.phase() == Phase::Playing; ++i) {
    top.flap();
    top.advance(1.0f / 120);
  }
  require(top.phase() == Phase::Failed &&
              top.bird_y() - Game::kBirdRadius <= Game::kTop,
          "top contact fails");
  Game frozen = top;
  top.advance(0.1f);
  top.flap();
  require_same(top, frozen, "failure freezes world");
}

void test_gate_impact() {
  Game game;
  game.flap();
  while (game.phase() == Phase::Playing && game.gates()[0].x > 210) {
    if (game.velocity_y() >= -180 &&
        game.bird_y() + game.velocity_y() * 0.08f > 233)
      game.flap();
    game.advance(1.0f / 120);
  }
  require(game.phase() == Phase::Playing, "approach gate alive");
  int ticks = 0;
  while (game.phase() == Phase::Playing && ticks++ < 120) {
    if (ticks == 1) game.flap();
    game.advance(1.0f / 120);
  }
  require(game.phase() == Phase::Failed &&
              game.bird_y() - Game::kBirdRadius > Game::kTop &&
              game.gates()[0].x < Game::kBirdX + Game::kBirdRadius &&
              game.gates()[0].x + Game::kGateWidth >
                  Game::kBirdX - Game::kBirdRadius &&
              game.passed() == 0,
          "gate collision fails before scoring without boundary contact");
}

float target_gap(const Game& game) {
  float nearest = std::numeric_limits<float>::infinity();
  float target = 233;
  for (const auto& gate : game.gates()) {
    if (gate.active && gate.x + Game::kGateWidth >=
                           Game::kBirdX - Game::kBirdRadius &&
        gate.x < nearest) {
      nearest = gate.x;
      target = gate.gap_y;
    }
  }
  return target;
}

// Search small controller settings, then replay the found taps exactly.
bool solve_level(int level_index, Game& solved, int unlocked = 1) {
  constexpr std::array<std::array<float, 12>, 4> centers{{
      {{233, 213, 233, 253, 233, 233}},
      {{233, 201, 233, 265, 233, 209, 241, 233}},
      {{233, 193, 225, 269, 233, 201, 245, 277, 237, 209}},
      {{233, 187, 233, 279, 233, 193, 245, 279, 225, 187, 239, 233}},
  }};
  constexpr std::array<float, 9> horizons{{0.08f, 0.12f, 0.16f, 0.20f,
                                            0.24f, 0.28f, 0.32f, 0.38f,
                                            0.44f}};
  constexpr std::array<float, 15> offsets{{-28, -24, -20, -16, -12,
                                            -8, -4, 0, 4, 8, 12, 16,
                                            20, 24, 28}};
  constexpr std::array<float, 10> min_velocity{{-180, -160, -140, -120,
                                                 -100, -80, -60, -40,
                                                 -20, 0}};
  for (float horizon : horizons) {
    for (float offset : offsets) {
      for (float minimum : min_velocity) {
        Game game;
        game.set_unlocked_levels(unlocked);
        game.select_level(level_index);
        std::vector<int> flap_frames;
        std::array<roundwing::Gate, Game::kMaxGates> previous = game.gates();
        int observed_spawns = 1;
        int previous_passed = 0;
        for (int tick = 0; tick < 5000 &&
                           (game.phase() == Phase::Ready ||
                            game.phase() == Phase::Playing);
             ++tick) {
          const float target = target_gap(game);
          if (game.phase() == Phase::Ready ||
              (game.velocity_y() >= minimum &&
               game.bird_y() + game.velocity_y() * horizon > target + offset)) {
            game.flap();
            flap_frames.push_back(tick);
          }
          game.advance(1.0f / 120);
          for (int slot = 0; slot < Game::kMaxGates; ++slot) {
            const auto& gate = game.gates()[slot];
            if (gate.active &&
                (!previous[slot].active || gate.x > previous[slot].x)) {
              require(observed_spawns < game.level().goal &&
                          gate.gap_y == centers[level_index][observed_spawns],
                      "gate spawn order and centers");
              ++observed_spawns;
            }
          }
          require(game.passed() >= previous_passed &&
                      game.passed() <= previous_passed + 1 &&
                      game.passed() <= observed_spawns,
                  "score increments once per passing gate");
          previous = game.gates();
          previous_passed = game.passed();
        }
        if (game.phase() == Phase::Cleared || game.phase() == Phase::Complete) {
          require(observed_spawns == game.level().goal,
                  "all goal gates spawned before victory");
          Game replay;
          replay.set_unlocked_levels(unlocked);
          replay.select_level(level_index);
          for (int tick = 0, next_flap = 0; tick < 5000 &&
                                      (replay.phase() == Phase::Ready ||
                                       replay.phase() == Phase::Playing); ++tick) {
            if (next_flap < static_cast<int>(flap_frames.size()) &&
                flap_frames[next_flap] == tick) {
              replay.flap();
              ++next_flap;
            }
            replay.advance(1.0f / 120);
          }
          require_same(replay, game, "flap frame replay is deterministic");
          const std::string path = "artifacts/replay-level-" +
                                   std::to_string(level_index + 1) + ".txt";
          std::ofstream output(path);
          require(output.good(), "replay artifact opens");
          for (int frame : flap_frames) output << frame << '\n';
          output.close();
          require(output.good(), "replay artifact writes");
          solved = game;
          std::cout << "level " << level_index + 1 << " solved: horizon="
                    << horizon << " offset=" << offset
                    << " min_velocity=" << minimum << '\n';
          return true;
        }
      }
    }
  }
  return false;
}

void test_complete_levels() {
  Game progress;
  for (int i = 0; i < Game::kLevels; ++i) {
    Game solved;
    require(solve_level(i, solved), "automated tap solver completes stage");
    require(solved.passed() == solved.level().goal, "each gate scores once");
    require(solved.unlocked_levels() == std::min(i + 2, Game::kLevels),
            "victory unlocks next level");
    Game frozen = solved;
    solved.advance(0.1f);
    solved.flap();
    require_same(solved, frozen, "victory freezes world and bird");
    if (i == Game::kLevels - 1) {
      require(solved.phase() == Phase::Complete, "last victory completes game");
      solved.next_level();
      require(solved.phase() == Phase::Complete, "complete has no next level");
    } else {
      require(solved.phase() == Phase::Cleared, "stage victory clears stage");
      solved.next_level();
      require(solved.level_index() == i + 1 && solved.phase() == Phase::Ready,
              "next level enters ready");
    }
  }
  progress.set_unlocked_levels(4);
  progress.select_level(0);
  require(solve_level(0, progress, 4), "earlier level replays");
  require(progress.unlocked_levels() == 4, "replay retains progress");
}

}  // namespace

int main() {
  test_definitions_and_transitions();
  test_time_and_pause();
  test_boundaries();
  test_gate_impact();
  test_complete_levels();
  std::cout << "PASS: " << checks << " checks\n";
}
