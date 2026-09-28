#include "short_press_button.hpp"

#include <cstdlib>
#include <iostream>

using brookesia::screen_power::ShortPressButton;

namespace {

int checks = 0;

void require(bool condition, const char* message)
{
    ++checks;
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void arm(ShortPressButton& button)
{
    require(!button.update(false, 0), "initial release does not click");
    require(!button.update(false, 29), "initial release waits for debounce");
    require(!button.update(false, 30), "stable initial release arms without clicking");
}

void test_short_press_clicks_once_on_stable_release()
{
    ShortPressButton button;
    arm(button);
    require(!button.update(true, 100), "press edge does not click");
    require(!button.update(true, 129), "press debounce does not click");
    require(!button.update(true, 130), "stable press does not click");
    require(!button.update(false, 200), "release edge does not click");
    require(!button.update(false, 229), "release waits the full 30 ms");
    require(button.update(false, 230), "short press clicks at stable release");
    require(!button.update(false, 231), "release click is emitted only once");
    require(!button.update(false, 500), "idle release does not repeat click");
    require(!button.update(true, 600), "second press edge does not click");
    require(!button.update(true, 630), "second stable press does not click");
    require(!button.update(false, 700), "second release edge does not click");
    require(button.update(false, 730), "next short press clicks independently");
}

void test_contact_bounce_does_not_create_extra_clicks()
{
    ShortPressButton button;
    arm(button);
    require(!button.update(true, 100), "brief press begins");
    require(!button.update(false, 110), "press bounce releases");
    require(!button.update(true, 120), "press bounce restarts debounce");
    require(!button.update(true, 149), "29 ms press is not stable");
    require(!button.update(false, 159), "unaccepted press returns to release");
    require(!button.update(false, 189), "brief press never clicks");
    require(!button.update(true, 200), "valid press begins");
    require(!button.update(true, 230), "valid press becomes stable");
    require(!button.update(false, 300), "release bounce begins");
    require(!button.update(true, 310), "release bounce returns to pressed");
    require(!button.update(false, 320), "release debounce restarts");
    require(!button.update(false, 349), "29 ms release is not stable");
    require(button.update(false, 350), "bounced short press clicks once");
    require(!button.update(false, 380), "bounce does not produce a second click");
}

void test_short_press_threshold_uses_debounced_edges()
{
    for (uint32_t duration : {999U, 1000U, 5000U}) {
        ShortPressButton button;
        arm(button);
        require(!button.update(true, 100), "threshold press begins");
        require(!button.update(true, 150), "late sample accepts stable press");
        require(!button.update(false, 100 + duration), "threshold release begins");
        require(button.update(false, 130 + duration) == (duration == 999),
                "only a hold strictly shorter than 1000 ms clicks");
        require(!button.update(false, 160 + duration), "threshold release never repeats");
    }
}

void test_long_hold_never_clicks_and_next_short_press_works()
{
    ShortPressButton button;
    arm(button);
    require(!button.update(true, 100), "long press begins");
    require(!button.update(true, 130), "long press becomes stable");
    require(!button.update(true, 1100), "long press threshold does not click");
    require(!button.update(true, 5100), "holding button does not repeat");
    require(!button.update(false, 5200), "long press release begins");
    require(!button.update(false, 5230), "long press release never clicks");
    require(!button.update(true, 5300), "short press after long hold begins");
    require(!button.update(true, 5330), "short press after long hold becomes stable");
    require(!button.update(false, 5400), "short press after long hold releases");
    require(button.update(false, 5430), "long hold does not prevent future short clicks");
}

void test_startup_held_button_requires_a_stable_release()
{
    ShortPressButton button;
    require(!button.update(true, 0), "boot while pressed does not click");
    require(!button.update(true, 30), "boot press is not armed");
    require(!button.update(false, 100), "boot release begins");
    require(!button.update(true, 110), "boot release bounce cannot arm");
    require(!button.update(true, 140), "boot hold remains ignored after bounce");
    require(!button.update(false, 200), "boot release restarts");
    require(!button.update(false, 229), "boot release waits for debounce");
    require(!button.update(false, 230), "boot release arms without clicking");
    require(!button.update(true, 300), "first deliberate press begins");
    require(!button.update(true, 330), "first deliberate press becomes stable");
    require(!button.update(false, 400), "first deliberate release begins");
    require(button.update(false, 430), "first deliberate short press clicks");
}

void test_inhibition_cancels_an_active_press()
{
    ShortPressButton button;
    arm(button);
    require(!button.update(true, 100), "press before inhibition begins");
    require(!button.update(true, 130), "press before inhibition becomes stable");
    require(!button.update(true, 150, true), "inhibition cancels active press");
    require(!button.update(true, 170), "held button after inhibition remains ignored");
    require(!button.update(false, 200), "cancelled press releases");
    require(!button.update(false, 230), "cancelled press never clicks after inhibition");
    require(!button.update(true, 300), "new press after inhibition begins");
    require(!button.update(true, 330), "new press after inhibition becomes stable");
    require(!button.update(false, 400), "new press after inhibition releases");
    require(button.update(false, 430), "new deliberate press after inhibition clicks");
}

void test_press_started_during_inhibition_cannot_click_afterwards()
{
    ShortPressButton button;
    arm(button);
    require(!button.update(false, 100, true), "inhibited idle does not click");
    require(!button.update(true, 130, true), "inhibited press edge does not click");
    require(!button.update(true, 160, true), "inhibited stable press does not click");
    require(!button.update(false, 200), "release after inhibition is not a click");
    require(!button.update(false, 230), "stable release after inhibition only arms");
    require(!button.update(true, 300), "new uninhibited press begins");
    require(!button.update(true, 330), "new uninhibited press becomes stable");
    require(!button.update(false, 400), "new uninhibited release begins");
    require(button.update(false, 430), "new uninhibited press clicks");
}

void test_click_ready_during_inhibition_is_discarded()
{
    ShortPressButton button;
    arm(button);
    require(!button.update(true, 100), "press before inhibited release begins");
    require(!button.update(true, 130), "press before inhibited release becomes stable");
    require(!button.update(false, 200), "release before inhibition begins");
    require(!button.update(false, 230, true), "inhibition discards a ready release click");
    require(!button.update(false, 260), "discarded click is not delayed until inhibition ends");
    require(!button.update(false, 290), "stable idle after inhibition never clicks");
}

void test_debounce_and_hold_duration_survive_clock_wrap()
{
    ShortPressButton button;
    require(!button.update(false, 0xffffff90U), "release before clock wrap begins");
    require(!button.update(false, 0xffffffaeU), "release before clock wrap arms");
    require(!button.update(true, 0xfffffff0U), "press before clock wrap begins");
    require(!button.update(true, 0x0dU), "29 ms across clock wrap is not stable");
    require(!button.update(true, 0x0eU), "30 ms across clock wrap accepts press");
    require(!button.update(false, 0x54U), "100 ms hold spanning clock wrap releases");
    require(button.update(false, 0x72U), "short hold spanning clock wrap clicks");

    ShortPressButton long_button;
    require(!long_button.update(false, 0xffffff90U), "long hold clock wrap setup begins");
    require(!long_button.update(false, 0xffffffaeU), "long hold clock wrap setup arms");
    require(!long_button.update(true, 0xfffffff0U), "long hold before clock wrap begins");
    require(!long_button.update(true, 0x0eU), "long hold across clock wrap becomes stable");
    require(!long_button.update(false, 0x3d8U), "1000 ms hold across clock wrap releases");
    require(!long_button.update(false, 0x3f6U), "1000 ms hold across clock wrap never clicks");
}

} // namespace

int main()
{
    test_short_press_clicks_once_on_stable_release();
    test_contact_bounce_does_not_create_extra_clicks();
    test_short_press_threshold_uses_debounced_edges();
    test_long_hold_never_clicks_and_next_short_press_works();
    test_startup_held_button_requires_a_stable_release();
    test_inhibition_cancels_an_active_press();
    test_press_started_during_inhibition_cannot_click_afterwards();
    test_click_ready_during_inhibition_is_discarded();
    test_debounce_and_hold_duration_survive_clock_wrap();
    std::cout << "PASS: " << checks << " power button checks\n";
}
