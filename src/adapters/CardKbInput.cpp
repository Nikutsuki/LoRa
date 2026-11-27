#include "CardKbInput.hpp"

namespace pico_ui {

CardKbInput::CardKbInput(CardKbKeyboard &keyboard)
    : keyboard_(keyboard)
{
}

CardKbKeyboard &CardKbInput::keyboard()
{
    return keyboard_;
}

bool CardKbInput::poll(ui::KeyEvent &event)
{
    uint8_t key = 0;
    if (!keyboard_.readKey(key)) {
        return false;
    }

    event = decode(key);
    return event.key != ui::Key::None;
}

ui::KeyEvent CardKbInput::decode(uint8_t key)
{
    ui::KeyEvent event{};

    if (escapeState_ == 1) {
        escapeState_ = key == '[' ? 2 : 0;
        return event;
    }

    if (escapeState_ == 2) {
        escapeState_ = 0;
        switch (key) {
        case 'A':
            event.key = ui::Key::Up;
            break;
        case 'B':
            event.key = ui::Key::Down;
            break;
        case 'C':
            event.key = ui::Key::Right;
            break;
        case 'D':
            event.key = ui::Key::Left;
            break;
        default:
            break;
        }
        return event;
    }

    if (key == 0x1b) {
        escapeState_ = 1;
        return event;
    }

    switch (key) {
    case 0x1c:
    case 0x80:
    case 0xb4:
        event.key = ui::Key::Left;
        return event;
    case 0x1d:
    case 0x81:
    case 0xb7:
        event.key = ui::Key::Right;
        return event;
    case 0x1e:
    case 0x82:
    case 0xb5:
        event.key = ui::Key::Up;
        return event;
    case 0x1f:
    case 0x83:
    case 0xb6:
        event.key = ui::Key::Down;
        return event;
    case '\r':
    case '\n':
        event.key = ui::Key::Enter;
        return event;
    case 8:
    case 127:
        event.key = ui::Key::Backspace;
        return event;
    default:
        if (key >= 32 && key <= 126) {
            event.key = ui::Key::Character;
            event.character = static_cast<char>(key);
        }
        return event;
    }
}

}
