// Copyright 2019 Pokitec
// All rights reserved.

#include "TTauri/GUI/Window_base.hpp"
#include "TTauri/GUI/Window.hpp"
#include "TTauri/GUI/GUIDevice.hpp"
#include "TTauri/Widgets/WindowWidget.hpp"

namespace tt {

using namespace std;

Window_base::Window_base(std::shared_ptr<WindowDelegate> const &delegate, Label &&title) :
    state(State::Initializing),
    delegate(delegate),
    title(std::move(title))
{
}

Window_base::~Window_base()
{
    // Destroy the top-level widget, before automatic destruction of the constraint solver
    // and other objects that the widgets require from the window during their destruction.
    widget.release();

    try {
        gsl_suppress(f.6) {
            if (state != State::NoWindow) {
                LOG_FATAL("Window '{}' was not properly teardown before destruction.", title.text());
            }
            LOG_INFO("Window '{}' has been propertly destructed.", title.text());
        }
    } catch (...) {
        abort();
    }
}

void Window_base::initialize()
{
    auto lock = std::scoped_lock(guiMutex);

    widget = std::make_unique<WindowWidget>(*static_cast<Window *>(this), title);

    // The width and height of the window will be modified by the user and also for
    // testing the minimum and maximum size of the window.
    widgetSolver.add_stay(widget->width, rhea::strength::medium());
    widgetSolver.add_stay(widget->height, rhea::strength::medium());

    openingWindow();

    // Finished initializing the window.
    state = State::NoDevice;
}

bool Window_base::isClosed() {
    auto lock = std::scoped_lock(guiMutex);
    return state == State::NoWindow;
}

rhea::constraint Window_base::addConstraint(rhea::constraint const& constraint) noexcept {
    auto lock = std::scoped_lock(widgetSolverMutex);

    widgetSolver.add_constraint(constraint);
    constraintsUpdated = true;
    return constraint;
}

rhea::constraint Window_base::addConstraint(
    rhea::linear_equation const& equation,
    rhea::strength const &strength,
    double weight
) noexcept {
    // Since equation has not been assigned to the solver, we do not need to lock
    // the widgetSolverMutex.

    return addConstraint(rhea::constraint(equation, strength, weight));
}

rhea::constraint Window_base::addConstraint(
    rhea::linear_inequality const& equation,
    rhea::strength const &strength,
    double weight
) noexcept {
    // Since equation has not been assigned to the solver, we do not need to lock
    // the widgetSolverMutex.

    return addConstraint(rhea::constraint(equation, strength, weight));
}

void Window_base::removeConstraint(rhea::constraint const& constraint) noexcept {
    auto lock = std::scoped_lock(widgetSolverMutex);

    widgetSolver.remove_constraint(constraint);
    constraintsUpdated = true;
}

rhea::constraint Window_base::replaceConstraint(
    rhea::constraint const &oldConstraint,
    rhea::constraint const &newConstraint
) noexcept {
    auto lock = std::scoped_lock(widgetSolverMutex);

    widgetSolver.remove_constraint(oldConstraint);
    widgetSolver.add_constraint(newConstraint);
    constraintsUpdated = true;
    return newConstraint;
}

rhea::constraint Window_base::replaceConstraint(
    rhea::constraint const &oldConstraint,
    rhea::linear_equation const &newEquation,
    rhea::strength const &strength,
    double weight
) noexcept {
    // Since equation has not been assigned to the solver, we do not need to lock
    // the widgetSolverMutex.

    return replaceConstraint(oldConstraint, rhea::constraint(newEquation, strength, weight));
}

rhea::constraint Window_base::replaceConstraint(
    rhea::constraint const &oldConstraint,
    rhea::linear_inequality const &newEquation,
    rhea::strength const &strength,
    double weight
) noexcept {
    // Since equation has not been assigned to the solver, we do not need to lock
    // the widgetSolverMutex.

    return replaceConstraint(oldConstraint, rhea::constraint(newEquation, strength, weight));
}

void Window_base::layout(hires_utc_clock::time_point displayTimePoint)
{
    auto force = forceLayout.exchange(false);
    auto need = layoutChildren(displayTimePoint, force);
    if (force || need >= 1) {
        forceRedraw = true;
    }
}

int Window_base::layoutChildren(hires_utc_clock::time_point displayTimePoint, bool force) {
    constexpr int layout_retries = 10;

    auto total_need = 0;

    for (auto i = 0; i != layout_retries; ++i) {
        ttlet child_need = widget->needs(displayTimePoint);
        total_need |= child_need;

        if (force || child_need >= 2) {
            widget->layout(displayTimePoint);
        }

        // Grandchildren need to be layed out when the child has changed.
        total_need |= widget->layoutChildren(displayTimePoint, force);

        // Layout may have changed the constraints, in that case recalculate them.
        if (constraintsUpdated) {
            constraintsUpdated = false;
            layoutWindow();

        } else {
            return total_need;
        }
    }
    LOG_FATAL("Unable to layout child widgets");
}

void Window_base::openingWindow() {
    Window *thisWindow = dynamic_cast<Window *>(this);
    assert(thisWindow);
    delegate->openingWindow(*thisWindow);

    // Execute a layout to determine initial window size.
    layout(cpu_utc_clock::now());

    updateToNextKeyboardTarget(nullptr);
}

void Window_base::closingWindow() {
    Window* thisWindow = dynamic_cast<Window*>(this);
    assert(thisWindow);
    delegate->closingWindow(*thisWindow);

    auto lock = std::scoped_lock(guiMutex);
    state = State::NoWindow;
}

void Window_base::setDevice(GUIDevice *newDevice)
{
    auto lock = std::scoped_lock(guiMutex);

    if (device) {
        state = State::DeviceLost;
        teardown();
    }

    device = newDevice;
}

void Window_base::updateToNextKeyboardTarget(Widget *currentTargetWidget) noexcept {
    auto tmp = widget->nextKeyboardWidget(currentTargetWidget, false);
    if (tmp == foundWidgetPtr) {
        tmp = nullptr;
    }

    updateKeyboardTarget(tmp);
}

void Window_base::updateToPrevKeyboardTarget(Widget *currentTargetWidget) noexcept {
    auto tmp = widget->nextKeyboardWidget(currentTargetWidget, true);
    if (tmp == foundWidgetPtr) {
        tmp = nullptr;
    }
    updateKeyboardTarget(tmp);
}

[[nodiscard]] float Window_base::windowScale() const noexcept {
    return std::ceil(dpi / 100.0f);
}

void Window_base::windowChangedSize(ivec extent) {
    currentWindowExtent = extent;
    suggestWidgetExtent(currentWindowExtent);
    forceLayout = true;
}

void Window_base::updateMouseTarget(Widget const *newTargetWidget, vec position) noexcept {
    if (newTargetWidget != mouseTargetWidget) {
        if (mouseTargetWidget != nullptr) {
            mouseTargetWidget->handleMouseEvent(MouseEvent::exited());
        }
        mouseTargetWidget = const_cast<Widget *>(newTargetWidget);
        if (mouseTargetWidget != nullptr) { 
            mouseTargetWidget->handleMouseEvent(MouseEvent::entered(position));
        }
    }
}

void Window_base::updateKeyboardTarget(Widget const *newTargetWidget) noexcept {
    if (newTargetWidget == nullptr || !newTargetWidget->acceptsFocus()) {
        newTargetWidget = nullptr;
    }

    if (newTargetWidget != keyboardTargetWidget) {
        if (keyboardTargetWidget != nullptr) {
            keyboardTargetWidget->handleKeyboardEvent(KeyboardEvent::exited());
        }
        keyboardTargetWidget = const_cast<Widget *>(newTargetWidget);
        if (keyboardTargetWidget != nullptr) {
            keyboardTargetWidget->handleKeyboardEvent(KeyboardEvent::entered());
        }
    }
}

void Window_base::handleMouseEvent(MouseEvent event) noexcept {
    switch (event.type) {
    case MouseEvent::Type::Exited: // Mouse left window.
        updateMouseTarget(nullptr);
        break;

    case MouseEvent::Type::ButtonDown:
    case MouseEvent::Type::Move: {
        ttlet hitbox = hitBoxTest(event.position);
        updateMouseTarget(hitbox.widget, event.position);

        if (event.type == MouseEvent::Type::ButtonDown) {
            updateKeyboardTarget(hitbox.widget);
        }
        } break;
    default:;
    }

    // Send event to target-widget.
    if (mouseTargetWidget != nullptr) {
        ttlet windowOffset = mouseTargetWidget->offsetFromWindow();
        event.position -= windowOffset;
        event.downPosition -= windowOffset;
        mouseTargetWidget->handleMouseEvent(event);
    }
}

void Window_base::handleKeyboardEvent(KeyboardEvent const &event) noexcept {
    if (keyboardTargetWidget != nullptr) {
        keyboardTargetWidget->handleKeyboardEvent(event);

    } else if (event.type == KeyboardEvent::Type::Key) {
        // If no widgets have been selected handle the keyboard-focus changes.
        for (ttlet command : event.getCommands()) {
            if (command == "gui.widget.next"_ltag) {
                updateToNextKeyboardTarget(nullptr);
            } else if (command == "gui.widget.prev"_ltag) {
                updateToPrevKeyboardTarget(nullptr);
            }
        }
    }
}

void Window_base::handleKeyboardEvent(KeyboardState _state, KeyboardModifiers modifiers, KeyboardVirtualKey key) noexcept {
    return handleKeyboardEvent(KeyboardEvent(_state, modifiers, key));
}

void Window_base::handleKeyboardEvent(Grapheme grapheme, bool full) noexcept {
    return handleKeyboardEvent(KeyboardEvent(grapheme, full));
}

void Window_base::handleKeyboardEvent(char32_t c, bool full) noexcept {
    return handleKeyboardEvent(Grapheme(c), full);
}

HitBox Window_base::hitBoxTest(vec position) const noexcept {
    return widget->hitBoxTest(position);
}

vec Window_base::suggestWidgetExtent(vec extent) noexcept {
    auto lock = std::scoped_lock(widgetSolverMutex);

    ttlet &width = widget->width;
    ttlet &height = widget->height;

    widgetSolver.suggest(width, extent.width());
    widgetSolver.suggest(height, extent.height());

    return vec{width.value(), height.value()};
}

[[nodiscard]] std::pair<vec,vec> Window_base::getMinimumAndMaximumWidgetExtent() noexcept {
    auto minimumWidgetExtent = suggestWidgetExtent(vec{0.0f, 0.0f});
    auto maximumWidgetExtent = suggestWidgetExtent(vec{
        std::numeric_limits<int>::max(),
        std::numeric_limits<int>::max()
    });
    
    return {minimumWidgetExtent, maximumWidgetExtent};
}

void Window_base::layoutWindow() noexcept {
    tt_assume(widget);

    std::tie(minimumWindowExtent, maximumWindowExtent) = getMinimumAndMaximumWidgetExtent();

    if (state != Window_base::State::Initializing) {
        if (
            (currentWindowExtent.x() < minimumWindowExtent.x()) ||
            (currentWindowExtent.y() < minimumWindowExtent.y())
        ) {
            setWindowSize(minimumWindowExtent);
        }

        if (
            (currentWindowExtent.x() > maximumWindowExtent.x()) ||
            (currentWindowExtent.y() > maximumWindowExtent.y())
        ) {
            setWindowSize(maximumWindowExtent);
        }
    }

    // Set to actual window size.
    suggestWidgetExtent(currentWindowExtent);

    LOG_INFO("Window '{}' minimumExtent={} maximumExtent={} currentExtent={}",
        title.text(),
        minimumWindowExtent,
        maximumWindowExtent,
        currentWindowExtent
    );

}

}
