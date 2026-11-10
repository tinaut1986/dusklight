#pragma once

namespace Rml {
class Context;
}

namespace dusk::ui {

/*
 * Fork: drag to scroll with the primary button / a finger.
 *
 * On the Steam Deck in Game Mode, gamescope turns touchscreen input into mouse
 * clicks and motion, so RmlUi's own touch scrolling never sees a finger and a
 * drag only moved the cursor. This listens (capture phase, on the context root)
 * for a left-button drag: past a small threshold it scrolls the nearest
 * scrollable container under the press, like a phone, and swallows the click
 * that would follow on release. Elements with their own drag behaviour
 * (colour pickers, scrollbars) are left alone.
 */
void install_drag_scroll(Rml::Context& context) noexcept;

}  // namespace dusk::ui
