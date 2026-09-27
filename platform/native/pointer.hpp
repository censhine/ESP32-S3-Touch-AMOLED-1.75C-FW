#pragma once

namespace roundwing {
// Accept or reject a whole physical press at its origin. Moving across the
// round display edge must not manufacture another press while still held.
struct PointerState {
    int x = 233, y = 233;
    bool held = false, accepted = false;

    void update(int next_x, int next_y, bool down) {
        x = next_x;
        y = next_y;
        if (down && !held) {
            const int dx = x - 233, dy = y - 233;
            accepted = dx * dx + dy * dy <= 233 * 233;
        }
        if (!down) accepted = false;
        held = down;
    }
};
} // namespace roundwing
