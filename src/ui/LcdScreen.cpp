#include "LcdScreen.hpp"

#include <cstdio>

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

void LcdScreen::present(const ScreenModel &model)
{
    if (model.fullRedraw()) {
        display_.fillRect(0, 0, display_.width(), display_.height(), theme_.background);
    }

    for (size_t i = 0; i < model.count(); i++) {
        renderComponent(model.at(i));
    }
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
