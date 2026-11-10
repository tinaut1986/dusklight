#include "changelog_window.hpp"

#include "dusk/self_update.hpp"
#include "i18n.hpp"
#include "modal.hpp"
#include "ui.hpp"

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
    ChangelogModal(ChangelogMode mode, std::function<void()> onInstall)
        : Modal{Props{
              .title = i18n::tr_str(mode == ChangelogMode::WhatsNew ? "changelog.title_whats_new"
                                                                    : "changelog.title_history"),
              .bodyRml = "",
              .actions = make_actions(mode, onInstall),
              .onDismiss = [](Modal& modal) {
                  mDoAud_seStartMenu(kSoundWindowClose);
                  modal.pop();
              },
              .variant = "changelog",
          }},
          mMode{mode} {
        mBody = content_pane().add_rml("");
        if (const auto* check = self_update::last_check()) {
            show(*check);
        } else {
            set_message(i18n::tr_str("changelog.loading"));
            mTask = self_update::check_for_update(
                self_update::follows_betas(), i18n::current_language());
        }
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
                show(*result);
            } else {
                set_message(i18n::tr_str("changelog.failed"));
            }
        }
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
    static std::vector<ModalAction> make_actions(
        ChangelogMode mode, const std::function<void()>& onInstall) {
        std::vector<ModalAction> actions{ModalAction{
            .label = i18n::tr_str("changelog.close"),
            .onPressed = [](Modal& modal) {
                mDoAud_seStartMenu(kSoundWindowClose);
                modal.pop();
            },
        }};
        const auto* check = self_update::last_check();
        if (mode == ChangelogMode::WhatsNew && onInstall && check != nullptr &&
            check->result.status == borealis::update::Status::UpdateAvailable)
        {
            const auto& latest = check->result.latest;
            std::string version = latest.tagName.substr(0, latest.tagName.find('+'));
            if (!version.empty() && version.front() == 'v') {
                version.erase(0, 1);
            }
            const bool canInstall =
                self_update::supported() && self_update::find_asset(latest) != nullptr;
            actions.push_back(ModalAction{
                .label = fmt::format(fmt::runtime(i18n::tr_source(canInstall ? "prelaunch.update.install"
                                                                             : "prelaunch.update.download")),
                    fmt::arg("version", version)),
                .onPressed = [onInstall](Modal& modal) {
                    // Copy first: popping may destroy this action and its captures.
                    const auto install = onInstall;
                    modal.pop();
                    install();
                },
            });
        }
        return actions;
    }

    void set_message(const std::string& text) {
        mBody->SetInnerRML("<changelog-empty>" + escape(text) + "</changelog-empty>");
    }

    void show(const self_update::UpdateCheck& check) {
        const auto& entries = mMode == ChangelogMode::WhatsNew ? check.whatsNew : check.history;
        if (entries.empty()) {
            set_message(i18n::tr_str(mMode == ChangelogMode::WhatsNew ? "changelog.up_to_date"
                                                                      : "changelog.empty"));
            return;
        }
        mBody->SetInnerRML(entries_rml(entries));
    }

    ChangelogMode mMode;
    Rml::Element* mBody = nullptr;
    borealis::Task<self_update::UpdateCheck> mTask;
};

}  // namespace

void open_changelog(ChangelogMode mode, std::function<void()> onInstall) {
    ensure_styles();
    auto& doc = push_document(std::make_unique<ChangelogModal>(mode, std::move(onInstall)));
    apply_scoped_styles(doc);
    doc.focus();
}

bool has_whats_new() noexcept {
    const auto* check = self_update::last_check();
    return check != nullptr && !check->whatsNew.empty();
}

}  // namespace dusk::ui
