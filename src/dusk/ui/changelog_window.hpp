#pragma once

namespace dusk::ui {

/*
 * Fork: release notes and update window, in the style of ftpd/NSA.
 *
 * WhatsNew: what changed from the installed version up to the version the
 *   updater would install (the channel's highest: a stable, or a beta when
 *   following betas), betas in between included, with an Install button.
 * CheckNow: the same, after a fresh check (Settings -> Interface, also in game).
 * History: every release up to that same version, newest first.
 *
 * Installing keeps the window open and shows the progress, so it works the
 * same on the start screen and in game. WhatsNew/History reuse the last check;
 * when there is none they run one themselves.
 */
enum class ChangelogMode {
    WhatsNew,
    CheckNow,
    History,
};

void open_changelog(ChangelogMode mode);

/* Whether the start screen should show What's New before installing. */
bool has_whats_new() noexcept;

}  // namespace dusk::ui
