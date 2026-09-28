# Wi-Fi Keyboard Implementation Plan

**Goal:** Replace cramped Settings Wi-Fi keyboard with the selected two-page large-key editor.
**Architecture:** LVGL-only WifiPasswordKeyboard owns its overlay and callbacks. WlanPage owns it via unique_ptr and retains its resident worker and Wi-Fi state machine.
**Tech Stack:** C++17, LVGL 9.4, ESP-IDF 5.5.5, native CMake tests.

## Constraints
- Preserve existing applications and Wi-Fi/NVS worker behavior.
- 466px circular geometry, including top-cropped app page.
- Alphabet in two pages of 13, 5 columns and fixed space/delete keys; printable ASCII available.
- 64×50px character keys, 28px labels, release-to-input with no repeated character on hold.
- Hide/reset draft on cancel/close/open; do not print passwords in logs.

## Task 1: LVGL editor
- [x] Add `firmware/components/Settings/ui/WifiPasswordKeyboard.hpp/.cpp` with constructor(parent, submit, cancel, context), show(ssid), hide(), visible(). Submit callback receives const char* while textarea still exists; caller copies before hide.
- [x] Layout top title, input/eye, modes/page, 15-key grid, cancel/connect inside safe circle. Account for parent top crop.
- [x] Implement lowercase/uppercase, digits, all symbols, space, delete, hidden/show and length feedback.

## Task 2: WlanPage integration
- [x] Own editor using unique_ptr; destroy before parent screen.
- [x] Replace old textarea/keyboard and callbacks with editor submit/cancel. Keep CONNECTING and wifi_task unchanged.
- [x] Back cancels draft first; close destroys editor; async list refresh may hide/reset editor through one API.

## Task 3: Verification
- [x] Native target links same production editor to existing LVGL.
- [x] Real pointer tests: type mixed password, change pages/mode, delete, show/hide, invalid short submit, valid submit, max length, drag out cancellation, long-hold single input, cancel/reopen.
- [x] Check all buttons against 223px circular bound for 466×466 and cropped pages; repeat lifecycle and inspect rendered images.
- [x] Run native suite and complete firmware build, inspect diff and independent code review, document pending physical verification.
