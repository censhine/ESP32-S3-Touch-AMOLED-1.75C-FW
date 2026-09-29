// Compile the actual transaction and worker helpers against deterministic APIs.
#include "../firmware/main/power_manager.cpp"
#include <algorithm>
#include <cstdlib>
#include <iostream>

namespace {
using namespace power_manager_test;
esp_brookesia::systems::phone::Phone phone;
lv_display_t display;
int checks = 0;
void require(bool ok, const char *message) {
    ++checks;
    if (!ok) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
int count(const char *call) { return std::count(f.calls.begin(), f.calls.end(), call); }
void before(const char *a, const char *b) {
    const auto first = std::find(f.calls.begin(), f.calls.end(), a);
    const auto second = std::find(f.calls.begin(), f.calls.end(), b);
    require(first != f.calls.end() && second != f.calls.end() && first < second,
            "preparation and recovery occur in the required order");
}
void reset(const char *failure = "") {
    f = Fake{}; f.fail = failure;
    brookesia::power::state = {};
    brookesia::power::state.phone = &phone;
    brookesia::power::state.display = &display;
    brookesia::power::wake_requested = false;
}
void run() {
    brookesia::power::enter_standby();
    require(!f.paused && !f.status_down, "transaction always releases successfully suspended services");
    require(!f.gpio_wake && !f.sleep_wake && f.isr == nullptr, "wake sources and ISR always disarm");
    require(!f.lock_error && f.locks == 0 && f.critical == 0, "blocking service calls occur outside GUI lock and locks balance");
    require(count("eject") == 0, "standby never ejects user storage");
}
void normal_transaction() {
    for (int mount : {STORAGE_SERVICE_STATE_MOUNTED, STORAGE_SERVICE_STATE_UNMOUNTED}) {
        reset(); f.mount = mount; run();
        require(count("sleep") == 1 && count("panel_wake") == 1, "normal transaction sleeps and restores exactly once");
        require(f.mount == mount, "normal sleep preserves mount/eject preference");
        before("gpio_arm", "status_down"); before("status_down", "pause");
        before("pause", "codec_stop"); before("codec_stop", "audio_suspend");
        before("audio_suspend", "panel_sleep"); before("panel_sleep", "sleep");
        before("sleep", "isr_remove"); before("isr_remove", "panel_wake");
        before("panel_wake", "resume"); before("resume", "status_up");
    }
}
void failed_preparation() {
    for (const char *failure : {"status_down", "pause", "lock", "codec_stop", "audio_suspend", "panel_sleep"}) {
        reset(failure); run();
        require(count("sleep") == 0, "every failed preparation prevents sleep");
        require(count("status_up") == 1, "even failed status suspension is unwound");
        require(count("resume") == (std::string(failure) == "status_down" || std::string(failure) == "pause" ? 0 : 1),
                "only a successfully paused adapter is resumed");
        require(count("panel_wake") == (std::string(failure) == "panel_sleep" ? 1 : 0),
                "partial panel sleep failure is restored, untouched panel is not woken");
    }
}
void app_becomes_active_during_suspend() {
    reset(); f.app_during_pause = true; run();
    require(f.running == 1 && count("sleep") == 0, "new running app aborts after suspension completes");
    require(count("codec_stop") == 0 && count("panel_sleep") == 0, "new app's audio and panel are not powered down");
    before("pause", "resume"); before("resume", "status_up");
}
void panel_wake_is_bounded() {
    reset("panel_wake"); run();
    require(count("sleep") == 1 && count("panel_wake") == 3, "persistent panel wake failure gets three attempts");
    require(count("delay") == 3 && f.recovery, "failed panel recovery stays pending after bounded retries");
    require(count("resume") == 1 && count("status_up") == 1, "panel fault cannot strand GUI or radio suspension");
    before("panel_wake", "resume");
}
void released_power_press_aborts_sleep() {
    reset(); f.power_during_panel = true; run();
    require(f.power == 0 && brookesia::power::wake_pending(), "ISR latches short POWER press even after release");
    require(count("isr_disable") == 1 && count("sleep") == 0, "latched POWER press suppresses light sleep");
    require(count("panel_wake") == 1, "cancelled sleep restores the prepared panel");
}
void readiness_protects_active_resources() {
    reset(); require(brookesia::power::standby_ready(), "idle dark device is ready");
    f.leases = 1; require(!brookesia::power::standby_ready(), "active media lease prevents sleep"); f.leases = 0;
    for (int mount : {STORAGE_SERVICE_STATE_MOUNTING, STORAGE_SERVICE_STATE_EJECTING}) {
        f.mount = mount; require(!brookesia::power::standby_ready(), "storage transition prevents sleep");
    }
    f.mount = STORAGE_SERVICE_STATE_MOUNTED; f.owner = 1;
    require(!brookesia::power::standby_ready(), "active audio owner prevents sleep"); f.owner = 0;
    f.recovery = true; require(!brookesia::power::standby_ready(), "pending panel recovery prevents another sleep");
    f.recovery = false; f.dark_ms = 4999; require(!brookesia::power::standby_ready(), "dark settle interval is respected");
    f.dark_ms = 5000; require(brookesia::power::standby_ready(), "dark settle exact boundary is accepted");
}
void cleanup_runs_in_lvgl_context() {
    reset();
    require(brookesia::power::start(reinterpret_cast<void *>(1), &display, &phone) == ESP_OK,
            "power service starts");
    auto *timer = brookesia::power::state.cleanup_timer;
    require(timer != nullptr && timer->period == 1000, "cleanup uses a one-second LVGL timer");
    f.running = 1; f.idle = 0; f.now = 0;
    auto &gui = esp_brookesia::gui::LvLock::getInstance();
    require(gui.lock(1000), "simulate LVGL callback lock");
    timer->callback(timer);
    f.now = f.idle = 299999; timer->callback(timer);
    require(f.running == 1, "foreground survives before five-minute deadline");
    f.now = f.idle = 300000; timer->callback(timer);
    require(f.running == 0 && count("app_stop") == 1, "cleanup timer dispatches normal STOP at deadline");
    gui.unlock();
}
}
int main() {
    normal_transaction(); failed_preparation(); app_becomes_active_during_suspend();
    panel_wake_is_bounded(); released_power_press_aborts_sleep(); readiness_protects_active_resources();
    cleanup_runs_in_lvgl_context();
    std::cout << "PASS: " << checks << " real power manager checks\n";
}
