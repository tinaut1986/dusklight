#pragma once

#include <functional>

namespace dusk::ui {

/*
 * Fork: release notes window, in the style of ftpd/NSA.
 *
 * WhatsNew: what changed from the installed version up to the version the
 *   updater would install (the channel's highest: a stable, or a beta when
 *   following betas). Betas in between are included. `onInstall` adds the
 *   install/download action.
 * History: every release up to that same version, newest first.
 *
 * Uses the last update check; when there is none (checks disabled, or opened
 * from the settings before the start screen ran one) it runs one itself.
 */
enum class ChangelogMode {
    WhatsNew,
    History,
};

void open_changelog(ChangelogMode mode, std::function<void()> onInstall = {});

/* Whether the start screen should show What's New before installing. */
bool has_whats_new() noexcept;

}  // namespace dusk::ui
