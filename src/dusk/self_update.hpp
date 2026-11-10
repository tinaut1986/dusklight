#pragma once

#include <borealis/update.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace dusk::self_update {

/*
 * In-place update of the running AppImage (fork feature).
 *
 * When Dusklight runs from an AppImage ($APPIMAGE is set and its folder is
 * writable), an available release can be installed without leaving the game:
 * the matching "*-linux-<arch>.AppImage" asset is downloaded next to the current
 * file, checked against the SHA-256 digest GitHub publishes for it, made
 * executable and renamed over the old AppImage. The running copy keeps working
 * (its file stays open); the new version starts on the next launch.
 */

/* One release's notes, as shown in the What's New / changelog window. */
struct NotesEntry {
    std::string version;  // "v1.1.0" (tag without the +base metadata)
    std::string base;     // "2.0.3" upstream base, empty if the tag has none
    bool prerelease = false;
    std::string notes;  // Markdown: the user-facing block in the menu language,
                        // English, or the commit list as a last resort
};

struct UpdateCheck {
    /* Target release (the highest version of the channel) and whether it is newer. */
    borealis::update::Result result;
    /* Releases after the installed version up to the target, newest first. Betas
     * in between are included on both channels. Empty when up to date. */
    std::vector<NotesEntry> whatsNew;
    /* Every release up to the target, newest first: the channel's changelog. */
    std::vector<NotesEntry> history;
};

/*
 * Update check for the fork. Unlike borealis' check (which takes the most recently
 * published release), this picks the highest *version* among stable releases and,
 * when includeBeta is set, prereleases too. So a beta build is offered a newer
 * stable, and a beta published after a newer stable never hides it. Release notes
 * are taken in `language` (a res/l10n code) when the release has them.
 */
borealis::Task<UpdateCheck> check_for_update(bool includeBeta, std::string language);

/* The selection used by check_for_update, on a GitHub /releases JSON array. */
UpdateCheck select_update(std::string_view releasesJson, std::string_view currentVersion,
    bool includeBeta, std::string_view language = "en");

/* The user-facing notes of one release body (see docs/release-notes/README.md). */
std::string extract_notes(std::string_view body, std::string_view language);

/* Beta builds always follow betas; stable builds when Beta Updates is on. */
bool follows_betas() noexcept;

/* Latest finished check, shared by the start screen and the changelog window. */
void set_last_check(UpdateCheck check);
const UpdateCheck* last_check() noexcept;

enum class State {
    Idle,
    Downloading,
    Verifying,
    Installed,
    Failed,
};

/* True when running from a writable AppImage on a supported architecture. */
bool supported() noexcept;

/* The release asset this build would install, if the release has one. */
const borealis::update::Asset* find_asset(const borealis::update::Release& release) noexcept;

/* Start downloading and installing. Returns false if not supported or no asset. */
bool start(const borealis::update::Release& release);

/* Advance the download/verify/install steps. Call every frame while not Idle. */
void poll();

State state() noexcept;
/* Bytes done and expected total of the current step, when known. */
std::uint64_t progress_completed() noexcept;
std::optional<std::uint64_t> progress_total() noexcept;
/* Reason of the last failure (English, for logs). */
const std::string& error() noexcept;

}  // namespace dusk::self_update
