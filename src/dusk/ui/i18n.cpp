#include "i18n.hpp"

#include "dusk/logging.h"
#include "dusk/settings.h"

#include <borealis/io.hpp>
#include <nlohmann/json.hpp>

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_locale.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace dusk::ui::i18n {
namespace {

using Json = nlohmann::json;

using StringMap = std::unordered_map<std::string, std::string>;

constexpr std::string_view kResourceDir = "res/l10n";
constexpr std::string_view kMetaKey = "_meta";

StringMap gTranslations;
StringMap gEnglish;
bool gTranslated = false;
std::string gCurrentLanguage{kSourceLanguage};
std::vector<LanguageInfo> gLanguages;
std::atomic<bool> gRefreshRequested{false};

/*
 * Directory holding the locale files. Assets resolve relative to SDL_GetBasePath()
 * (or DUSK_ASSET_DIR when installed), mirroring ImGuiEngine::GetAssetPath.
 */
std::string locale_dir() {
    std::string path;
#ifdef DUSK_ASSET_DIR
    const char* basePath = DUSK_ASSET_DIR;
#else
    const char* basePath = SDL_GetBasePath();
#endif
    if (basePath != nullptr && basePath[0] != '\0') {
        path = basePath;
        if (path.back() != '/' && path.back() != '\\') {
            path += '/';
        }
    }
    path += kResourceDir;
    return path;
}

std::string locale_path(std::string_view code) {
    std::string path = locale_dir();
    path += '/';
    path += code;
    path += ".json";
    return path;
}

std::string lower(std::string_view value) {
    std::string out{value};
    std::transform(out.begin(), out.end(), out.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

void flatten(const std::string& prefix, const Json& node, StringMap& out) {
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            if (prefix.empty() && it.key() == kMetaKey) {
                continue;
            }
            std::string key = it.key();
            if (!prefix.empty()) {
                key = prefix + "." + key;
            }
            flatten(key, it.value(), out);
        }
        return;
    }
    if (node.is_string()) {
        out.emplace(prefix, node.get<std::string>());
    }
}

std::optional<Json> read_json(const std::string& path) {
    SDL_IOStream* stream = SDL_IOFromFile(path.c_str(), "rb");
    if (stream == nullptr) {
        return std::nullopt;
    }
    const Sint64 size = SDL_GetIOSize(stream);
    std::string contents;
    if (size > 0) {
        contents.resize(static_cast<size_t>(size));
        const size_t read = SDL_ReadIO(stream, contents.data(), contents.size());
        contents.resize(read);
    }
    SDL_CloseIO(stream);

    try {
        return Json::parse(contents);
    } catch (const Json::parse_error& e) {
        DuskLog.error("i18n: failed to parse {}: {}", path, e.what());
        return std::nullopt;
    }
}

bool load_language_file(std::string_view code, StringMap& out) {
    const auto json = read_json(locale_path(code));
    if (!json) {
        return false;
    }
    flatten("", *json, out);
    return true;
}

/*
 * Sorted list of fmt replacement fields ("{count}", "{}") in a string, ignoring
 * escaped braces ("{{" / "}}"). Used to reject translations that would make
 * fmt::format throw or silently drop a value.
 */
std::vector<std::string> placeholders(std::string_view text) {
    std::vector<std::string> out;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '{') {
            continue;
        }
        if (i + 1 < text.size() && text[i + 1] == '{') {
            ++i;
            continue;
        }
        const size_t end = text.find('}', i);
        if (end == std::string_view::npos) {
            out.emplace_back(text.substr(i));
            break;
        }
        out.emplace_back(text.substr(i, end - i + 1));
        i = end;
    }
    std::sort(out.begin(), out.end());
    return out;
}

/* Drop translated entries whose placeholders differ from English. */
void validate_translations(std::string_view code) {
    size_t rejected = 0;
    for (auto it = gTranslations.begin(); it != gTranslations.end();) {
        const auto english = gEnglish.find(it->first);
        if (english != gEnglish.end() && placeholders(english->second) != placeholders(it->second)) {
            DuskLog.warn("i18n: '{}' in {}.json has placeholders that do not match English; "
                         "using English",
                it->first, code);
            it = gTranslations.erase(it);
            ++rejected;
            continue;
        }
        ++it;
    }
    if (rejected > 0) {
        DuskLog.warn("i18n: rejected {} entr{} from {}.json", rejected, rejected == 1 ? "y" : "ies",
            code);
    }
}

void discover_languages() {
    gLanguages.clear();

    std::error_code ec;
    const std::filesystem::path dir = borealis::io::fs_path_from_utf8(locale_dir());
    for (const auto& entry : std::filesystem::directory_iterator{dir, ec}) {
        if (!entry.is_regular_file(ec) || entry.path().extension() != ".json") {
            continue;
        }
        LanguageInfo info;
        info.code = borealis::io::fs_path_to_generic_string(entry.path().stem());
        info.name = info.code;
        if (const auto json = read_json(locale_path(info.code))) {
            const auto meta = json->find(kMetaKey);
            if (meta != json->end() && meta->is_object()) {
                const auto name = meta->find("name");
                if (name != meta->end() && name->is_string()) {
                    info.name = name->get<std::string>();
                }
            }
        }
        gLanguages.push_back(std::move(info));
    }
    if (ec) {
        DuskLog.warn("i18n: could not list {}: {}", locale_dir(), ec.message());
    }

    const bool hasEnglish = std::any_of(gLanguages.begin(), gLanguages.end(),
        [](const LanguageInfo& l) { return l.code == kSourceLanguage; });
    if (!hasEnglish) {
        gLanguages.push_back({std::string{kSourceLanguage}, "English"});
    }

    std::sort(gLanguages.begin(), gLanguages.end(), [](const LanguageInfo& a, const LanguageInfo& b) {
        if ((a.code == kSourceLanguage) != (b.code == kSourceLanguage)) {
            return a.code == kSourceLanguage;
        }
        return lower(a.name) < lower(b.name);
    });
}

const LanguageInfo* find_language(std::string_view code) {
    const std::string wanted = lower(code);
    for (const auto& language : gLanguages) {
        if (lower(language.code) == wanted) {
            return &language;
        }
    }
    return nullptr;
}

/*
 * Map the system locales onto an available language. "pt_BR" tries "pt-BR" first,
 * then "pt".
 */
std::string detect_system_language() {
    int localeCount = 0;
    SDL_Locale** locales = SDL_GetPreferredLocales(&localeCount);
    if (locales == nullptr || localeCount <= 0) {
        return std::string{kSourceLanguage};
    }

    std::string result{kSourceLanguage};
    for (int i = 0; i < localeCount; ++i) {
        const SDL_Locale* locale = locales[i];
        if (locale == nullptr || locale->language == nullptr) {
            continue;
        }
        const std::string language = locale->language;
        if (locale->country != nullptr && locale->country[0] != '\0') {
            if (const auto* match = find_language(language + "-" + locale->country)) {
                result = match->code;
                break;
            }
        }
        if (const auto* match = find_language(language)) {
            result = match->code;
            break;
        }
    }
    SDL_free(locales);
    return result;
}

std::string resolve_language() {
    const std::string configured = getSettings().ui.menuLanguage.getValue();
    if (configured.empty() || lower(configured) == kAutoLanguage) {
        return detect_system_language();
    }
    if (const auto* match = find_language(configured)) {
        return match->code;
    }
    DuskLog.warn("i18n: configured language '{}' not found in {}; using English", configured,
        locale_dir());
    return std::string{kSourceLanguage};
}

void apply_language() {
    gTranslations.clear();
    gEnglish.clear();

    gCurrentLanguage = resolve_language();
    gTranslated = gCurrentLanguage != kSourceLanguage;

    // Always load English: it doubles as the fallback for missing keys.
    if (!load_language_file(kSourceLanguage, gEnglish)) {
        DuskLog.warn("i18n: English locale file missing; menu strings will show their keys");
    }

    if (gTranslated) {
        if (!load_language_file(gCurrentLanguage, gTranslations)) {
            DuskLog.warn("i18n: failed to load locale '{}'; falling back to English",
                gCurrentLanguage);
            gTranslations.clear();
            gTranslated = false;
            gCurrentLanguage = std::string{kSourceLanguage};
        } else {
            validate_translations(gCurrentLanguage);
        }
    }
    DuskLog.info("i18n: menu language '{}' ({} strings, {} translated)", gCurrentLanguage,
        gEnglish.size(), gTranslated ? gTranslations.size() : gEnglish.size());
}

const std::string* find_string(const StringMap& map, std::string_view key) {
    const auto it = map.find(std::string{key});
    return it != map.end() ? &it->second : nullptr;
}

}  // namespace

void initialize() noexcept {
    discover_languages();
    apply_language();
}

void shutdown() noexcept {
    gTranslations.clear();
    gEnglish.clear();
    gLanguages.clear();
    gTranslated = false;
}

std::string_view tr(std::string_view key) noexcept {
    if (gTranslated) {
        if (const auto* value = find_string(gTranslations, key)) {
            return *value;
        }
    }
    if (const auto* value = find_string(gEnglish, key)) {
        return *value;
    }
    return key;
}

const std::vector<LanguageInfo>& available_languages() noexcept {
    return gLanguages;
}

const std::string& current_language() noexcept {
    return gCurrentLanguage;
}

bool is_translated() noexcept {
    return gTranslated;
}

void request_refresh() noexcept {
    // Reload the translation tables so rebuilt documents pick up the new language.
    apply_language();
    gRefreshRequested.store(true, std::memory_order_release);
}

bool consume_refresh_requested() noexcept {
    return gRefreshRequested.exchange(false, std::memory_order_acq_rel);
}

std::string_view language_setting_name() noexcept {
    return "ui.language";
}

}  // namespace dusk::ui::i18n
