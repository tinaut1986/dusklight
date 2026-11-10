#include "display.hpp"

#include "dusk/config.hpp"
#include "dusk/dusk.h"
#include "dusk/settings.h"

#include <SDL3/SDL_video.h>
#include <aurora/lib/window.hpp>
#include <dolphin/vi.h>

#include <climits>
#include <cstdlib>
#include <string_view>

namespace dusk::display {
namespace {

constexpr int kUnsetPosition = INT32_MIN;

bool has_saved_position() {
    const auto& video = getSettings().video;
    return video.lastWindowX.getValue() != kUnsetPosition &&
           video.lastWindowY.getValue() != kUnsetPosition;
}

}  // namespace

bool fullscreen_locked() noexcept {
    static const bool locked = [] {
        const char* desktop = std::getenv("XDG_CURRENT_DESKTOP");
        return desktop != nullptr && std::string_view{desktop} == "gamescope";
    }();
    return locked;
}

bool fullscreen() noexcept {
    return fullscreen_locked() || getSettings().video.enableFullscreen.getValue();
}

void set_fullscreen(bool value) noexcept {
    if (fullscreen_locked()) {
        return;
    }
    getSettings().video.enableFullscreen.setValue(value);
    VISetWindowFullscreen(value);
    config::save();
}

void restore_default_window() noexcept {
    if (fullscreen_locked()) {
        return;
    }
    auto& video = getSettings().video;
    video.enableFullscreen.setValue(false);
    VISetWindowFullscreen(false);
    VISetWindowSize(defaultWindowWidth * 2, defaultWindowHeight * 2);
    VICenterWindow();
    video.lastWindowWidth.setValue(0);
    video.lastWindowHeight.setValue(0);
    video.lastWindowX.setValue(kUnsetPosition);
    video.lastWindowY.setValue(kUnsetPosition);
    config::save();
}

void fill_startup_config(AuroraConfig& config, int defaultWidth, int defaultHeight) noexcept {
    const auto& video = getSettings().video;
    config.startFullscreen = fullscreen();

    // The last windowed size and position are always restored (they used to need
    // the "Remember Window Size" option, off by default).
    const int width = video.lastWindowWidth.getValue();
    const int height = video.lastWindowHeight.getValue();
    if (width > 0 && height > 0) {
        config.windowWidth = static_cast<uint32_t>(width);
        config.windowHeight = static_cast<uint32_t>(height);
    } else {
        config.windowWidth = static_cast<uint32_t>(defaultWidth);
        config.windowHeight = static_cast<uint32_t>(defaultHeight);
    }
    if (has_saved_position() && !fullscreen_locked()) {
        config.windowPosX = video.lastWindowX.getValue();
        config.windowPosY = video.lastWindowY.getValue();
    } else {
        config.windowPosX = -1;
        config.windowPosY = -1;
    }
}

void after_window_created() noexcept {
    if (fullscreen() || !has_saved_position()) {
        return;
    }
    // A monitor that is gone can leave the saved position off every screen.
    SDL_Window* window = aurora::window::get_sdl_window();
    if (window == nullptr) {
        return;
    }
    int x = 0, y = 0, w = 0, h = 0;
    SDL_GetWindowPosition(window, &x, &y);
    SDL_GetWindowSize(window, &w, &h);
    const SDL_Rect rect{x, y, w, h};
    int count = 0;
    SDL_DisplayID* displays = SDL_GetDisplays(&count);
    bool visible = false;
    for (int i = 0; displays != nullptr && i < count && !visible; ++i) {
        SDL_Rect bounds{};
        if (SDL_GetDisplayUsableBounds(displays[i], &bounds)) {
            SDL_Rect overlap{};
            visible = SDL_GetRectIntersection(&rect, &bounds, &overlap) && overlap.w >= 64 &&
                      overlap.h >= 64;
        }
    }
    SDL_free(displays);
    if (!visible) {
        VICenterWindow();
    }
}

void handle_aurora_event(const AuroraEvent& event) noexcept {
    if (fullscreen()) {
        return;  // only the windowed geometry is remembered
    }
    auto& video = getSettings().video;
    switch (event.type) {
    case AURORA_WINDOW_RESIZED:
        if (static_cast<int>(event.windowSize.width) != video.lastWindowWidth.getValue() ||
            static_cast<int>(event.windowSize.height) != video.lastWindowHeight.getValue())
        {
            video.lastWindowWidth.setValue(static_cast<int>(event.windowSize.width));
            video.lastWindowHeight.setValue(static_cast<int>(event.windowSize.height));
            config::save();
        }
        break;
    case AURORA_WINDOW_MOVED:
        if (event.windowPos.x != video.lastWindowX.getValue() ||
            event.windowPos.y != video.lastWindowY.getValue())
        {
            video.lastWindowX.setValue(event.windowPos.x);
            video.lastWindowY.setValue(event.windowPos.y);
            config::save();
        }
        break;
    default:
        break;
    }
}

}  // namespace dusk::display
