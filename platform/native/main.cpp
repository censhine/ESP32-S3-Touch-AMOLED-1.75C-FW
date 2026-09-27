#include "game_view.hpp"
#include "pointer.hpp"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace {
constexpr int kSize = 466;
struct Native {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    SDL_Texture* texture = nullptr;
    bool quit = false;
    roundwing::PointerState pointer;
    std::filesystem::path save_path;
};

uint32_t ticks() { return SDL_GetTicks(); }

void flush(lv_display_t* display, const lv_area_t*, uint8_t* pixels) {
    auto* native = static_cast<Native*>(lv_display_get_user_data(display));
    SDL_UpdateTexture(native->texture, nullptr, pixels, kSize * 4);
    SDL_RenderClear(native->renderer);
    SDL_RenderCopy(native->renderer, native->texture, nullptr, nullptr);
    SDL_RenderPresent(native->renderer);
    lv_display_flush_ready(display);
}

void pointer_read(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* native = static_cast<Native*>(lv_indev_get_user_data(indev));
    data->point.x = native->pointer.x;
    data->point.y = native->pointer.y;
    data->state = native->pointer.accepted
                      ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

int read_progress(const std::filesystem::path& path) {
    int unlocked = 1;
    std::ifstream input(path);
    if (!(input >> unlocked)) return 1;
    return std::clamp(unlocked, 1, 4);
}

void save_progress(int unlocked, void* context) {
    auto* native = static_cast<Native*>(context);
    if (native->save_path.empty()) return;
    try {
        std::filesystem::create_directories(native->save_path.parent_path());
        auto temporary = native->save_path;
        temporary += ".tmp";
        std::ofstream output(temporary);
        output << std::clamp(unlocked, 1, 4) << '\n';
        output.close();
        if (!output) throw std::runtime_error("Could not write preview progress");
        std::filesystem::rename(temporary, native->save_path);
    } catch (const std::exception& error) {
        std::cerr << "Progress was not saved: " << error.what() << '\n';
    }
}

void quit(void* context) { static_cast<Native*>(context)->quit = true; }

bool set_scene(roundwing::Game& game, const std::string& scene, const std::string& replay) {
    if (scene == "ready") return true;
    if (scene == "cleared" || scene == "complete") {
        game.set_unlocked_levels(4);
        game.select_level(scene == "complete" ? 3 : 0);
        std::ifstream input(replay);
        if (!input) { std::cerr << "A real gameplay --replay file is required for this scene.\n"; return false; }
        std::set<int> taps;
        int frame;
        while (input >> frame) taps.insert(frame);
        for (int step = 0; step < 20000; ++step) {
            if (taps.count(step)) game.flap();
            game.advance(1.0f / 120.0f);
            if (game.phase() == roundwing::Phase::Cleared || game.phase() == roundwing::Phase::Complete) return true;
            if (game.phase() == roundwing::Phase::Failed) break;
        }
        std::cerr << "Replay did not complete the selected stage.\n";
        return false;
    }
    game.flap();
    for (int i = 0; i < 72; ++i) game.advance(1.0f / 120.0f);
    if (scene == "paused") game.pause();
    else if (scene == "failed") {
        for (int i = 0; i < 360 && game.phase() == roundwing::Phase::Playing; ++i) game.advance(1.0f / 120.0f);
    } else if (scene != "playing") {
        std::cerr << "Unknown scene: " << scene << '\n';
        return false;
    }
    return true;
}

bool screenshot(SDL_Renderer* renderer, const std::string& path) {
    int width = 0, height = 0;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!surface) return false;
    const bool success = SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_ARGB8888,
                                              surface->pixels, surface->pitch) == 0 &&
                         SDL_SaveBMP(surface, path.c_str()) == 0;
    SDL_FreeSurface(surface);
    return success;
}
}  // namespace

int main(int argc, char** argv) {
    std::string scene = "ready", shot, replay;
    int frame_limit = 0;
    bool headless = false, no_save = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--headless") headless = true;
        else if (arg == "--no-save") no_save = true;
        else if ((arg == "--scene" || arg == "--screenshot" || arg == "--replay" || arg == "--frames") && i + 1 < argc) {
            const std::string value = argv[++i];
            if (arg == "--scene") scene = value;
            else if (arg == "--screenshot") shot = value;
            else if (arg == "--replay") replay = value;
            else frame_limit = std::max(1, std::atoi(value.c_str()));
        } else {
            std::cerr << "Usage: round-wing-preview [--headless] [--no-save] [--scene ready|playing|paused|failed|cleared|complete] "
                         "[--replay path] [--screenshot image.bmp] [--frames count]\n";
            return 2;
        }
    }
    if (headless) SDL_setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << SDL_GetError() << '\n'; return 1;
    }
    Native native;
    if (!no_save && shot.empty() && scene == "ready") {
        const char* user_home = std::getenv("HOME");
        if (user_home) native.save_path = std::filesystem::path(user_home) / "Library/Application Support/RoundWing/preview-progress.txt";
    }
    native.window = SDL_CreateWindow("圆翼闯关 · 圆屏试玩", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                      kSize, kSize, headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_SHOWN);
    if (native.window) native.renderer = SDL_CreateRenderer(native.window, -1, SDL_RENDERER_SOFTWARE);
    if (native.renderer) native.texture = SDL_CreateTexture(native.renderer, SDL_PIXELFORMAT_ARGB8888,
                                                             SDL_TEXTUREACCESS_STREAMING, kSize, kSize);
    if (!native.texture) { std::cerr << SDL_GetError() << '\n'; SDL_Quit(); return 1; }

    lv_init();
    lv_tick_set_cb(ticks);
    auto* display = lv_display_create(kSize, kSize);
    lv_display_set_color_format(display, LV_COLOR_FORMAT_ARGB8888);
    std::vector<uint32_t> pixels(kSize * kSize);
    lv_display_set_buffers(display, pixels.data(), nullptr, pixels.size() * sizeof(uint32_t), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_user_data(display, &native);
    lv_display_set_flush_cb(display, flush);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x242933), 0);
    auto* pointer = lv_indev_create();
    lv_indev_set_type(pointer, LV_INDEV_TYPE_POINTER);
    lv_indev_set_mode(pointer, LV_INDEV_MODE_EVENT);
    lv_indev_set_read_cb(pointer, pointer_read);
    lv_indev_set_user_data(pointer, &native);

    auto view = std::make_unique<roundwing::GameView>(lv_screen_active(), roundwing::ViewCallbacks{quit, save_progress, &native},
                                                    read_progress(native.save_path));
    if (!set_scene(view->game(), scene, replay)) return 1;
    view->update(0);
    lv_refr_now(display);
    if (!shot.empty()) {
        const bool success = screenshot(native.renderer, shot);
        view.reset();
        lv_indev_delete(pointer);
        lv_display_delete(display);
        SDL_DestroyTexture(native.texture); SDL_DestroyRenderer(native.renderer); SDL_DestroyWindow(native.window); SDL_Quit();
        std::cout << (success ? "Saved " : "Screenshot failed: ") << shot << '\n';
        return success ? 0 : 1;
    }

    using Clock = std::chrono::steady_clock;
    auto previous = Clock::now();
    int frames = 0;
    while (!native.quit) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) native.quit = true;
            else if (event.type == SDL_MOUSEMOTION) {
                native.pointer.update(event.motion.x, event.motion.y, native.pointer.held);
                lv_indev_read(pointer);
            }
            else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
                if (event.button.button == SDL_BUTTON_LEFT) {
                    native.pointer.update(event.button.x, event.button.y, event.type == SDL_MOUSEBUTTONDOWN);
                    // Feed every edge before consuming the next SDL event: a queued
                    // down/up pair must still pass through LVGL button hit testing.
                    lv_indev_read(pointer);
                }
            } else if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                if (event.key.keysym.sym == SDLK_SPACE) view->tap();
                else if (event.key.keysym.sym == SDLK_ESCAPE) {
                    if (view->game().phase() == roundwing::Phase::Paused) view->game().resume();
                    else view->suspend();
                }
            } else if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                native.pointer.update(native.pointer.x, native.pointer.y, false);
                lv_indev_reset(pointer, nullptr);
                lv_indev_read(pointer);
                view->suspend();
            }
        }
        const auto now = Clock::now();
        const float elapsed = std::chrono::duration<float>(now - previous).count();
        if (elapsed >= 1.0f / 60.0f) {
            previous = now;
            view->update(elapsed);
            ++frames;
            if (frame_limit > 0 && frames >= frame_limit) native.quit = true;
        }
        lv_timer_handler();
        SDL_Delay(3);
    }
    view.reset();
    lv_indev_delete(pointer);
    lv_display_delete(display);
    SDL_DestroyTexture(native.texture); SDL_DestroyRenderer(native.renderer); SDL_DestroyWindow(native.window); SDL_Quit();
    return 0;
}
