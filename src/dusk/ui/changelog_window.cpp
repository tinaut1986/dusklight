#include "changelog_window.hpp"

#include "dusk/self_update.hpp"
#include "i18n.hpp"
#include "modal.hpp"
#include "ui.hpp"

#include <SDL3/SDL_misc.h>
#include <borealis/task.hpp>
#include <fmt/format.h>

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dusk::ui {
namespace {

using self_update::NotesEntry;

constexpr std::string_view kStyleId = "fork-changelog";
constexpr auto kStyles = R"RCSS(
window.modal.changelog {
    max-width: 860dp;
    max-height: 86%;
}
window.modal.changelog modal-content {
    min-height: 120dp;
}
changelog-entry {
    display: block;
    padding: 6dp 0dp 10dp 0dp;
    border-bottom: 1dp var(--color-border);
}
changelog-version {
    display: block;
    font-size: var(--font-size-2xl);
    font-weight: bold;
    color: var(--color-accent);
}
changelog-version beta {
    font-size: var(--font-size-sm);
    color: var(--color-warning);
    padding-left: 8dp;
}
changelog-version base {
    font-size: var(--font-size-sm);
    font-weight: normal;
    color: var(--color-border);
    padding-left: 8dp;
}
changelog-line {
    display: block;
    padding: 2dp 0dp 2dp 14dp;
    font-size: var(--font-size-lg);
}
changelog-heading {
    display: block;
    padding-top: 6dp;
    font-weight: bold;
}
changelog-status {
    display: block;
    padding: 4dp 0dp 8dp 0dp;
    font-weight: bold;
    color: var(--color-accent);
}
changelog-empty {
    display: block;
    color: var(--color-border);
}
)RCSS";

void ensure_styles() {
    static bool registered = false;
    if (!registered) {
        registered = register_scoped_styles(DocumentScope::Window, std::string{kStyleId}, kStyles);
    }
}

/* Inline Markdown that release notes use: **bold**, `code`, [text](url). */
std::string inline_rml(std::string_view text) {
    std::string out;
    bool bold = false;
    for (size_t i = 0; i < text.size(); ++i) {
        if (text.compare(i, 2, "**") == 0) {
            out += bold ? "</b>" : "<b>";
            bold = !bold;
            ++i;
            continue;
        }
        if (text[i] == '`') {
            continue;
        }
        if (text[i] == '[') {
            const auto close = text.find("](", i);
            const auto end = close == std::string_view::npos ? close : text.find(')', close);
            if (end != std::string_view::npos) {
                out += escape(text.substr(i + 1, close - i - 1));
                i = end;
                continue;
            }
        }
        out += escape(text.substr(i, 1));
    }
    if (bold) {
        out += "</b>";
    }
    return out;
}

std::string notes_rml(std::string_view markdown) {
    std::string out;
    while (!markdown.empty()) {
        const auto eol = markdown.find('\n');
        std::string_view line = markdown.substr(0, eol);
        markdown = eol == std::string_view::npos ? std::string_view{} : markdown.substr(eol + 1);
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) {
            line.remove_suffix(1);
        }
        if (line.empty() || line.front() == '>') {
            continue;
        }
        if (line.front() == '#') {
            line.remove_prefix(line.find_first_not_of("# ") == std::string_view::npos
                                   ? line.size()
                                   : line.find_first_not_of("# "));
            out += "<changelog-heading>" + inline_rml(line) + "</changelog-heading>";
        } else if (line.rfind("- ", 0) == 0 || line.rfind("* ", 0) == 0) {
            out += "<changelog-line>&#x2022; " + inline_rml(line.substr(2)) + "</changelog-line>";
        } else {
            out += "<changelog-line>" + inline_rml(line) + "</changelog-line>";
        }
    }
    return out;
}

std::string entries_rml(const std::vector<NotesEntry>& entries) {
    std::string out;
    for (const auto& entry : entries) {
        out += "<changelog-entry><changelog-version>" + escape(entry.version);
        if (entry.prerelease) {
            out += "<beta>" + escape(i18n::tr_str("changelog.beta")) + "</beta>";
        }
        if (!entry.base.empty()) {
            out += "<base>" +
                   escape(fmt::format(fmt::runtime(i18n::tr_source("changelog.base")),
                       fmt::arg("version", entry.base))) +
                   "</base>";
        }
        out += "</changelog-version>";
        out += entry.notes.empty() ?
                   "<changelog-empty>" + escape(i18n::tr_str("changelog.no_notes")) + "</changelog-empty>" :
                   notes_rml(entry.notes);
        out += "</changelog-entry>";
    }
    return out;
}

class ChangelogModal final : public Modal {
public:
    explicit ChangelogModal(ChangelogMode mode)
        : Modal{Props{
              .title = i18n::tr_str(mode == ChangelogMode::History ? "changelog.title_history"
                                                                   : "changelog.title_whats_new"),
              .bodyRml = "",
              .actions = make_actions(mode, this),
              .onDismiss = [](Modal& modal) {
                  mDoAud_seStartMenu(kSoundWindowClose);
                  modal.pop();
              },
              .variant = "changelog",
          }},
          mMode{mode} {
        mStatus = content_pane().add_rml("");
        mBody = content_pane().add_rml("");
        const auto* cached = self_update::last_check();
        if (cached != nullptr && mode != ChangelogMode::CheckNow) {
            mCheck = *cached;
            show();
        } else {
            set_message(i18n::tr_str(
                mode == ChangelogMode::CheckNow ? "prelaunch.update.checking" : "changelog.loading"));
            mTask = self_update::check_for_update(
                self_update::follows_betas(), i18n::current_language());
        }
        refresh_status();
    }

    void update() override {
        if (mTask && mTask.ready()) {
            std::optional<self_update::UpdateCheck> result;
            try {
                result = mTask.try_take();
            } catch (const std::exception&) {
            }
            mTask = {};
            if (result && result->result.status != borealis::update::Status::Failed) {
                self_update::set_last_check(*result);
                mCheck = std::move(result);
                show();
            } else {
                set_message(i18n::tr_str("changelog.failed"));
            }
        }
        refresh_status();
        Modal::update();
    }

protected:
    bool handle_nav_command(Rml::Event& event, NavCommand cmd) override {
        // The notes are text only: the D-pad/stick scrolls them, L1/R1 by pages.
        float step = 0.0f;
        const float page = std::max(content_pane().root()->GetClientHeight() - 40.0f, 80.0f);
        switch (cmd) {
        case NavCommand::Up:
            step = -80.0f;
            break;
        case NavCommand::Down:
            step = 80.0f;
            break;
        case NavCommand::Previous:
            step = -page;
            break;
        case NavCommand::Next:
            step = page;
            break;
        default:
            return Modal::handle_nav_command(event, cmd);
        }
        auto* scroller = content_pane().root();
        scroller->SetScrollTop(std::clamp(scroller->GetScrollTop() + step, 0.0f,
            std::max(0.0f, scroller->GetScrollHeight() - scroller->GetClientHeight())));
        return true;
    }

private:
    static std::vector<ModalAction> make_actions(ChangelogMode mode, ChangelogModal* self) {
        std::vector<ModalAction> actions{ModalAction{
            .label = i18n::tr_str("changelog.close"),
            .onPressed = [](Modal& modal) {
                mDoAud_seStartMenu(kSoundWindowClose);
                modal.pop();
            },
        }};
        if (mode != ChangelogMode::History) {
            actions.push_back(ModalAction{
                .label = i18n::tr_str(
                    self_update::supported() ? "changelog.install" : "changelog.download"),
                .onPressed = [self](Modal&) { self->install(); },
                .isDisabled = [self] { return !self->can_install(); },
            });
        }
        return actions;
    }

    bool update_available() const {
        return mCheck && mCheck->result.status == borealis::update::Status::UpdateAvailable;
    }

    bool can_install() const {
        const auto state = self_update::state();
        return update_available() && state != self_update::State::Downloading &&
               state != self_update::State::Verifying && state != self_update::State::Installed;
    }

    // Install in place from an AppImage; otherwise open the release page. The
    // window stays open and shows the progress, in game or on the start screen.
    void install() {
        if (!can_install()) {
            return;
        }
        mDoAud_seStartMenu(kSoundClick);
        const auto& latest = mCheck->result.latest;
        if (self_update::start(latest)) {
            return;
        }
        if (!latest.htmlUrl.empty()) {
            SDL_OpenURL(latest.htmlUrl.c_str());
        }
    }

    std::string target_version() const {
        if (!mCheck) {
            return {};
        }
        std::string version = mCheck->result.latest.tagName;
        version = version.substr(0, version.find('+'));
        if (!version.empty() && version.front() == 'v') {
            version.erase(0, 1);
        }
        return version;
    }

    void refresh_status() {
        std::string text;
        if (mMode != ChangelogMode::History) {
            switch (self_update::state()) {
            case self_update::State::Downloading: {
                const auto done = self_update::progress_completed();
                const auto total = self_update::progress_total();
                const int percent = total && *total > 0 ? static_cast<int>(done * 100 / *total) : 0;
                text = fmt::format(fmt::runtime(i18n::tr_source("prelaunch.update.downloading")),
                    fmt::arg("percent", percent));
                break;
            }
            case self_update::State::Verifying:
                text = i18n::tr_str("prelaunch.update.verifying");
                break;
            case self_update::State::Installed:
                text = fmt::format(fmt::runtime(i18n::tr_source("prelaunch.update.installed")),
                    fmt::arg("version", target_version()));
                break;
            case self_update::State::Failed:
                text = i18n::tr_str("prelaunch.update.install_failed");
                break;
            case self_update::State::Idle:
                if (update_available()) {
                    text = fmt::format(fmt::runtime(i18n::tr_source("changelog.available")),
                        fmt::arg("version", target_version()));
                }
                break;
            }
        }
        if (text != mStatusText) {
            mStatusText = text;
            mStatus->SetInnerRML(
                text.empty() ? "" : "<changelog-status>" + escape(text) + "</changelog-status>");
        }
    }

    void set_message(const std::string& text) {
        mBody->SetInnerRML("<changelog-empty>" + escape(text) + "</changelog-empty>");
    }

    void show() {
        const auto& entries = mMode == ChangelogMode::History ? mCheck->history : mCheck->whatsNew;
        if (entries.empty()) {
            set_message(i18n::tr_str(
                mMode == ChangelogMode::History ? "changelog.empty" : "changelog.up_to_date"));
            return;
        }
        mBody->SetInnerRML(entries_rml(entries));
    }

    ChangelogMode mMode;
    Rml::Element* mStatus = nullptr;
    Rml::Element* mBody = nullptr;
    std::string mStatusText;
    std::optional<self_update::UpdateCheck> mCheck;
    borealis::Task<self_update::UpdateCheck> mTask;
};

}  // namespace

void open_changelog(ChangelogMode mode) {
    ensure_styles();
    auto& doc = push_document(std::make_unique<ChangelogModal>(mode));
    apply_scoped_styles(doc);
    doc.focus();
}

bool has_whats_new() noexcept {
    const auto* check = self_update::last_check();
    return check != nullptr && !check->whatsNew.empty();
}

}  // namespace dusk::ui
