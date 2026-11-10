#pragma once

#include <borealis/update.hpp>

#include <cstdint>
#include <optional>
#include <string>

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
