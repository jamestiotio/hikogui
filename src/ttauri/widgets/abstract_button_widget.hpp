// Copyright 2020 Pokitec
// All rights reserved.

#pragma once

#include "widget.hpp"

namespace tt {

/** An abstract button widget.
 * This widgets implements the behavior for a widget where its whole
 * area is clickable, accepts and responds to gui_activate commands.
 */
template<typename T>
class abstract_button_widget : public widget {
public:
    using super = widget;
    using value_type = T;

    value_type const true_value;
    observable<value_type> value;

    using notifier_type = notifier<void()>;
    using callback_type = typename notifier_type::callback_type;
    using callback_ptr_type = typename notifier_type::callback_ptr_type;

    template<typename Value = observable<value_type>>
    [[nodiscard]] abstract_button_widget(
        gui_window &window,
        std::shared_ptr<widget> parent,
        value_type true_value,
        Value &&value = {}) :
        widget(window, parent), true_value(std::move(true_value)), value(std::forward<Value>(value))
    {
    }

    draw_context make_draw_context(draw_context context) const noexcept override
    {
        auto new_context = super::make_draw_context(context);

        if (_pressed) {
            new_context.fill_color = theme::global->fillColor(this->_semantic_layer + 2);
        }

        return new_context;
    }

    [[nodiscard]] bool accepts_keyboard_focus(keyboard_focus_group group) const noexcept
    {
        tt_axiom(gui_system_mutex.recurse_lock_count());
        return is_normal(group) && *enabled;
    }

    [[nodiscard]] bool handle_command(command command) noexcept
    {
        ttlet lock = std::scoped_lock(gui_system_mutex);

        if (*enabled) {
            switch (command) {
            case command::gui_activate: this->_notifier(); return true;
            case command::gui_enter:
                this->_notifier();
                this->window.update_keyboard_target(
                    this->shared_from_this(), keyboard_focus_group::normal, keyboard_focus_direction::forward);
                return true;
            default:;
            }
        }

        return super::handle_command(command);
    }

    [[nodiscard]] bool handle_mouse_event(MouseEvent const &event) noexcept final
    {
        ttlet lock = std::scoped_lock(gui_system_mutex);
        auto handled = super::handle_mouse_event(event);

        if (event.cause.leftButton) {
            handled = true;
            if (*enabled) {
                if (compare_then_assign(_pressed, static_cast<bool>(event.down.leftButton))) {
                    window.request_redraw(window_clipping_rectangle());
                }

                if (event.type == MouseEvent::Type::ButtonUp && _window_rectangle.contains(event.position)) {
                    handled |= handle_command(command::gui_activate);
                }
            }
        }
        return handled;
    }

    [[nodiscard]] HitBox hitbox_test(f32x4 window_position) const noexcept final
    {
        ttlet lock = std::scoped_lock(gui_system_mutex);

        if (_window_clipping_rectangle.contains(window_position)) {
            return HitBox{weak_from_this(), _draw_layer, *enabled ? HitBox::Type::Button : HitBox::Type::Default};
        } else {
            return HitBox{};
        }
    }

    /** Subscribe a callback to call when the button is activated.
     * @see notifier::subscribe()
     */
    template<typename Callback>
    [[nodiscard]] callback_ptr_type subscribe(Callback &&callback) noexcept
    {
        return _notifier.subscribe(std::forward<Callback>(callback));
    }

    /** Unsubscribe a callback.
     * @see notifier::subscribe()
     */
    void unsubscribe(callback_ptr_type &callback_ptr) noexcept
    {
        return _notifier.unsubscribe(callback_ptr);
    }

protected:
    /** The button is in a pressed state.
     */
    bool _pressed = false;

private:
    notifier_type _notifier;
};

} // namespace tt
