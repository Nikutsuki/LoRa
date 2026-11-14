#pragma once

#include "../CardKbKeyboard.hpp"
#include "../ui/Input.hpp"

namespace pico_ui {

/*!
 * Input adapter that decodes raw CardKB scancodes into ui key events.
 *
 * Owns the escape sequence state machine previously embedded in the
 * application, so the app no longer needs to know how a keyboard encodes keys.
 */
class CardKbInput : public ui::Input {
public:
    explicit CardKbInput(CardKbKeyboard &keyboard);

    bool poll(ui::KeyEvent &event) override;

    CardKbKeyboard &keyboard();

private:
    ui::KeyEvent decode(uint8_t key);

    CardKbKeyboard &keyboard_;
    uint8_t escapeState_ = 0;
};

}
