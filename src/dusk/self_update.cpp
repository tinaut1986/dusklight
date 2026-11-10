#include "self_update.hpp"

#include "dusk/app_info.hpp"
#include "dusk/hash.hpp"

#include <borealis/http.hpp>
#include <borealis/io.hpp>
#include <borealis/log.hpp>
#include <borealis/task.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

#if defined(__linux__) && !defined(__ANDROID__)
#include <unistd.h>
#define DUSK_SELF_UPDATE_PLATFORM 1
#else
#define DUSK_SELF_UPDATE_PLATFORM 0
#endif

namespace dusk::self_update {
namespace {

constexpr borealis::Log SelfUpdateLog{"dusk::self_update"};

#if defined(__x86_64__) || defined(_M_X64)
constexpr std::string_view kAssetSuffix = "-linux-x86_64.AppImage";
#elif defined(__aarch64__) || defined(_M_ARM64)
constexpr std::string_view kAssetSuffix = "-linux-aarch64.AppImage";
#else
constexpr std::string_view kAssetSuffix = "";
#endif

struct VerifyResult {
    bool ok = false;
    std::string error;
};

State gState = State::Idle;
std::string gError;
std::filesystem::path gTarget;
std::filesystem::path gPartial;
std::string gExpectedSha256;
borealis::Task<borealis::http::Result> gDownload;
borealis::Task<VerifyResult> gVerify;

std::string lowercase(std::string_view value) {
    std::string out{value};
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

bool ends_with(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() &&
           value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

/* GitHub publishes asset digests as "sha256:<hex>". */
std::string sha256_from_digest(std::string_view digest) {
    constexpr std::string_view prefix = "sha256:";
    if (digest.substr(0, prefix.size()) != prefix) {
        return {};
    }
    std::string hex = lowercase(digest.substr(prefix.size()));
    const bool valid = hex.size() == 64 && std::ranges::all_of(hex, [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
    return valid ? hex : std::string{};
}

void fail(std::string message) {
    SelfUpdateLog.error("Update install failed: {}", message);
    gError = std::move(message);
    gState = State::Failed;
    gDownload = {};
    gVerify = {};
    std::error_code ec;
    std::filesystem::remove(gPartial, ec);
}

VerifyResult verify_and_install(const std::filesystem::path& partial,
    const std::filesystem::path& target, const std::string& expectedSha256,
    borealis::TaskContext& context) {
    std::ifstream input{partial, std::ios::binary};
    if (!input) {
        return {.error = "Could not open the downloaded file"};
    }
    hash::Sha256 hash;
    std::array<uint8_t, 64 * 1024> buffer{};
    std::uint64_t completed = 0;
    while (input) {
        if (context.cancel_requested()) {
            return {.error = "Canceled"};
        }
        input.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        const auto count = input.gcount();
        if (count > 0) {
            hash.update(std::span{buffer.data(), static_cast<size_t>(count)});
            completed += static_cast<std::uint64_t>(count);
            context.report_progress(completed);
        }
    }
    if (!input.eof()) {
        return {.error = "Could not read the downloaded file"};
    }
    input.close();

    const std::string actual = hash.finish();
    if (actual != expectedSha256) {
        return {.error = "SHA-256 mismatch: expected " + expectedSha256 + ", got " + actual};
    }

    std::error_code ec;
    std::filesystem::permissions(partial,
        std::filesystem::perms::owner_all | std::filesystem::perms::group_read |
            std::filesystem::perms::group_exec | std::filesystem::perms::others_read |
            std::filesystem::perms::others_exec,
        std::filesystem::perm_options::replace, ec);
    if (ec) {
        return {.error = "Could not make the new AppImage executable: " + ec.message()};
    }
    // Same directory, so this is an atomic replace. The running AppImage keeps its
    // already-open file; the new one is used from the next launch.
    std::filesystem::rename(partial, target, ec);
    if (ec) {
        return {.error = "Could not replace the AppImage: " + ec.message()};
    }
    return {.ok = true};
}

}  // namespace

bool supported() noexcept {
#if DUSK_SELF_UPDATE_PLATFORM
    if (kAssetSuffix.empty()) {
        return false;
    }
    const char* appimage = std::getenv("APPIMAGE");
    if (appimage == nullptr || appimage[0] == '\0') {
        return false;
    }
    std::error_code ec;
    const std::filesystem::path path = borealis::io::fs_path_from_utf8(appimage);
    if (!std::filesystem::is_regular_file(path, ec)) {
        return false;
    }
    const auto dir = path.parent_path();
    return access(dir.c_str(), W_OK) == 0 && access(path.c_str(), W_OK) == 0;
#else
    return false;
#endif
}

const borealis::update::Asset* find_asset(const borealis::update::Release& release) noexcept {
    if (kAssetSuffix.empty()) {
        return nullptr;
    }
    for (const auto& asset : release.assets) {
        if (ends_with(asset.name, kAssetSuffix) && !asset.browserDownloadUrl.empty()) {
            return &asset;
        }
    }
    return nullptr;
}

bool start(const borealis::update::Release& release) {
    if (gState == State::Downloading || gState == State::Verifying) {
        return true;
    }
    if (!supported()) {
        return false;
    }
    const auto* asset = find_asset(release);
    if (asset == nullptr) {
        return false;
    }

    gError.clear();
    gExpectedSha256 = sha256_from_digest(asset->digest);
    if (gExpectedSha256.empty()) {
        // Never install a file we cannot verify.
        fail("The release asset has no SHA-256 digest");
        return true;
    }

    gTarget = borealis::io::fs_path_from_utf8(std::getenv("APPIMAGE"));
    gPartial = gTarget;
    gPartial += ".update";
    std::error_code ec;
    std::filesystem::remove(gPartial, ec);  // no resume across releases

    SelfUpdateLog.info("Downloading {} ({}) to {}", asset->name, release.tagName,
        borealis::io::fs_path_to_generic_string(gPartial));
    gState = State::Downloading;
    gDownload = borealis::http::start({
        .url = asset->browserDownloadUrl,
        .headers = {{.name = "User-Agent", .value = borealis::user_agent(AppInfo)}},
        .downloadTo = gPartial,
        .connectTimeout = std::chrono::seconds{10},
        .idleTimeout = std::chrono::seconds{30},
        .totalTimeout = std::nullopt,
    });
    return true;
}

void poll() {
    if (gState == State::Downloading && gDownload && gDownload.ready()) {
        std::optional<borealis::http::Result> result;
        try {
            result = gDownload.try_take();
        } catch (const std::exception& e) {
            fail(std::string{"Download failed: "} + e.what());
            return;
        }
        gDownload = {};
        if (!result) {
            fail("Download produced no result");
            return;
        }
        if (result->error != borealis::http::Error::None) {
            fail("Download failed: " + result->message);
            return;
        }
        const int status = result->response.statusCode;
        if (status < 200 || status >= 300) {
            fail("Download failed with HTTP " + std::to_string(status));
            return;
        }
        gState = State::Verifying;
        gVerify = borealis::spawn(
            [partial = gPartial, target = gTarget, expected = gExpectedSha256](
                borealis::TaskContext& context) {
                return verify_and_install(partial, target, expected, context);
            });
        return;
    }

    if (gState == State::Verifying && gVerify && gVerify.ready()) {
        std::optional<VerifyResult> result;
        try {
            result = gVerify.try_take();
        } catch (const std::exception& e) {
            fail(std::string{"Install failed: "} + e.what());
            return;
        }
        gVerify = {};
        if (!result || !result->ok) {
            fail(result ? result->error : "Install produced no result");
            return;
        }
        SelfUpdateLog.info("Installed update to {}", borealis::io::fs_path_to_generic_string(gTarget));
        gState = State::Installed;
    }
}

State state() noexcept {
    return gState;
}

std::uint64_t progress_completed() noexcept {
    if (gState == State::Downloading && gDownload) {
        return gDownload.progress().completed;
    }
    if (gState == State::Verifying && gVerify) {
        return gVerify.progress().completed;
    }
    return 0;
}

std::optional<std::uint64_t> progress_total() noexcept {
    if (gState == State::Downloading && gDownload) {
        return gDownload.progress().total;
    }
    return std::nullopt;
}

const std::string& error() noexcept {
    return gError;
}

}  // namespace dusk::self_update
