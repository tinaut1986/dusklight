#pragma once

#include <aurora/aurora.h>
#include <aurora/event.h>

namespace dusk::display {

/*
 * Fork: display mode (windowed / fullscreen) that is saved and restored, with
 * the window's size and position, and locked to fullscreen in Steam's Game Mode.
 *
 * Game Mode runs games under gamescope (XDG_CURRENT_DESKTOP=gamescope), which
 * shows the game's window on the whole screen at its own size: a window there
 * only made the game render at the default 1216x896 instead of the screen's
 * resolution. So under gamescope the game always runs fullscreen and the
 * setting is left untouched (Desktop Mode and PCs use it as saved).
 */

/* Running in Steam's Game Mode (gamescope): fullscreen is forced. */
bool fullscreen_locked() noexcept;

/* The mode to use now: forced fullscreen, or the saved setting. */
bool fullscreen() noexcept;

/* Switch, apply and save (ignored while fullscreen is locked). */
void set_fullscreen(bool fullscreen) noexcept;

/* Back to windowed at the default size, centred, and save it. */
void restore_default_window() noexcept;

/* Startup: fill the window mode, size and position for aurora_initialize. */
void fill_startup_config(AuroraConfig& config, int defaultWidth, int defaultHeight) noexcept;

/* After the window exists: recentre a restored window that is off every screen. */
void after_window_created() noexcept;

/* Save the windowed size and position as the window changes. Call for every
 * Aurora event, from the start screen and from the game loop. */
void handle_aurora_event(const AuroraEvent& event) noexcept;

}  // namespace dusk::display
