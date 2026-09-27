#include "round_wing_app.hpp"

#include <array>
#include <cstdint>
#include <memory>

#include "esp_brookesia.hpp"
#include "esp_lib_utils.h"
#include "esp_log.h"
#include "nvs.h"

namespace {

constexpr int kIconSize = 112;
constexpr char kLogTag[] = "RoundWing";

// Original bird artwork generated as pixels at compile time, with no external decoder.
constexpr std::array<uint8_t, kIconSize * kIconSize * 4> makeIcon() {
  std::array<uint8_t, kIconSize * kIconSize * 4> pixels{};
  for (int y = 0; y < kIconSize; ++y) {
    for (int x = 0; x < kIconSize; ++x) {
      const int dx = x - 56;
      const int dy = y - 56;
      const bool disc = dx * dx + dy * dy < 53 * 53;
      const bool body = (x - 54) * (x - 54) * 4 + (y - 57) * (y - 57) * 9 < 47 * 47 * 4;
      const bool wing = (x - 40) * (x - 40) * 4 + (y - 66) * (y - 66) * 9 < 17 * 17 * 4;
      const bool eye = (x - 72) * (x - 72) + (y - 46) * (y - 46) < 5 * 5;
      const bool beak = x > 80 && x < 99 && y > 57 && y < 68 && x + y < 161;
      const auto index = (y * kIconSize + x) * 4;
      if (disc) {
        uint8_t r = 28, g = 63, b = 79;
        if (body) { r = 255; g = 207; b = 82; }
        if (wing) { r = 238; g = 119; b = 79; }
        if (eye) { r = 22; g = 40; b = 52; }
        if (beak) { r = 255; g = 131; b = 69; }
        pixels[index] = b;
        pixels[index + 1] = g;
        pixels[index + 2] = r;
        pixels[index + 3] = 255;
      }
    }
  }
  return pixels;
}

constexpr auto kIconPixels = makeIcon();
const lv_image_dsc_t kIcon = {
    {LV_IMAGE_HEADER_MAGIC, LV_COLOR_FORMAT_ARGB8888, 0, kIconSize, kIconSize, kIconSize * 4, 0},
    static_cast<uint32_t>(kIconPixels.size()),
    kIconPixels.data(),
    nullptr,
    nullptr,
};

int loadProgress() {
  nvs_handle_t handle;
  const esp_err_t open_result = nvs_open("roundwing", NVS_READONLY, &handle);
  if (open_result == ESP_ERR_NVS_NOT_FOUND) return 1;
  if (open_result != ESP_OK) {
    ESP_LOGW(kLogTag, "Progress read unavailable: %s", esp_err_to_name(open_result));
    return 1;
  }
  uint8_t count = 1;
  const esp_err_t error = nvs_get_u8(handle, "unlocked", &count);
  nvs_close(handle);
  return error == ESP_OK && count >= 1 && count <= roundwing::Game::kLevels ? count : 1;
}

}  // namespace

namespace esp_brookesia::apps {

RoundWingApp* RoundWingApp::instance_ = nullptr;

RoundWingApp* RoundWingApp::requestInstance() {
  if (!instance_) instance_ = new RoundWingApp();
  return instance_;
}

RoundWingApp::RoundWingApp()
    : App("Round Wing", &kIcon, true, false, false) {}

RoundWingApp::~RoundWingApp() {
  stopView();
  if (close_pending_) lv_async_call_cancel(deferredClose, this);
  instance_ = nullptr;
}

bool RoundWingApp::run() {
  if (close_pending_) lv_async_call_cancel(deferredClose, this);
  stopView();
  close_pending_ = false;
  const roundwing::ViewCallbacks callbacks{exitRequested, saveProgress, this};
  view_ = std::make_unique<roundwing::GameView>(lv_screen_active(), callbacks, loadProgress());
  last_tick_ = lv_tick_get();
  timer_ = lv_timer_create(tick, 33, this);
  if (!timer_) {
    view_.reset();
    return false;
  }
  return true;
}

void RoundWingApp::tick(lv_timer_t* timer) {
  auto* self = static_cast<RoundWingApp*>(lv_timer_get_user_data(timer));
  if (!self->view_) return;
  const uint32_t now = lv_tick_get();
  const float delta = static_cast<float>(now - self->last_tick_) / 1000.0f;
  self->last_tick_ = now;
  self->view_->update(delta);
}

void RoundWingApp::exitRequested(void* context) {
  static_cast<RoundWingApp*>(context)->requestClose();
}

void RoundWingApp::requestClose() {
  if (close_pending_) return;
  if (view_) view_->suspend();
  if (timer_) lv_timer_pause(timer_);
  close_pending_ = true;
  // GameView invokes exit from an LVGL event callback; core close mutates that tree.
  if (lv_async_call(deferredClose, this) != LV_RESULT_OK) {
    close_pending_ = false;
    if (timer_) lv_timer_resume(timer_);
  }
}

void RoundWingApp::deferredClose(void* context) {
  auto* self = static_cast<RoundWingApp*>(context);
  self->close_pending_ = false;
  self->notifyCoreClosed();
}

bool RoundWingApp::back() {
  if (view_) view_->suspend();
  requestClose();
  return true;
}

bool RoundWingApp::pause() {
  if (view_) view_->suspend();
  if (timer_) lv_timer_pause(timer_);
  return true;
}

bool RoundWingApp::resume() {
  last_tick_ = lv_tick_get();
  if (timer_ && !close_pending_) lv_timer_resume(timer_);
  return true;
}

void RoundWingApp::stopView() {
  if (timer_) {
    lv_timer_delete(timer_);
    timer_ = nullptr;
  }
  view_.reset();
}

bool RoundWingApp::close() {
  if (close_pending_) {
    lv_async_call_cancel(deferredClose, this);
    close_pending_ = false;
  }
  stopView();
  return true;
}

void RoundWingApp::saveProgress(int unlocked, void*) {
  if (unlocked < 1 || unlocked > roundwing::Game::kLevels) return;
  nvs_handle_t handle;
  const esp_err_t open_result = nvs_open("roundwing", NVS_READWRITE, &handle);
  if (open_result != ESP_OK) {
    ESP_LOGW(kLogTag, "Progress save unavailable: %s", esp_err_to_name(open_result));
    return;
  }
  esp_err_t result = nvs_set_u8(handle, "unlocked", static_cast<uint8_t>(unlocked));
  if (result == ESP_OK) result = nvs_commit(handle);
  if (result != ESP_OK) ESP_LOGW(kLogTag, "Progress save failed: %s", esp_err_to_name(result));
  nvs_close(handle);
}

ESP_UTILS_REGISTER_PLUGIN_WITH_CONSTRUCTOR(systems::base::App, RoundWingApp, "Round Wing", []() {
  return std::shared_ptr<RoundWingApp>(RoundWingApp::requestInstance(), [](RoundWingApp*) {});
})

}  // namespace esp_brookesia::apps
