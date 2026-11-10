#include "self_update.hpp"

#include <nlohmann/json.hpp>

#include <exception>
#include <optional>
#include <string>

// Kept apart from self_update.cpp so it only depends on borealis' version
// helpers and can be tested without HTTP or the game.

namespace dusk::self_update {

borealis::update::Result select_update(
    std::string_view releasesJson, std::string_view currentVersion, bool includeBeta) {
    using borealis::update::Result;
    using borealis::update::Status;
    using Json = nlohmann::json;

    Json releases;
    try {
        releases = Json::parse(releasesJson);
    } catch (const std::exception& e) {
        return {.status = Status::Failed,
            .message = std::string{"Failed to parse GitHub releases JSON: "} + e.what()};
    }
    if (!releases.is_array()) {
        return {.status = Status::Failed, .message = "Expected a GitHub releases array"};
    }

    const Json* best = nullptr;
    std::optional<borealis::update::Version> bestVersion;
    for (const auto& release : releases) {
        if (!release.is_object() || release.value("draft", false)) {
            continue;
        }
        if (release.value("prerelease", false) && !includeBeta) {
            continue;
        }
        const auto tag = release.value("tag_name", std::string{});
        const auto version = borealis::update::parse_version(tag);
        if (!version) {
            continue;  // not one of our tags
        }
        if (!bestVersion || borealis::update::compare_version(*version, *bestVersion) > 0) {
            best = &release;
            bestVersion = version;
        }
    }
    if (best == nullptr) {
        // Nothing published for this channel yet: not an error, just nothing to offer.
        return {.status = Status::UpToDate, .message = "No releases for this channel yet"};
    }

    Result result;
    try {
        result.latest = borealis::update::parse_github_release(best->dump());
    } catch (const std::exception& e) {
        return {.status = Status::Failed,
            .message = std::string{"Failed to parse GitHub release: "} + e.what()};
    }

    const auto current = borealis::update::parse_version(currentVersion);
    if (!current) {
        result.status = Status::Failed;
        result.message = "Failed to parse current version '" + std::string{currentVersion} + "'";
        return result;
    }
    result.status = borealis::update::compare_version(*bestVersion, *current) > 0
                        ? Status::UpdateAvailable
                        : Status::UpToDate;
    return result;
}

}  // namespace dusk::self_update
