#include "Ui.hpp"

#include <cstring>

namespace ui {

Component::Component(Rect bounds, bool focusable)
    : bounds_(bounds), focusable_(focusable)
{
}

bool Component::focusable() const
{
    return focusable_;
}

bool Component::focused() const
{
    return focused_;
}

void Component::setFocused(bool focused)
{
    focused_ = focused;
}

const Rect &Component::bounds() const
{
    return bounds_;
}

bool Component::activate()
{
    return false;
}

Label::Label(Rect bounds, const char *text, uint16_t color, uint8_t scale)
    : Component(bounds, false), text_(text), color_(color), scale_(scale)
{
}

void Label::setText(const char *text)
{
    text_ = text;
}

void Label::setColor(uint16_t color)
{
    color_ = color;
}

void Label::render(Ili9341Display &display, const Theme &theme)
{
    display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, theme.background);
    display.drawText(bounds_.x, bounds_.y, text_, color_, theme.background, scale_);
}

Button::Button(Rect bounds, const char *label)
    : Component(bounds, true), label_(label)
{
}

void Button::setLabel(const char *label)
{
    label_ = label;
}

const char *Button::label() const
{
    return label_;
}

void Button::render(Ili9341Display &display, const Theme &theme)
{
    uint16_t fill = focused_ ? theme.focus : theme.surface;
    uint16_t text = focused_ ? Ili9341Display::Black : theme.text;
    display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, fill);
    display.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, focused_ ? theme.text : theme.muted);
    display.drawText(static_cast<uint16_t>(bounds_.x + 5), static_cast<uint16_t>(bounds_.y + 6), label_, text, fill, 1);
}

bool Button::activate()
{
    return true;
}

TextBox::TextBox(Rect bounds, const char *text)
    : Component(bounds, true), text_(text)
{
}

void TextBox::setText(const char *text)
{
    text_ = text;
}

void TextBox::render(Ili9341Display &display, const Theme &theme)
{
    uint16_t fill = focused_ ? theme.surfaceAlt : theme.surface;
    display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, fill);
    display.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, focused_ ? theme.focus : theme.muted);
    display.drawText(static_cast<uint16_t>(bounds_.x + 5), static_cast<uint16_t>(bounds_.y + 7), text_, theme.text, fill, 1);
}

ListItem::ListItem(Rect bounds, const char *primary, const char *secondary, bool selected)
    : Component(bounds, true), primary_(primary), secondary_(secondary), selected_(selected)
{
}

void ListItem::setContent(const char *primary, const char *secondary, bool selected)
{
    primary_ = primary;
    secondary_ = secondary;
    selected_ = selected;
}

void ListItem::render(Ili9341Display &display, const Theme &theme)
{
    uint16_t fill = selected_ ? theme.surfaceAlt : theme.surface;
    if (focused_) {
        fill = theme.focus;
    }
    uint16_t text = focused_ ? Ili9341Display::Black : theme.text;
    display.fillRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, fill);
    display.drawRect(bounds_.x, bounds_.y, bounds_.w, bounds_.h, focused_ ? theme.text : theme.muted);
    display.drawText(static_cast<uint16_t>(bounds_.x + 5), static_cast<uint16_t>(bounds_.y + 4), primary_, text, fill, 1);
    display.drawText(static_cast<uint16_t>(bounds_.x + 5), static_cast<uint16_t>(bounds_.y + 15), secondary_, focused_ ? Ili9341Display::Black : theme.muted, fill, 1);
}

void Page::setComponents(Component **components, size_t count)
{
    count_ = count > MaxComponents ? MaxComponents : count;
    for (size_t i = 0; i < count_; i++) {
        components_[i] = components[i];
    }
    normalizeFocus();
}

void Page::render(Ili9341Display &display, const Theme &theme)
{
    normalizeFocus();
    for (size_t i = 0; i < count_; i++) {
        components_[i]->setFocused(i == focusedIndex_ && components_[i]->focusable());
        components_[i]->render(display, theme);
    }
}

bool Page::handle(KeyEvent event)
{
    switch (event.key) {
    case Key::Left:
    case Key::Up:
        moveFocus(-1);
        return false;
    case Key::Right:
    case Key::Down:
        moveFocus(1);
        return false;
    case Key::Enter:
        if (count_ > 0 && components_[focusedIndex_]->focusable()) {
            return components_[focusedIndex_]->activate();
        }
        return false;
    default:
        return false;
    }
}

size_t Page::focusedIndex() const
{
    return focusedIndex_;
}

void Page::setFocusedIndex(size_t index)
{
    focusedIndex_ = index;
    normalizeFocus();
}

void Page::moveFocus(int direction)
{
    if (count_ == 0) {
        focusedIndex_ = 0;
        return;
    }

    size_t next = focusedIndex_;
    for (size_t attempts = 0; attempts < count_; attempts++) {
        if (direction < 0) {
            next = next == 0 ? count_ - 1 : next - 1;
        } else {
            next = (next + 1) % count_;
        }
        if (components_[next]->focusable()) {
            focusedIndex_ = next;
            return;
        }
    }
}

void Page::normalizeFocus()
{
    if (count_ == 0) {
        focusedIndex_ = 0;
        return;
    }
    if (focusedIndex_ >= count_) {
        focusedIndex_ = 0;
    }
    if (!components_[focusedIndex_]->focusable()) {
        moveFocus(1);
    }
}

}
