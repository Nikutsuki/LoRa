#include "UartScreen.hpp"

namespace pico_ui {

UartScreen::UartScreen(proto::LinkSession &session, uint16_t width, uint16_t height)
    : session_(session), width_(width), height_(height)
{
}

void UartScreen::sendHello()
{
    session_.sendHello(width_, height_);
}

proto::ComponentMessage UartScreen::toMessage(const ui::ComponentView &view)
{
    proto::ComponentMessage message;
    message.kind = static_cast<uint8_t>(view.kind);
    message.x = view.bounds.x;
    message.y = view.bounds.y;
    message.w = view.bounds.w;
    message.h = view.bounds.h;
    message.focused = view.focused;
    message.selected = view.selected;
    message.focusable = view.focusable;
    message.color = view.color;
    message.statusColor = view.statusColor;
    message.scale = view.scale;
    message.primary = view.primary;
    message.secondary = view.secondary;
    
    return message;
}

void UartScreen::present(const ui::ScreenModel &model)
{
    session_.beginScreen(width_, height_, model.fullRedraw());

    bool allAccepted = true;
    for (size_t i = 0; i < model.count(); i++) {
        if (!session_.addComponent(toMessage(model.at(i)))) {
            // Do not continue silently: a dropped widget removes a control from
            // the remote screen, which looks like the terminal is broken.
            allAccepted = false;
            break;
        }
    }

    lastFrameSent_ = allAccepted && session_.finishScreen();
}

bool UartScreen::lastFrameSent() const
{
    return lastFrameSent_;
}

}
