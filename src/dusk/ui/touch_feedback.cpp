#include "touch_feedback.hpp"

#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Core.h>
#include <RmlUi/Core/ElementDocument.h>
#include <RmlUi/Core/PropertyIdSet.h>
#include <RmlUi/Core/SystemInterface.h>
#include <SDL3/SDL_touch.h>
#include <aurora/rmlui.hpp>

#include <algorithm>
#include <vector>

namespace dusk::ui::touch_feedback {
namespace {

// Theme accent: --color-accent in res/rml/theme.rcss (194, 164, 45).
constexpr auto kDocument = R"RML(<rml>
<head>
<style>
body {
    position: absolute;
    left: 0;
    top: 0;
    width: 100%;
    height: 100%;
    pointer-events: none;
    z-index: 10000;
}
touch-mark, touch-mark dot {
    pointer-events: none;
}
touch-mark {
    display: block;
    position: absolute;
    width: 64dp;
    height: 64dp;
    margin-left: -32dp;
    margin-top: -32dp;
    border-radius: 32dp;
    border: 3dp rgba(194, 164, 45, 235);
    background-color: rgba(194, 164, 45, 45);
    transform: scale(0.45);
    opacity: 0;
    transition: transform opacity 0.12s cubic-out;
}
touch-mark.down {
    transform: scale(1);
    opacity: 1;
}
touch-mark.up {
    transform: scale(1.9);
    opacity: 0;
    transition: transform opacity 0.35s cubic-out;
}
touch-mark dot {
    display: block;
    position: absolute;
    left: 26dp;
    top: 26dp;
    width: 6dp;
    height: 6dp;
    border-radius: 3dp;
    background-color: rgba(255, 244, 200, 230);
}
</style>
</head>
<body></body>
</rml>)RML";

constexpr double kFadeSeconds = 0.4;

struct Mark {
    SDL_FingerID finger = 0;
    Rml::Element* element = nullptr;
    bool pressed = true;   // finger still down
    bool shown = false;    // "down" class applied (one frame after creation)
    double removeAt = 0.0; // after release
};

Rml::ElementDocument* gDocument = nullptr;
std::vector<Mark> gMarks;

double now() {
    auto* system = Rml::GetSystemInterface();
    return system ? system->GetElapsedTime() : 0.0;
}

Rml::Context* context() {
    return aurora::rmlui::is_initialized() ? aurora::rmlui::get_context() : nullptr;
}

Rml::ElementDocument* document() {
    if (gDocument == nullptr) {
        if (auto* ctx = context()) {
            gDocument = ctx->LoadDocumentFromMemory(kDocument, "fork-touch-feedback");
        }
    }
    return gDocument;
}

void place(Rml::Element* element, const SDL_TouchFingerEvent& finger) {
    auto* ctx = context();
    if (ctx == nullptr) {
        return;
    }
    const auto size = ctx->GetDimensions();
    element->SetProperty(Rml::PropertyId::Left, Rml::Property(finger.x * static_cast<float>(size.x), Rml::Unit::PX));
    element->SetProperty(Rml::PropertyId::Top, Rml::Property(finger.y * static_cast<float>(size.y), Rml::Unit::PX));
}

Mark* find(SDL_FingerID id) {
    for (auto& mark : gMarks) {
        if (mark.pressed && mark.finger == id) {
            return &mark;
        }
    }
    return nullptr;
}

void release(SDL_FingerID id) {
    if (auto* mark = find(id)) {
        mark->pressed = false;
        mark->element->SetClass("down", false);
        mark->element->SetClass("up", true);
        mark->removeAt = now() + kFadeSeconds;
    }
}

}  // namespace

void handle_event(const SDL_Event& event) noexcept {
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN: {
        if (SDL_GetTouchDeviceType(event.tfinger.touchID) != SDL_TOUCH_DEVICE_DIRECT) {
            return;
        }
        auto* doc = document();
        if (doc == nullptr) {
            return;
        }
        release(event.tfinger.fingerID);  // a stale mark for a reused id
        auto element = doc->CreateElement("touch-mark");
        element->AppendChild(doc->CreateElement("dot"));
        auto* mark = doc->AppendChild(std::move(element));
        place(mark, event.tfinger);
        gMarks.push_back({.finger = event.tfinger.fingerID, .element = mark});
        doc->Show(Rml::ModalFlag::None, Rml::FocusFlag::None);
        doc->PullToFront();
        break;
    }
    case SDL_EVENT_FINGER_MOTION:
        if (auto* mark = find(event.tfinger.fingerID)) {
            place(mark->element, event.tfinger);
        }
        break;
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED:
        release(event.tfinger.fingerID);
        break;
    default:
        break;
    }
}

void update() noexcept {
    if (gDocument == nullptr || gMarks.empty()) {
        return;
    }
    const double t = now();
    for (auto& mark : gMarks) {
        // Applied one frame after creation so the grow-in transition runs.
        if (!mark.shown && mark.pressed) {
            mark.element->SetClass("down", true);
            mark.shown = true;
        }
    }
    std::erase_if(gMarks, [t](Mark& mark) {
        if (mark.pressed || t < mark.removeAt) {
            return false;
        }
        mark.element->GetParentNode()->RemoveChild(mark.element);
        return true;
    });
    if (gMarks.empty()) {
        gDocument->Hide();
    } else {
        gDocument->PullToFront();  // stay above documents opened meanwhile
    }
}

void shutdown() noexcept {
    gMarks.clear();
    if (gDocument != nullptr) {
        gDocument->Close();
        gDocument = nullptr;
    }
}

}  // namespace dusk::ui::touch_feedback
