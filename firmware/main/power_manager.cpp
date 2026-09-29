#include "power_manager.hpp"

#include "app_idle_policy.hpp"
#include "bsp/esp-bsp.h"
#include "bsp_board_extra.h"
#include "driver/gpio.h"
#include "esp_brookesia.hpp"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lv_adapter.h"
#include "esp_pm.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "storage_service.h"
#include "system_status.hpp"

namespace brookesia::power {
namespace {
constexpr char TAG[] = "PowerManager";
constexpr uint32_t DARK_SETTLE_MS = 5000;
constexpr uint32_t STANDBY_RETRY_MS = 30000;
using Phone = esp_brookesia::systems::phone::Phone;
using Context = esp_brookesia::systems::base::Context;

struct State {
    esp_lcd_panel_handle_t panel = nullptr;
    lv_display_t *display = nullptr;
    Phone *phone = nullptr;
    TaskHandle_t task = nullptr;
    lv_timer_t *cleanup_timer = nullptr;
    AppIdlePolicy apps;
};
State state;
portMUX_TYPE wake_lock = portMUX_INITIALIZER_UNLOCKED;
bool wake_requested = false;

bool wake_pending()
{
    portENTER_CRITICAL(&wake_lock);
    const bool pending = wake_requested;
    portEXIT_CRITICAL(&wake_lock);
    return pending;
}

void IRAM_ATTR on_power_wake(void *)
{
    portENTER_CRITICAL_ISR(&wake_lock);
    wake_requested = true;
    portEXIT_CRITICAL_ISR(&wake_lock);
    // GPIO wake remains enabled; suppress repeated level interrupts while the
    // key is held. This also latches short presses during panel sleep delays.
    gpio_intr_disable(BSP_BUTTON_PWR_GPIO);
}

bool keys_released()
{
    return gpio_get_level(BSP_BUTTON_PWR_GPIO) == 0 &&
           gpio_get_level(BSP_BUTTON_BOOT_GPIO) != 0;
}

bool foreground_app(esp_brookesia::systems::base::App *app)
{
    auto *recents = state.phone->getDisplay().getRecentsScreen();
    return app == state.phone->getManager().getActiveApp() &&
           (recents == nullptr || !recents->checkVisible());
}

void on_app_event(lv_event_t *event)
{
    const auto *data = static_cast<const Context::AppEventData *>(lv_event_get_param(event));
    if (data == nullptr) return;
    // Registered after the manager, which processes the event synchronously.
    // Forget stopped lifetimes even when the same installed app is reopened
    // before the next one-second policy scan. START also grants a fresh window.
    if (data->type == Context::AppEventType::START ||
            (data->type == Context::AppEventType::STOP &&
             state.phone->getManager().getRunningAppById(data->id) == nullptr)) {
        state.apps.forget(data->id);
    }
}

// Caller owns the LVGL lock. A failed application close leaves the manager
// entry in place, so it cannot silently permit standby with a live worker.
void close_idle_apps()
{
    auto &manager = state.phone->getManager();
    const uint32_t now = lv_tick_get();
    const uint32_t idle = lv_display_get_inactive_time(state.display);
    int due[32];
    unsigned count = 0;
    state.apps.begin_scan();
    for (unsigned i = 0; i < manager.getRunningAppCount(); ++i) {
        auto *app = manager.getRunningAppByIdenx(i);
        if (app != nullptr && state.apps.observe(app->getId(),
                foreground_app(app), now, idle) && count < 32) {
            due[count++] = app->getId();
        }
    }
    state.apps.end_scan();
    for (unsigned i = 0; i < count; ++i) {
        auto *app = manager.getRunningAppById(due[i]);
        if (app == nullptr) {
            continue;
        }
        ESP_LOGI(TAG, "Idle close: %s id=%d (%s)", app->getName(), due[i],
                 foreground_app(app) ? "foreground 300s" : "background 120s");
        const Context::AppEventData event{due[i], Context::AppEventType::STOP, nullptr};
        const bool sent = state.phone->sendAppEvent(&event);
        if (!sent || manager.getRunningAppById(due[i]) != nullptr) {
            state.apps.close_failed(due[i], lv_tick_get());
            ESP_LOGW(TAG, "App %d retained after incomplete cleanup; retry after 30s", due[i]);
        } else {
            ESP_LOGI(TAG, "App %d closed; internal=%zu psram=%zu", due[i],
                     heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
                     heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
        }
    }
}

void cleanup_tick(lv_timer_t *)
{
    // App STOP can execute deep UI teardown. Keep it on the existing 20 KiB
    // LVGL worker stack, in the same context as a manual application close.
    close_idle_apps();
}

bool standby_ready()
{
    if (!screen_power::is_off() || screen_power::needs_standby_recovery() ||
            screen_power::off_duration_ms() < DARK_SETTLE_MS ||
            state.phone->getManager().getRunningAppCount() != 0 || !keys_released() ||
            bsp_extra_audio_session_get_owner() != BSP_EXTRA_AUDIO_OWNER_NONE) {
        return false;
    }
    storage_service_info_t storage{};
    return storage_service_get_info(&storage) == ESP_OK && storage.active_leases == 0 &&
           storage.state != STORAGE_SERVICE_STATE_MOUNTING &&
           storage.state != STORAGE_SERVICE_STATE_EJECTING;
}

// No LVGL lock on entry: both services need their own worker to acknowledge
// suspension. Every completed preparation step is unwound even on failure.
void enter_standby()
{
    bool status_requested = false;
    bool adapter_paused = false;
    bool panel_attempted = false;
    bool wake_armed = false;
    bool wake_handler = false;
    esp_err_t result = ESP_OK;
    int64_t slept_us = 0;
    auto &gui = esp_brookesia::gui::LvLock::getInstance();

    do {
        portENTER_CRITICAL(&wake_lock);
        wake_requested = false;
        portEXIT_CRITICAL(&wake_lock);
        result = gpio_wakeup_enable(BSP_BUTTON_PWR_GPIO, GPIO_INTR_HIGH_LEVEL);
        if (result != ESP_OK) break;
        wake_armed = true;
        result = gpio_isr_handler_add(BSP_BUTTON_PWR_GPIO, on_power_wake, nullptr);
        if (result != ESP_OK) break;
        wake_handler = true;
        result = esp_sleep_enable_gpio_wakeup();
        if (result != ESP_OK) break;
        status_requested = true;
        result = system_status::set_standby(true);
        if (result != ESP_OK) break;
        result = esp_lv_adapter_pause(2000);
        if (result != ESP_OK) break;
        adapter_paused = true;

        if (!gui.lock(2000)) {
            result = ESP_ERR_TIMEOUT;
            break;
        }
        // A user may have woken the screen or launched an app while the two
        // workers were suspending. Re-check after the LVGL worker has stopped.
        if (!standby_ready() || wake_pending()) {
            gui.unlock();
            result = ESP_ERR_INVALID_STATE;
            break;
        }
        result = bsp_extra_codec_dev_stop();
        if (result == ESP_OK) result = bsp_audio_suspend();
        // Applications have closed their files and released every lease. Keep
        // the internal volume's mount/eject preference intact across sleep.
        if (result == ESP_OK) {
            panel_attempted = true; // SLPIN may succeed even if DSTBON fails.
            result = screen_power::sleep_panel_for_standby();
        }
        gui.unlock();
        if (result != ESP_OK) break;

        system_status::Snapshot battery{};
        system_status::get_snapshot(battery);
        ESP_LOGI(TAG, "Entering light sleep: apps=0, radio/audio/LVGL stopped; battery=%d%%; POWER wakes",
                 battery.battery_valid ? battery.battery_percent : -1);
        if (!keys_released() || wake_pending()) {
            result = ESP_ERR_INVALID_STATE;
            break;
        }
        const int64_t before = esp_timer_get_time();
        result = esp_light_sleep_start();
        slept_us = esp_timer_get_time() - before;
        ESP_LOGI(TAG, "Light sleep returned: %s, duration=%lld ms, wake=%d",
                 esp_err_to_name(result), static_cast<long long>(slept_us / 1000),
                 static_cast<int>(esp_sleep_get_wakeup_cause()));
    } while (false);

    if (wake_handler) (void)gpio_isr_handler_remove(BSP_BUTTON_PWR_GPIO);
    if (wake_armed) {
        (void)gpio_wakeup_disable(BSP_BUTTON_PWR_GPIO);
        (void)gpio_set_intr_type(BSP_BUTTON_PWR_GPIO, GPIO_INTR_DISABLE);
        (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_GPIO);
    }
    // A persistent panel IO fault must not strand networking and the UI task.
    // Failed recovery leaves display refresh disabled; POWER and the worker
    // can retry it later while the other services have already resumed.
    if (panel_attempted || wake_pending()) {
        for (unsigned attempt = 0; attempt < 3; ++attempt) {
            bool restored = false;
            if (gui.lock(2000)) {
                restored = screen_power::wake_from_standby();
                gui.unlock();
            }
            if (restored) break;
            ESP_LOGE(TAG, "Panel wake incomplete (attempt %u/3)", attempt + 1);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
    if (adapter_paused) {
        const esp_err_t resumed = esp_lv_adapter_resume();
        if (resumed != ESP_OK) ESP_LOGE(TAG, "LVGL resume: %s", esp_err_to_name(resumed));
    }
    if (status_requested) {
        const esp_err_t resumed = system_status::set_standby(false);
        if (resumed != ESP_OK) ESP_LOGE(TAG, "Status/radio resume: %s", esp_err_to_name(resumed));
    }
    // Codec stays powered down until an audio app explicitly opens it again.
    if (result != ESP_OK) ESP_LOGW(TAG, "Standby aborted safely: %s", esp_err_to_name(result));
}

void worker(void *)
{
    auto &gui = esp_brookesia::gui::LvLock::getInstance();
    int64_t next_attempt_us = 0;
    for (;;) {
        bool ready = false;
        if (gui.lock(1000)) {
            if (screen_power::needs_standby_recovery() &&
                    esp_timer_get_time() >= next_attempt_us) {
                (void)screen_power::wake_from_standby();
                next_attempt_us = esp_timer_get_time() + STANDBY_RETRY_MS * 1000LL;
            }
            ready = standby_ready();
            gui.unlock();
        }
        if (ready && esp_timer_get_time() >= next_attempt_us) {
            enter_standby();
            next_attempt_us = esp_timer_get_time() + STANDBY_RETRY_MS * 1000LL;
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
} // namespace

esp_err_t start(esp_lcd_panel_handle_t panel, lv_display_t *display, Phone *phone)
{
    if (panel == nullptr || display == nullptr || phone == nullptr) return ESP_ERR_INVALID_ARG;
    if (state.task != nullptr) return ESP_ERR_INVALID_STATE;
    const esp_err_t isr_result = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (isr_result != ESP_OK && isr_result != ESP_ERR_INVALID_STATE) return isr_result;
    const esp_pm_config_t pm_config{
        .max_freq_mhz = 240,
        .min_freq_mhz = 80,
        .light_sleep_enable = false,
    };
    const esp_err_t pm_result = esp_pm_configure(&pm_config);
    if (pm_result != ESP_OK) {
        ESP_LOGW(TAG, "Dynamic frequency scaling unavailable: %s", esp_err_to_name(pm_result));
    }
    state.panel = panel;
    state.display = display;
    state.phone = phone;
    auto &gui = esp_brookesia::gui::LvLock::getInstance();
    if (!gui.lock(2000)) return ESP_ERR_TIMEOUT;
    const bool registered = phone->registerAppEventCallback(on_app_event, nullptr);
    if (registered) state.cleanup_timer = lv_timer_create(cleanup_tick, 1000, nullptr);
    if (registered && state.cleanup_timer == nullptr) {
        phone->unregisterAppEventCallback(on_app_event, nullptr);
    }
    gui.unlock();
    if (!registered || state.cleanup_timer == nullptr) return ESP_ERR_NO_MEM;
    if (xTaskCreate(worker, "power_manager", 6144, nullptr, 3, &state.task) != pdPASS) {
        state.task = nullptr;
        if (gui.lock(-1)) {
            lv_timer_delete(state.cleanup_timer);
            state.cleanup_timer = nullptr;
            phone->unregisterAppEventCallback(on_app_event, nullptr);
            gui.unlock();
        }
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "Policy ready: foreground idle=300s, background=120s, dark/empty standby after 5s");
    return ESP_OK;
}
} // namespace brookesia::power
