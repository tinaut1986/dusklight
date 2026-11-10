#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace dusk::ui::i18n {

/*
 * Menu (Dusklight UI) localization. Not related to the game's own language.
 *
 * Every res/l10n/<code>.json file is a language. Each file may carry a "_meta"
 * object with a display name, e.g. {"_meta": {"name": "Español"}}. en.json is the
 * source table and the fallback for any key a translation does not provide.
 *
 * The configured language is stored as a code ("es", "pt-BR", ...) or "auto",
 * which picks the best match for the system locale.
 */

constexpr std::string_view kAutoLanguage = "auto";
constexpr std::string_view kSourceLanguage = "en";

struct LanguageInfo {
    std::string code;  // file name without extension, e.g. "es" or "pt-BR"
    std::string name;  // native display name from _meta.name, or the code
};

/* Discover languages and load the configured one. Call once during UI startup. */
void initialize() noexcept;
void shutdown() noexcept;

/*
 * Translate a string by its dot path (e.g. "menu.reset").
 * Fallback order: current language -> English -> the key itself.
 *
 * Translations whose {placeholders} do not match the English string are rejected
 * at load time, so the result is always safe to pass to fmt::runtime().
 */
std::string_view tr(std::string_view key) noexcept;

/* Convenience wrapper returning a std::string for direct use with Rml::String APIs. */
inline std::string tr_str(std::string_view key) noexcept {
    return std::string{tr(key)};
}

/*
 * Format string for composite texts (e.g. "Press {key} to reset the game.").
 * Same as tr(); kept as a separate name so call sites document that the result is
 * passed to fmt. Placeholders are validated against English when loading.
 */
inline std::string_view tr_source(std::string_view key) noexcept {
    return tr(key);
}

/* Languages found in res/l10n, English first, then sorted by display name. */
const std::vector<LanguageInfo>& available_languages() noexcept;

/* Code of the language currently in effect after "auto" resolution. */
const std::string& current_language() noexcept;

/* Whether the current table comes from a language other than English. */
bool is_translated() noexcept;

/*
 * Rebuild open menus after a language change. Requested by the language selector and
 * consumed by ui::update() so documents rebuild outside of config callbacks.
 */
void request_refresh() noexcept;
bool consume_refresh_requested() noexcept;

/* The name of the CVar backing the menu language, for subscribers. */
std::string_view language_setting_name() noexcept;

}  // namespace dusk::ui::i18n
