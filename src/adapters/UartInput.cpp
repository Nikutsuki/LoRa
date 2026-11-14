#include "UartInput.hpp"

namespace pico_ui {

UartInput::UartInput(proto::LinkSession &session)
    : session_(session)
{
}

bool UartInput::poll(ui::KeyEvent &event)
{
    proto::KeyMessage message;
    if (!session_.nextKey(message)) {
        return false;
    }

    event.key = toKey(message.code);
    event.character = message.code == proto::KeyCode::Character ? message.character : '\0';
    return event.key != ui::Key::None;
}

ui::Key UartInput::toKey(proto::KeyCode code)
{
    switch (code) {
    case proto::KeyCode::Up:
        return ui::Key::Up;
    case proto::KeyCode::Down:
        return ui::Key::Down;
    case proto::KeyCode::Left:
        return ui::Key::Left;
    case proto::KeyCode::Right:
        return ui::Key::Right;
    case proto::KeyCode::Enter:
        return ui::Key::Enter;
    case proto::KeyCode::Backspace:
        return ui::Key::Backspace;
    case proto::KeyCode::Character:
        return ui::Key::Character;
    case proto::KeyCode::None:
        break;
    }
    return ui::Key::None;
}

}
