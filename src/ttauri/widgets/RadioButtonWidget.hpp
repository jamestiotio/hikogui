// Copyright 2020 Pokitec
// All rights reserved.

#pragma once

#include "Widget.hpp"
#include "../GUI/DrawContext.hpp"
#include "../text/format10.hpp"
#include "../observable.hpp"
#include <memory>
#include <string>
#include <array>
#include <optional>
#include <future>

namespace tt {

template<typename ValueType>
class RadioButtonWidget : public Widget {
protected:
    aarect radioButtonRectangle;
    aarect pipRectangle;
    aarect labelRectangle;

    std::unique_ptr<TextCell> labelCell;

    ValueType activeValue;

public:
    observable<ValueType> value;
    observable<std::string> label;

    RadioButtonWidget(Window &window, Widget *parent, ValueType activeValue) noexcept :
        Widget(window, parent),
        activeValue(std::move(activeValue)),
        value(),
        label()
    {
        [[maybe_unused]] ttlet value_cbid = value.add_callback([this](auto...){
            this->window.requestRedraw = true;
        });
        [[maybe_unused]] ttlet label_cbid = label.add_callback([this](auto...){
            requestConstraint = true;
        });
    }

    ~RadioButtonWidget() {
    }

    [[nodiscard]] bool updateConstraints() noexcept override {
        if (!Widget::updateConstraints()) {
            return false;
        }

        ttlet lock = std::scoped_lock(mutex);
        labelCell = std::make_unique<TextCell>(*label, theme->labelStyle);

        ttlet minimumHeight = std::max(labelCell->preferredExtent().height(), Theme::smallSize);
        ttlet minimumWidth = labelCell->preferredExtent().width() + Theme::smallSize + Theme::margin * 2.0f;

        window.stopConstraintSolver();
        window.replaceConstraint(minimumWidthConstraint, width >= minimumWidth);
        window.replaceConstraint(minimumHeightConstraint, height >= minimumHeight);
        window.replaceConstraint(baseConstraint, base == top - Theme::smallSize * 0.5f);
        window.startConstraintSolver();
        return true;
    }

    [[nodiscard]] bool updateLayout(hires_utc_clock::time_point displayTimePoint, bool forceLayout) noexcept override {
        if (!Widget::updateLayout(displayTimePoint, forceLayout)) {
            return false;
        }

        ttlet lock = std::scoped_lock(mutex);

        radioButtonRectangle = aarect{
            0.0f,
            baseHeight() - Theme::smallSize * 0.5f,
            Theme::smallSize,
            Theme::smallSize
        };

        ttlet labelX = radioButtonRectangle.p3().x() + Theme::margin;
        labelRectangle = aarect{
            labelX,
            0.0f,
            rectangle().width() - labelX,
            rectangle().height()
        };

        pipRectangle = shrink(radioButtonRectangle, 1.5f);
        return true;
    }

    void drawRadioButton(DrawContext drawContext) noexcept {
        drawContext.cornerShapes = vec{radioButtonRectangle.height() * 0.5f};
        drawContext.drawBoxIncludeBorder(radioButtonRectangle);
    }

    void drawPip(DrawContext drawContext) noexcept {
        // draw pip
        if (value == activeValue) {
            if (*enabled && window.active) {
                drawContext.color = theme->accentColor;
            }
            std::swap(drawContext.color, drawContext.fillColor);
            drawContext.cornerShapes = vec{pipRectangle.height() * 0.5f};
            drawContext.drawBoxIncludeBorder(pipRectangle);
        }
    }

    void drawLabel(DrawContext drawContext) noexcept {
        if (*enabled) {
            drawContext.color = theme->labelStyle.color;
        }

        labelCell->draw(drawContext, labelRectangle, Alignment::TopLeft, baseHeight(), true);
    }

    void draw(DrawContext const &drawContext, hires_utc_clock::time_point displayTimePoint) noexcept override {
        drawRadioButton(drawContext);
        drawPip(drawContext);
        drawLabel(drawContext);
        Widget::draw(drawContext, displayTimePoint);
    }

    void handleMouseEvent(MouseEvent const &event) noexcept override {
        Widget::handleMouseEvent(event);

        if (*enabled) {
            if (
                event.type == MouseEvent::Type::ButtonUp &&
                event.cause.leftButton &&
                rectangle().contains(event.position)
            ) {
                handleCommand(command::gui_activate);
            }
        }
    }

    void handleCommand(command command) noexcept override {
        if (!*enabled) {
            return;
        }

        if (command == command::gui_activate) {
            if (assign_and_compare(value, activeValue)) {
                window.requestRedraw = true;
            }
        }
        Widget::handleCommand(command);
    }

    [[nodiscard]] HitBox hitBoxTest(vec position) const noexcept override {
        if (rectangle().contains(position)) {
            return HitBox{this, elevation, *enabled ? HitBox::Type::Button : HitBox::Type::Default};
        } else {
            return HitBox{};
        }
    }

    [[nodiscard]] bool acceptsFocus() const noexcept override {
        return *enabled;
    }
};


}
