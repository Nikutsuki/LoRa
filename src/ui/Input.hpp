#pragma once

#include <cstdint>

namespace ui {

enum class Key {
    None,
    Left,
    Right,
    Up,
    Down,
    Enter,
    Backspace,
    Character,
};

struct KeyEvent {
    Key key = Key::None;
    char character = '\0';
};

/*!
 * Abstract key source.
 *
 * Implementations translate their transport specific encoding into KeyEvent
 * and must never block for longer than one application poll cycle.
 */
class Input {
public:
    virtual ~Input() = default;

    /*!
     * Retrieves a pending key event.
     *
     * \return true when \p event was filled with a real key press.
     */
    virtual bool poll(KeyEvent &event) = 0;
};

}
