#pragma once

#include <SDL3/SDL_events.h>

namespace dusk::ui::touch_feedback {

/*
 * Fork: a mark where a finger touches the screen, instead of the mouse arrow.
 *
 * Only real touchscreen events draw it (on the Steam Deck that needs Steam's
 * "Touchscreen Native Support"; with gamescope's default mouse emulation the
 * game only ever sees a mouse). A gold ring in the theme's accent colour grows
 * in on touch, follows the finger, and expands and fades on release. It sits in
 * its own document above everything and never takes input.
 */
void handle_event(const SDL_Event& event) noexcept;
void update() noexcept;
void shutdown() noexcept;

}  // namespace dusk::ui::touch_feedback
