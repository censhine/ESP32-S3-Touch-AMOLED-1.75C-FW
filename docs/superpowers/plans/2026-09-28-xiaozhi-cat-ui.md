# Xiaozhi Cat UI Implementation Plan

> For agentic workers: use subagent-driven-development for the independent avatar and layout tasks; root owns integration, verification and flashing.

**Goal:** Implement the user-approved warm cream 466×466 round-screen UI with an expressive ginger kitten, nonoverlapping Wi-Fi/status, and readable three-line paged subtitles, then build and flash the application.

**Architecture:** Keep voice/network state in XiaozhiApp. XiaozhiUi owns layout, pagination and avatar updates. XiaozhiCatAvatar uses flash-resident raster layers drawn from the approved SVG plus small moving eye/mouth layers; no full-screen frame animation or new image decoder is required.

**Tech Stack:** ESP-IDF 5.5.5, LVGL 9.4.0, C++17, native LVGL interaction/render tests, RGB565A8 generated art assets.

## Global Constraints
- Approved design: /Users/apple/.codex/visualizations/2026/09/28/xiaozhi-cat-design/design-board.png, expressions.png and design-notes.md.
- User approved design and previously requested code changes and flashing after approval; no additional approval gate remains.
- Start at becc819; preserve existing power-button, idle-sleep, audio, Wi-Fi and activation behavior.
- Application-only flash at existing factory partition; do not rewrite NVS, model, SPIFFS or media partitions.
- Existing full Chinese font must remain available for arbitrary replies. Avoid adding another full CJK font to flash; render existing 30px glyphs at approximately 24px through an LVGL label transform if required.
- Avatar and subtitle updates run under the existing LVGL lock; destroy timers before deleting view objects; pause animations when the app is hidden.

## Task 1: Avatar renderer and original assets
Files: create firmware/components/XiaozhiApp/XiaozhiCatAvatar.hpp/.cpp, assets/cat_avatar_assets.c/.h, tools/generate-cat-assets.py (plus SVG sources when useful), tests/xiaozhi_cat_tests.cpp.
Interface in namespace esp_brookesia::apps:
```cpp
enum class XiaozhiCatActivity { Idle, Listening, Thinking, Speaking, Sleeping, Error };
class XiaozhiCatAvatar {
public:
    bool create(lv_obj_t *parent); // fixed 280×224, caller positions root
    void destroy();
    void setActivity(XiaozhiCatActivity activity);
    void setEmotion(const char *name);
    void setPaused(bool paused);
    lv_obj_t *root() const;
};
```
- [ ] Generate layered art matching the approved cat, with open/blink/happy eye states, movable green irises, mouth open/closed and cream muzzle.
- [ ] Implement activity/emotion mapping and low-frequency state changes, eye saccades and blinking with restrained head tilt; target at most 20fps, avoid rerasterizing the entire display.
- [ ] Test activity changes, destroy/recreate, parent-first deletion, animation pause, unsupported emotion fallback, and capture native render previews.

## Task 2: Layout and subtitle pagination
Files: modify firmware/components/XiaozhiApp/XiaozhiUi.hpp/.cpp; create XiaozhiSubtitlePager.hpp/.cpp if needed; tests/xiaozhi_ui_tests.cpp and host stubs.
Consumes avatar API above. Root will add source lists to CMake and wire app activity.
Additional public UI interfaces: setActivity(XiaozhiCatActivity), setPaused(bool).
- [ ] Warm cream full-screen background; compact green status pill, Wi-Fi in a separate region and short stable status text. No marquee or overlapping status glyphs.
- [ ] Place cat at x93,y85 in a 280×224 drawing region; preserve activation messages as a separate mode.
- [ ] Render full Chinese subtitles at approximately24px using existing font, three complete lines in a fixed safe rectangle around x83,y307,w300,h96. Use real glyph measurements for UTF-8 wrapping and page length.
- [ ] New distinct text starts at page one; repeated identical refreshes do not reset pagination. Long text remains accessible via timed page advance and touch-to-advance/review; hide page indicator for one page. Respect sentence boundaries where possible and preserve all Unicode content.
- [ ] Native LVGL tests verify icon/status disjointness, round-safe text bounds, three complete lines, long mixed CJK/Latin text, repeated refreshes, activation visibility and destroy/recreate.

## Task 3: App integration, review and device delivery
Files: firmware/components/XiaozhiApp/XiaozhiApp.cpp, both CMakeLists, docs/XIAOZHI_CAT_UI_ZH.md.
- [ ] Map existing typed State to avatar activity; setPaused on app pause/resume, without changing network/audio states.
- [ ] Add sources and native LVGL test executables. Run baseline and final relevant tests and render screenshots of actual firmware widgets.
- [ ] Build ESP-IDF firmware, inspect size (8MiB application ceiling) and image validity. Run independent review of lifecycle, pagination and resource footprint; fix confirmed findings.
- [ ] Keep new firmware and a copy of previous application artifact with hashes. Verify connected device identity and existing partition layout, use app-flash, then inspect boot log for startup and crash/reset errors.
- [ ] Report actual verification limits: boot logs and native renders do not replace visual/hardware interaction feedback. Leave reviewable source changes and precise build/flash records.
