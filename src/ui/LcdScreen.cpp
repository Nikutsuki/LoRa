#include "LcdScreen.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

namespace {

/*!
 * Builds "prefix + text + suffix" into \p out, truncating rather than overflowing.
 */
void formatLabel(char *out, size_t outSize, const char *prefix, const char *text, const char *suffix)
{
    if (outSize == 0) {
        return;
    }
    std::snprintf(out, outSize, "%s%s%s",
                  prefix != nullptr ? prefix : "",
                  text != nullptr ? text : "",
                  suffix != nullptr ? suffix : "");
}

}

LcdScreen::LcdScreen(Ili9341Display &display, const Theme &theme)
    : display_(display), theme_(theme)
{
}

bool sameView(const ComponentView &a, const ComponentView &b)
{
    return a.kind == b.kind &&
           a.bounds.x == b.bounds.x &&
           a.bounds.y == b.bounds.y &&
           a.bounds.w == b.bounds.w &&
           a.bounds.h == b.bounds.h &&
           a.color == b.color &&
           a.statusColor == b.statusColor &&
           a.scale == b.scale &&
           a.focused == b.focused &&
           a.selected == b.selected &&
           a.focusable == b.focusable &&
           std::strcmp(a.primary, b.primary) == 0 &&
           std::strcmp(a.secondary, b.secondary) == 0;
}

void LcdScreen::present(const ScreenModel &model)
{
    const bool full = model.fullRedraw() || !hasPresented_ || lastPresented_.count() != model.count();
    if (full) {
        display_.fillRect(0, 0, display_.width(), display_.height(), theme_.background);
    }

    for (size_t i = 0; i < model.count(); i++) {
        const ComponentView &view = model.at(i);
        if (!full && sameView(view, lastPresented_.at(i))) {
            continue;
        }
        renderComponent(view);
    }

    lastPresented_ = model;
    hasPresented_ = true;
}

void LcdScreen::renderComponent(const ComponentView &view)
{
    const uint16_t x = view.bounds.x;
    const uint16_t y = view.bounds.y;
    const uint16_t w = view.bounds.w;
    const uint16_t h = view.bounds.h;

    switch (view.kind) {
    case ComponentKind::Label: {
        // A header carries right aligned text, so only a plain label is cleared
        // and drawn on the page background.
        display_.fillRect(x, y, w, h, theme_.background);
        display_.drawText(x, y, view.primary, view.color, theme_.background, view.scale);

        if (view.statusColor != 0) {
            this->display_.fillRect(static_cast<uint16_t>(x + w - 8), static_cast<uint16_t>(y + 4), 4, 4, view.statusColor);
        }

        if (view.secondary[0] != '\0') {
            const uint16_t advance = static_cast<uint16_t>(8 * view.scale);
            size_t length = 0;
            while (view.secondary[length] != '\0') {
                length++;
            }
            const uint16_t textWidth = static_cast<uint16_t>(length * advance);
            const uint16_t rightX = textWidth < w ? static_cast<uint16_t>(x + w - textWidth - 4) : x;
            display_.drawText(rightX, y, view.secondary, theme_.muted, theme_.background, view.scale);
        }
        break;
    }

    case ComponentKind::Divider:
        display_.fillRect(x, y, w, h > 0 ? h : 1, view.color);
        break;

    case ComponentKind::StatusBar:
        display_.fillRect(x, y, w, h, theme_.surface);
        display_.drawText(static_cast<uint16_t>(x + 6), static_cast<uint16_t>(y + 3),
                          view.primary, view.color, theme_.surface, 1);
        break;

    case ComponentKind::Button: {
        const uint16_t fill = view.focused ? theme_.focus : theme_.surface;
        const uint16_t text = view.focused ? Ili9341Display::Black : theme_.text;
        // A focused button is distinguished by its filled background, not by
        // bracketing the label: the buttons are sized for the bare text and
        // adding brackets overflows them.
        display_.fillRect(x, y, w, h, fill);
        display_.drawRect(x, y, w, h, view.focused ? theme_.text : theme_.muted);
        display_.drawText(static_cast<uint16_t>(x + 5), static_cast<uint16_t>(y + 6),
                          view.primary, text, fill, 1);
        break;
    }

    case ComponentKind::TextBox: {
        const uint16_t fill = view.focused ? theme_.surfaceAlt : theme_.surface;
        display_.fillRect(x, y, w, h, fill);
        display_.drawRect(x, y, w, h, view.focused ? theme_.focus : theme_.muted);
        display_.drawText(static_cast<uint16_t>(x + 5), static_cast<uint16_t>(y + 7), view.primary, theme_.text, fill, 1);
        break;
    }

    case ComponentKind::ListItem: {
        uint16_t fill = view.selected ? theme_.surfaceAlt : theme_.surface;
        if (view.focused) {
            fill = theme_.focus;
        }
        const uint16_t text = view.focused ? Ili9341Display::Black : theme_.text;
        const uint16_t secondary = view.focused ? Ili9341Display::Black : theme_.muted;
        display_.fillRect(x, y, w, h, fill);
        display_.drawRect(x, y, w, h, view.focused ? theme_.text : theme_.muted);
        display_.drawText(static_cast<uint16_t>(x + 5), static_cast<uint16_t>(y + 4), view.primary, text, fill, 1);
        display_.drawText(static_cast<uint16_t>(x + 5), static_cast<uint16_t>(y + 15), view.secondary, secondary, fill, 1);
        break;
    }
    }
}

}
