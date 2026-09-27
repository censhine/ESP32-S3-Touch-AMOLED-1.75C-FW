#pragma once

#include <memory>

#include "systems/phone/esp_brookesia_phone_app.hpp"
#include "game_view.hpp"

namespace esp_brookesia::apps {

class RoundWingApp final : public systems::phone::App {
 public:
  static RoundWingApp* requestInstance();
  ~RoundWingApp() override;

 protected:
  RoundWingApp();
  bool run() override;
  bool back() override;
  bool close() override;
  bool pause() override;
  bool resume() override;

 private:
  static void tick(lv_timer_t* timer);
  static void exitRequested(void* context);
  static void saveProgress(int unlocked, void* context);
  static void deferredClose(void* context);
  void stopView();
  void requestClose();

  static RoundWingApp* instance_;
  std::unique_ptr<roundwing::GameView> view_;
  lv_timer_t* timer_ = nullptr;
  uint32_t last_tick_ = 0;
  bool close_pending_ = false;
};

}  // namespace esp_brookesia::apps
