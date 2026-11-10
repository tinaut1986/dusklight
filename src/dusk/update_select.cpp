#include "self_update.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <exception>
#include <mutex>
#include <optional>
#include <string>

// Kept apart from self_update.cpp so it only depends on borealis' version
// helpers and can be tested without HTTP or the game.

namespace dusk::self_update {
namespace {

using borealis::update::Result;
using borealis::update::Status;
using borealis::update::Version;
using Json = nlohmann::json;

struct Candidate {
    const Json* json;
    Version version;
    std::string tag;
    bool prerelease;
};

std::mutex gLastCheckMutex;
std::optional<UpdateCheck> gLastCheck;

std::string trim(std::string_view value) {
    const auto begin = value.find_first_not_of(" \t\r\n");
    if (begin == std::string_view::npos) {
        return {};
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return std::string{value.substr(begin, end - begin + 1)};
}

/* Text between <!-- dusklight-notes:LANG --> and <!-- /dusklight-notes:LANG -->. */
std::optional<std::string> marked_block(std::string_view body, std::string_view language) {
    const std::string open = "<!-- dusklight-notes:" + std::string{language} + " -->";
    const std::string close = "<!-- /dusklight-notes:" + std::string{language} + " -->";
    const auto a = body.find(open);
    if (a == std::string_view::npos) {
        return std::nullopt;
    }
    const auto start = a + open.size();
    const auto b = body.find(close, start);
    auto block = trim(body.substr(start, b == std::string_view::npos ? std::string_view::npos : b - start));
    if (block.empty()) {
        return std::nullopt;
    }
    return block;
}

/* The bullet lines of the "## Changes" section the workflow generates. */
std::string commit_list(std::string_view body) {
    const auto section = body.find("## Changes");
    if (section == std::string_view::npos) {
        return {};
    }
    std::string out;
    std::string_view rest = body.substr(section);
    rest.remove_prefix(std::min(rest.size(), rest.find('\n') == std::string_view::npos
                                                 ? rest.size()
                                                 : rest.find('\n') + 1));
    while (!rest.empty()) {
        const auto eol = rest.find('\n');
        const auto line = trim(rest.substr(0, eol));
        rest = eol == std::string_view::npos ? std::string_view{} : rest.substr(eol + 1);
        if (line.rfind("## ", 0) == 0) {
            break;
        }
        if (line.rfind("- ", 0) == 0) {
            out += line;
            out += '\n';
        }
    }
    return out;
}

std::string version_label(std::string_view tag) {
    return std::string{tag.substr(0, tag.find('+'))};
}

std::string base_label(std::string_view tag) {
    constexpr std::string_view marker = "+base.";
    const auto pos = tag.find(marker);
    if (pos == std::string_view::npos) {
        return {};
    }
    auto base = tag.substr(pos + marker.size());
    if (!base.empty() && base.front() == 'v') {
        base.remove_prefix(1);
    }
    return std::string{base};
}

NotesEntry make_entry(const Candidate& candidate, std::string_view language) {
    return {
        .version = version_label(candidate.tag),
        .base = base_label(candidate.tag),
        .prerelease = candidate.prerelease,
        .notes = extract_notes(candidate.json->value("body", std::string{}), language),
    };
}

}  // namespace

std::string extract_notes(std::string_view body, std::string_view language) {
    if (!language.empty()) {
        if (auto block = marked_block(body, language)) {
            return *block;
        }
    }
    if (language != "en") {
        if (auto block = marked_block(body, "en")) {
            return *block;
        }
    }
    return trim(commit_list(body));
}

UpdateCheck select_update(std::string_view releasesJson, std::string_view currentVersion,
    bool includeBeta, std::string_view language) {
    UpdateCheck check;

    Json releases;
    try {
        releases = Json::parse(releasesJson);
    } catch (const std::exception& e) {
        check.result = {.status = Status::Failed,
            .message = std::string{"Failed to parse GitHub releases JSON: "} + e.what()};
        return check;
    }
    if (!releases.is_array()) {
        check.result = {.status = Status::Failed, .message = "Expected a GitHub releases array"};
        return check;
    }

    // Every published fork release, highest version first.
    std::vector<Candidate> all;
    for (const auto& release : releases) {
        if (!release.is_object() || release.value("draft", false)) {
            continue;
        }
        const auto tag = release.value("tag_name", std::string{});
        const auto version = borealis::update::parse_version(tag);
        if (!version) {
            continue;  // not one of our tags
        }
        all.push_back({&release, *version, tag, release.value("prerelease", false)});
    }
    std::stable_sort(all.begin(), all.end(), [](const Candidate& a, const Candidate& b) {
        return borealis::update::compare_version(a.version, b.version) > 0;
    });

    // Target: the highest version of the channel (stable only, or betas too).
    const auto target = std::find_if(all.begin(), all.end(),
        [includeBeta](const Candidate& c) { return includeBeta || !c.prerelease; });
    if (target == all.end()) {
        // Nothing published for this channel yet: not an error, just nothing to offer.
        check.result = {.status = Status::UpToDate, .message = "No releases for this channel yet"};
        return check;
    }

    try {
        check.result.latest = borealis::update::parse_github_release(target->json->dump());
    } catch (const std::exception& e) {
        check.result = {.status = Status::Failed,
            .message = std::string{"Failed to parse GitHub release: "} + e.what()};
        return check;
    }

    // Changelog: every release up to the target, betas included on both
    // channels (a stable lists the betas that led to it). Betas newer than a
    // stable target are left out.
    const auto current = borealis::update::parse_version(currentVersion);
    for (auto it = target; it != all.end(); ++it) {
        auto entry = make_entry(*it, language);
        if (current && borealis::update::compare_version(it->version, *current) > 0) {
            check.whatsNew.push_back(entry);
        }
        check.history.push_back(std::move(entry));
    }

    if (!current) {
        check.result.status = Status::Failed;
        check.result.message =
            "Failed to parse current version '" + std::string{currentVersion} + "'";
        return check;
    }
    check.result.status = borealis::update::compare_version(target->version, *current) > 0
                              ? Status::UpdateAvailable
                              : Status::UpToDate;
    if (check.result.status != Status::UpdateAvailable) {
        check.whatsNew.clear();
    }
    return check;
}

void set_last_check(UpdateCheck check) {
    std::lock_guard lock{gLastCheckMutex};
    gLastCheck = std::move(check);
}

const UpdateCheck* last_check() noexcept {
    std::lock_guard lock{gLastCheckMutex};
    return gLastCheck ? &*gLastCheck : nullptr;
}

}  // namespace dusk::self_update
