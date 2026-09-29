#include "system_status.hpp"
#include "test_support.hpp"

#include <cstdlib>
#include <future>
#include <iostream>

namespace {
using namespace system_status_test;
namespace status = brookesia::system_status;
int checks = 0;
void require(bool condition, const char *message) {
    ++checks;
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
template <typename Predicate> void wait_for(Predicate predicate, const char *message) {
    for (int i = 0; i < 100 && !predicate(); ++i) vTaskDelay(10);
    require(predicate(), message);
}
}

int main() {
    require(status::set_standby(true) == ESP_ERR_INVALID_STATE, "unstarted monitor rejects standby");
    esp_brookesia::systems::phone::StatusBar bar;
    require(status::start(&bar, 100) == ESP_OK, "monitor starts");
    wait_for([] { return ui_updates > 0; }, "monitor polls and updates GUI");
    require(radio_started && saved_wifi_enabled, "saved Wi-Fi preference starts radio");

    require(status::set_standby(true) == ESP_OK, "standby entry succeeds");
    require(!radio_started, "standby stops radio");
    const int old_reads = battery_reads, old_updates = ui_updates, old_connects = connects;
    emit_wifi(WIFI_EVENT_STA_DISCONNECTED);
    vTaskDelay(220);
    require(battery_reads == old_reads && ui_updates == old_updates, "standby stops PMU polling and GUI work");
    require(connects == old_connects, "disconnect event does not reconnect while asleep");
    status::Snapshot snapshot;
    require(status::get_snapshot(snapshot) && snapshot.wifi_enabled && !snapshot.wifi_connected,
            "snapshot retains desired Wi-Fi with disconnected radio");
    require(status::set_wifi_enabled(false) == ESP_ERR_INVALID_STATE, "standby rejects preference changes");
    require(status::connect_wifi({}) == ESP_ERR_INVALID_STATE, "standby rejects STA changes");
    require(status::request_wifi_connect() == ESP_ERR_INVALID_STATE, "standby rejects reconnect requests");
    const int old_stops = stops;
    require(status::set_standby(true) == ESP_OK && stops == old_stops, "repeated entry is idempotent");
    require(status::set_standby(false) == ESP_OK && radio_started, "wake restores radio");
    wait_for([=] { return battery_reads > old_reads; }, "wake resumes monitoring");
    require(nvs_writes == 0 && saved_wifi_enabled, "sleep and wake never write preference to NVS");

    fail_stop = true;
    require(status::set_standby(true) == ESP_FAIL, "radio stop failure aborts standby");
    require(status::request_wifi_connect() == ESP_OK, "failed entry rolls back API suspension");
    const int before_rollback_reads = battery_reads;
    wait_for([=] { return battery_reads > before_rollback_reads; }, "failed entry resumes monitor");

    {
        std::unique_lock<std::mutex> lock(read_mutex);
        block_battery_read = true;
        require(read_condition.wait_for(lock, std::chrono::seconds(1), [] { return battery_read_waiting; }),
                "a PMU transaction is in progress");
    }
    auto pending_entry = std::async(std::launch::async, [] { return status::set_standby(true); });
    require(pending_entry.wait_for(std::chrono::milliseconds(80)) == std::future_status::timeout,
            "entry waits for an active PMU transaction");
    {
        std::lock_guard<std::mutex> lock(read_mutex);
        block_battery_read = false;
        read_condition.notify_all();
    }
    require(pending_entry.get() == ESP_OK, "entry finishes after PMU transaction releases");
    fail_start = true;
    require(status::set_standby(false) == ESP_FAIL, "wake reports transient radio restart failure");
    wait_for([] { return radio_started.load(); }, "normal monitor retries failed radio restart");

    gui_mutex.lock();
    vTaskDelay(130);
    auto gui_blocked_entry = std::async(std::launch::async, [] { return status::set_standby(true); });
    const bool entered = gui_blocked_entry.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    gui_mutex.unlock();
    require(entered && gui_blocked_entry.get() == ESP_OK,
            "standby interrupts monitor's GUI lock wait without deadlock");
    require(status::stop() == ESP_OK, "stop wakes a monitor blocked in standby");
    for (auto *task : test_tasks) { task->thread.join(); delete task; }
    test_tasks.clear();

    saved_wifi_enabled = false;
    const int before_disabled_start = starts;
    require(status::start(&bar, 100) == ESP_OK, "monitor restarts with Wi-Fi disabled");
    require(status::set_standby(true) == ESP_OK && status::set_standby(false) == ESP_OK,
            "sleep cycle works with Wi-Fi disabled");
    require(starts == before_disabled_start && !radio_started, "wake honors disabled Wi-Fi preference");
    require(nvs_writes == 0, "all standby paths avoid NVS writes");
    require(status::stop() == ESP_OK, "monitor stops after wake");
    for (auto *task : test_tasks) { task->thread.join(); delete task; }
    std::cout << checks << " system standby checks passed\n";
}
