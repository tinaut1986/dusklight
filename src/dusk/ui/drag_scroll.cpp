#include "drag_scroll.hpp"

#include <RmlUi/Core/ComputedValues.h>
#include <RmlUi/Core/Context.h>
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core/EventListener.h>
#include <RmlUi/Core/SystemInterface.h>
#include <RmlUi/Core/Core.h>

#include <algorithm>
#include <cmath>

namespace dusk::ui {
namespace {

// How far (dp) a press must travel before it becomes a scroll instead of a tap.
constexpr float kDragThresholdDp = 12.0f;

bool scrolls(Rml::Style::Overflow overflow) {
    return overflow == Rml::Style::Overflow::Auto || overflow == Rml::Style::Overflow::Scroll;
}

/* True when the press started on something that drags by itself. */
bool has_own_drag(Rml::Element* element) {
    for (auto* e = element; e != nullptr; e = e->GetParentNode()) {
        if (e->GetComputedValues().drag() != Rml::Style::Drag::None) {
            return true;
        }
    }
    return false;
}

Rml::Element* scroll_container(Rml::Element* element, bool vertical) {
    for (auto* e = element; e != nullptr; e = e->GetParentNode()) {
        const auto& values = e->GetComputedValues();
        if (vertical) {
            if (scrolls(values.overflow_y()) && e->GetScrollHeight() > e->GetClientHeight() + 1.0f) {
                return e;
            }
        } else if (scrolls(values.overflow_x()) && e->GetScrollWidth() > e->GetClientWidth() + 1.0f) {
            return e;
        }
    }
    return nullptr;
}

class DragScrollListener final : public Rml::EventListener {
public:
    void ProcessEvent(Rml::Event& event) override {
        switch (event.GetId()) {
        case Rml::EventId::Mousedown:
            on_down(event);
            break;
        case Rml::EventId::Mousemove:
            on_move(event);
            break;
        case Rml::EventId::Mouseup:
            mPressed = false;
            mReleasedAt = now();
            break;
        case Rml::EventId::Click:
        case Rml::EventId::Dblclick:
            // The press turned into a scroll: the click RmlUi sends on that same
            // release must not activate a button. Only that one: a later click
            // (another tap, the gamepad) goes through.
            if (mSwallowClick && now() - mReleasedAt < 0.25) {
                event.StopPropagation();
            }
            mSwallowClick = false;
            break;
        default:
            break;
        }
    }

private:
    void on_down(Rml::Event& event) {
        mPressed = false;
        mDragging = false;
        mSwallowClick = false;
        if (event.GetParameter<int>("button", -1) != 0) {
            return;
        }
        auto* target = event.GetTargetElement();
        if (target == nullptr || has_own_drag(target)) {
            return;
        }
        auto* vertical = scroll_container(target, true);
        auto* horizontal = scroll_container(target, false);
        if (vertical == nullptr && horizontal == nullptr) {
            return;
        }
        mVertical = vertical ? vertical->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>{};
        mHorizontal = horizontal ? horizontal->GetObserverPtr() : Rml::ObserverPtr<Rml::Element>{};
        mPressed = true;
        mStartX = event.GetParameter<float>("mouse_x", 0.0f);
        mStartY = event.GetParameter<float>("mouse_y", 0.0f);
        mStartTop = vertical ? vertical->GetScrollTop() : 0.0f;
        mStartLeft = horizontal ? horizontal->GetScrollLeft() : 0.0f;
    }

    void on_move(Rml::Event& event) {
        if (!mPressed) {
            return;
        }
        const float x = event.GetParameter<float>("mouse_x", mStartX);
        const float y = event.GetParameter<float>("mouse_y", mStartY);
        const float dx = x - mStartX;
        const float dy = y - mStartY;
        if (!mDragging) {
            auto* context = event.GetTargetElement() ? event.GetTargetElement()->GetContext() : nullptr;
            const float ratio = context ? context->GetDensityIndependentPixelRatio() : 1.0f;
            if (std::hypot(dx, dy) < kDragThresholdDp * ratio) {
                return;
            }
            mDragging = true;
            mSwallowClick = true;
        }
        if (auto* v = mVertical.get()) {
            const float max = std::max(0.0f, v->GetScrollHeight() - v->GetClientHeight());
            v->SetScrollTop(std::clamp(mStartTop - dy, 0.0f, max));
        }
        if (auto* h = mHorizontal.get()) {
            const float max = std::max(0.0f, h->GetScrollWidth() - h->GetClientWidth());
            h->SetScrollLeft(std::clamp(mStartLeft - dx, 0.0f, max));
        }
        event.StopPropagation();
    }

    bool mPressed = false;
    bool mDragging = false;
    bool mSwallowClick = false;
    float mStartX = 0.0f;
    float mStartY = 0.0f;
    float mStartTop = 0.0f;
    float mStartLeft = 0.0f;
    double mReleasedAt = -1.0;
    Rml::ObserverPtr<Rml::Element> mVertical;
    Rml::ObserverPtr<Rml::Element> mHorizontal;

    static double now() {
        auto* system = Rml::GetSystemInterface();
        return system ? system->GetElapsedTime() : 0.0;
    }
};

DragScrollListener gListener;

}  // namespace

void install_drag_scroll(Rml::Context& context) noexcept {
    auto* root = context.GetRootElement();
    if (root == nullptr) {
        return;
    }
    for (auto id : {Rml::EventId::Mousedown, Rml::EventId::Mousemove, Rml::EventId::Mouseup,
             Rml::EventId::Click, Rml::EventId::Dblclick})
    {
        root->AddEventListener(id, &gListener, true);
    }
}

}  // namespace dusk::ui
