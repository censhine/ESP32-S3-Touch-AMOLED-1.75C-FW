#include "app_idle_policy.hpp"

#include <climits>
#include <cstdlib>
#include <iostream>

using brookesia::power::AppIdlePolicy;

namespace {

int checks = 0;

void require(bool condition, const char *message)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

bool scan(AppIdlePolicy &policy, int id, bool foreground,
          uint32_t now_ms, uint32_t user_idle_ms)
{
    policy.begin_scan();
    const bool due = policy.observe(id, foreground, now_ms, user_idle_ms);
    policy.end_scan();
    return due;
}

void test_foreground_exact_boundary_and_fresh_residency()
{
    AppIdlePolicy policy;
    require(AppIdlePolicy::FOREGROUND_TIMEOUT_MS == 300000,
            "foreground timeout is five minutes");
    require(!scan(policy, 0, true, 1000, 900000),
            "new foreground app cannot inherit old global inactivity");
    require(!scan(policy, 0, true, 300999, 900000),
            "foreground remains open one millisecond before residency timeout");
    require(scan(policy, 0, true, 301000, 300000),
            "foreground is due at both exact five-minute boundaries");
}

void test_touch_restarts_foreground_inactivity()
{
    AppIdlePolicy policy;
    require(!scan(policy, 17, true, 0, 0), "foreground starts fresh");
    require(!scan(policy, 17, true, 299999, 299999), "foreground waits for timeout");
    require(!scan(policy, 17, true, 300000, 0),
            "touch at residency deadline keeps foreground open");
    require(!scan(policy, 17, true, 599999, 299999),
            "foreground waits a full five minutes after last touch");
    require(scan(policy, 17, true, 600000, 300000),
            "foreground closes at inactivity deadline after touch");
}

void test_background_apps_have_independent_windows()
{
    AppIdlePolicy policy;
    require(AppIdlePolicy::BACKGROUND_TIMEOUT_MS == 120000,
            "background timeout is two minutes");
    require(!scan(policy, 1, false, 0, 900000),
            "new background app does not inherit global idle time");

    policy.begin_scan();
    require(!policy.observe(1, false, 60000, 0), "first background app remains open");
    require(!policy.observe(2, false, 60000, 0), "second background app starts fresh");
    policy.end_scan();

    policy.begin_scan();
    require(!policy.observe(1, false, 119999, 0), "background waits up to exact deadline");
    require(!policy.observe(2, false, 119999, 0), "younger background app remains open");
    policy.end_scan();

    policy.begin_scan();
    require(policy.observe(1, false, 120000, 0),
            "continuous foreground interaction does not defer background cleanup");
    require(!policy.observe(2, false, 120000, 0),
            "first deadline does not close the younger background app");
    policy.end_scan();

    require(!scan(policy, 2, false, 179999, 0), "younger app waits its own full window");
    require(scan(policy, 2, false, 180000, 0), "younger app reaches its own deadline");
}

void test_role_switches_restart_residency()
{
    AppIdlePolicy policy;
    require(!scan(policy, 5, true, 0, 0), "role-switch setup");
    require(!scan(policy, 5, false, 200000, 200000),
            "moving to background starts a new two-minute window");
    require(!scan(policy, 5, false, 319999, 319999), "background role waits until deadline");
    require(scan(policy, 5, false, 320000, 320000), "background role reaches deadline");
    policy.close_failed(5, 320000);
    require(!scan(policy, 5, true, 320001, 900000),
            "returning to foreground starts a full window after failed cleanup");
    require(!scan(policy, 5, true, 620000, 900000), "foreground role waits until deadline");
    require(scan(policy, 5, true, 620001, 900000), "foreground role reaches its new deadline");
    policy.close_failed(5, 620001);
    require(!scan(policy, 5, false, 620002, 0), "returning to background starts fresh again");
    require(!scan(policy, 5, false, 740001, 0), "second background window is complete");
    require(scan(policy, 5, false, 740002, 0), "second background role reaches deadline");
}

void test_failed_close_retries_without_a_storm()
{
    AppIdlePolicy policy;
    require(AppIdlePolicy::CLOSE_RETRY_MS == 30000, "failed close retries after thirty seconds");
    policy.close_failed(9, 0);
    require(!scan(policy, 9, false, 0, 0), "unknown failed close does not create an entry");
    require(scan(policy, 9, false, 120000, 0), "first close is due");
    policy.close_failed(9, 120000);
    for (uint32_t delta : {1U, 1000U, 10000U, 29999U}) {
        require(!scan(policy, 9, false, 120000 + delta, 0),
                "failed close never retries inside the backoff interval");
    }
    require(scan(policy, 9, false, 150000, 0), "close retries at exact backoff boundary");
    policy.close_failed(9, 150000);
    require(!scan(policy, 9, false, 179999, 0), "second failure starts its own backoff");
    require(scan(policy, 9, false, 180000, 0), "second retry reaches its own boundary");
}

void test_foreground_activity_after_failed_close()
{
    AppIdlePolicy policy;
    require(!scan(policy, 12, true, 0, 0), "foreground retry setup");
    require(scan(policy, 12, true, 300000, 300000), "foreground initially due");
    policy.close_failed(12, 300000);
    require(!scan(policy, 12, true, 330000, 0),
            "retry deadline does not override fresh foreground user activity");
    require(!scan(policy, 12, true, 629999, 299999), "touched foreground still waits");
    require(scan(policy, 12, true, 630000, 300000), "touched foreground becomes due later");
}

void test_absent_app_is_forgotten_and_reopen_is_fresh()
{
    AppIdlePolicy policy;
    require(!scan(policy, 3, false, 0, 0), "reopen setup");
    require(scan(policy, 3, false, 120000, 0), "old instance is due");
    policy.close_failed(3, 120000);
    policy.begin_scan();
    policy.end_scan();
    require(!scan(policy, 3, false, 120001, 900000),
            "app absent from one complete scan reopens with fresh timing");
    require(!scan(policy, 3, false, 240000, 900000), "reopened app waits its full window");
    require(scan(policy, 3, false, 240001, 900000), "reopened app reaches its own deadline");
}

void test_reset_clears_old_timing_and_failures()
{
    AppIdlePolicy policy;
    require(!scan(policy, INT_MAX, false, 0, 0), "maximum nonnegative id is supported");
    require(scan(policy, INT_MAX, false, 120000, 0), "maximum id tracks a deadline");
    policy.close_failed(INT_MAX, 120000);
    policy.reset();
    require(!scan(policy, INT_MAX, false, 120001, 0), "reset starts a new full window");
    require(!scan(policy, INT_MAX, false, 240000, 0), "reset does not retain an old deadline");
    require(scan(policy, INT_MAX, false, 240001, 0), "reset entry reaches fresh deadline");
}

void test_forget_allows_immediate_same_id_reopen()
{
    AppIdlePolicy policy;
    policy.begin_scan();
    require(!policy.observe(7, true, 0, 0), "immediate reopen foreground setup");
    require(!policy.observe(8, false, 0, 0), "immediate reopen neighbor setup");
    policy.end_scan();

    policy.begin_scan();
    require(policy.observe(7, true, 300000, 300000), "old foreground lifetime is due");
    policy.close_failed(7, 300000);
    policy.forget(7);
    policy.forget(-1);
    policy.forget(99);
    require(!policy.observe(7, true, 300000, 900000),
            "forget allows same-id reopen immediately without an absent scan");
    require(policy.observe(8, false, 300000, 0),
            "forget and unknown ids preserve other applications' timing");
    policy.end_scan();

    require(!scan(policy, 7, true, 599999, 900000),
            "immediate reopen waits its complete new residency window");
    require(scan(policy, 7, true, 600000, 900000),
            "immediate reopen reaches its new residency deadline");
}

void test_residency_and_retry_survive_clock_rollover()
{
    AppIdlePolicy policy;
    constexpr uint32_t start = UINT32_MAX - 60000U;
    require(!scan(policy, 4, false, start, 0), "background rollover setup");
    require(!scan(policy, 4, false, start + 119999U, 0), "background rollover before deadline");
    require(scan(policy, 4, false, start + 120000U, 0), "background rollover at deadline");

    policy.reset();
    require(!scan(policy, 4, true, start, 0), "foreground rollover setup");
    require(!scan(policy, 4, true, start + 299999U, 299999), "foreground rollover before deadline");
    require(scan(policy, 4, true, start + 300000U, 300000), "foreground rollover at deadline");

    policy.reset();
    constexpr uint32_t failed_at = UINT32_MAX - 10000U;
    require(!scan(policy, 4, false, failed_at - 120000U, 0), "retry rollover setup");
    require(scan(policy, 4, false, failed_at, 0), "close fails before clock rollover");
    policy.close_failed(4, failed_at);
    require(!scan(policy, 4, false, failed_at + 29999U, 0), "backoff spans clock rollover");
    require(scan(policy, 4, false, failed_at + 30000U, 0), "retry resumes at wrapped deadline");
}

void test_capacity_overflow_keeps_existing_apps_safe()
{
    AppIdlePolicy policy;
    policy.begin_scan();
    require(!policy.observe(-1, false, 0, 0), "negative id cannot occupy a slot");
    policy.close_failed(-1, 0);
    policy.close_failed(99, 0);
    for (int id = 0; id < 32; ++id) {
        require(!policy.observe(id, false, 0, 900000), "each capacity slot starts fresh");
    }
    require(!policy.observe(32, false, 0, 900000), "overflow never forces cleanup");
    policy.end_scan();

    policy.begin_scan();
    for (int id = 0; id < 32; ++id) {
        require(!policy.observe(id, false, 119999, 900000), "overflow preserves existing deadlines");
    }
    require(!policy.observe(32, false, 119999, 900000), "overflow remains untracked and safe");
    policy.end_scan();

    policy.begin_scan();
    for (int id = 0; id < 32; ++id) {
        require(policy.observe(id, false, 120000, 0), "every tracked app retains its exact deadline");
    }
    require(!policy.observe(32, false, 120000, 900000), "overflow is never marked due");
    policy.end_scan();

    policy.begin_scan();
    for (int id = 1; id < 32; ++id) {
        require(policy.observe(id, false, 120001, 0), "remaining applications stay tracked");
    }
    policy.end_scan();

    policy.begin_scan();
    require(!policy.observe(32, false, 120002, 900000), "forgotten application frees capacity");
    for (int id = 1; id < 32; ++id) {
        require(policy.observe(id, false, 120002, 0), "admitting new app does not evict tracked apps");
    }
    policy.end_scan();
    require(!scan(policy, 32, false, 240001, 900000), "newly admitted overflow app gets full window");
    require(scan(policy, 32, false, 240002, 900000), "newly admitted app reaches its own deadline");
}

} // namespace

int main()
{
    test_foreground_exact_boundary_and_fresh_residency();
    test_touch_restarts_foreground_inactivity();
    test_background_apps_have_independent_windows();
    test_role_switches_restart_residency();
    test_failed_close_retries_without_a_storm();
    test_foreground_activity_after_failed_close();
    test_absent_app_is_forgotten_and_reopen_is_fresh();
    test_reset_clears_old_timing_and_failures();
    test_forget_allows_immediate_same_id_reopen();
    test_residency_and_retry_survive_clock_rollover();
    test_capacity_overflow_keeps_existing_apps_safe();
    std::cout << "PASS: " << checks << " app idle policy checks\n";
}
