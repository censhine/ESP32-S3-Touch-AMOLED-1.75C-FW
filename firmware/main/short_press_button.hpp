#pragma once

#include <cstdint>

namespace brookesia::screen_power {

class ShortPressButton {
public:
    // The caller supplies logical pressed state after applying GPIO polarity.
    bool update(bool pressed, uint32_t now_ms, bool inhibited = false)
    {
        if (inhibited) {
            initialized_ = false;
            return false;
        }

        if (!initialized_) {
            initialized_ = true;
            candidate_pressed_ = pressed;
            candidate_since_ms_ = now_ms;
            // Treat startup and the end of inhibition as an unarmed hold.
            // Only a subsequent stable release can arm the next press.
            stable_pressed_ = true;
            armed_ = false;
            return false;
        }

        if (pressed != candidate_pressed_) {
            candidate_pressed_ = pressed;
            candidate_since_ms_ = now_ms;
        }
        if (candidate_pressed_ == stable_pressed_ ||
            static_cast<uint32_t>(now_ms - candidate_since_ms_) < 30) {
            return false;
        }

        stable_pressed_ = candidate_pressed_;
        if (stable_pressed_) {
            // Use the accepted edge's start so polling delays do not shorten
            // a long hold. Unsigned subtraction also handles clock wrap.
            pressed_since_ms_ = candidate_since_ms_;
            return false;
        }

        const bool clicked = armed_ &&
            static_cast<uint32_t>(candidate_since_ms_ - pressed_since_ms_) < 1000;
        armed_ = true;
        return clicked;
    }

private:
    uint32_t candidate_since_ms_ = 0;
    uint32_t pressed_since_ms_ = 0;
    bool initialized_ = false;
    bool candidate_pressed_ = false;
    bool stable_pressed_ = true;
    bool armed_ = false;
};

} // namespace brookesia::screen_power
