#pragma once

#include <cstddef>
#include <cstdint>

#include "Ili9341Display.hpp"

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

struct Rect {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

struct Theme {
    uint16_t background = Ili9341Display::Black;
    uint16_t surface = Ili9341Display::DarkGray;
    uint16_t surfaceAlt = Ili9341Display::Navy;
    uint16_t text = Ili9341Display::White;
    uint16_t muted = Ili9341Display::Gray;
    uint16_t accent = Ili9341Display::Cyan;
    uint16_t focus = Ili9341Display::Yellow;
    uint16_t danger = Ili9341Display::Red;
    uint16_t success = Ili9341Display::Green;
};

class Component {
public:
    Component(Rect bounds, bool focusable);
    virtual ~Component() = default;

    bool focusable() const;
    bool focused() const;
    void setFocused(bool focused);
    const Rect &bounds() const;

    virtual void render(Ili9341Display &display, const Theme &theme) = 0;
    virtual bool activate();

protected:
    Rect bounds_;
    bool focusable_;
    bool focused_ = false;
};

class Label : public Component {
public:
    Label(Rect bounds, const char *text, uint16_t color, uint8_t scale = 1);

    void setText(const char *text);
    void setColor(uint16_t color);
    void render(Ili9341Display &display, const Theme &theme) override;

private:
    const char *text_;
    uint16_t color_;
    uint8_t scale_;
};

class Button : public Component {
public:
    Button(Rect bounds, const char *label);

    void setLabel(const char *label);
    const char *label() const;
    void render(Ili9341Display &display, const Theme &theme) override;
    bool activate() override;

private:
    const char *label_;
};

class TextBox : public Component {
public:
    TextBox(Rect bounds, const char *text);

    void setText(const char *text);
    void render(Ili9341Display &display, const Theme &theme) override;

private:
    const char *text_;
};

class ListItem : public Component {
public:
    ListItem(Rect bounds, const char *primary, const char *secondary, bool selected);

    void setContent(const char *primary, const char *secondary, bool selected);
    void render(Ili9341Display &display, const Theme &theme) override;

private:
    const char *primary_;
    const char *secondary_;
    bool selected_;
};

class Page {
public:
    static constexpr size_t MaxComponents = 12;

    void setComponents(Component **components, size_t count);
    void render(Ili9341Display &display, const Theme &theme);
    bool handle(KeyEvent event);
    size_t focusedIndex() const;
    void setFocusedIndex(size_t index);

private:
    Component *components_[MaxComponents]{};
    size_t count_ = 0;
    size_t focusedIndex_ = 0;

    void moveFocus(int direction);
    void normalizeFocus();
};

}
