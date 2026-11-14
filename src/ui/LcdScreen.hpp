#pragma once

#include "../Ili9341Display.hpp"
#include "Screen.hpp"
#include "Ui.hpp"

namespace ui {

/*!
 * Screen implementation that paints a ScreenModel onto an ILI9341 panel.
 */
class LcdScreen : public Screen {
public:
    LcdScreen(Ili9341Display &display, const Theme &theme);

    void present(const ScreenModel &model) override;

private:
    void renderComponent(const ComponentView &view);

    Ili9341Display &display_;
    const Theme &theme_;
};

}
