#include "Ui.hpp"

#include <cstdio>
#include <cstring>

namespace ui {

uint16_t Theme::colorFor(Tone tone) const
{
    switch (tone) {
    case Tone::Accent:
        return accent;
    case Tone::Muted:
        return muted;
    case Tone::Danger:
        return danger;
    case Tone::Success:
        return success;
    case Tone::Focus:
        return focus;
    case Tone::Default:
        break;
    }
    return text;
}

Component::Component(Rect bounds, bool focusable)
    : bounds_(bounds), focusable_(focusable)
{
}

bool Component::focusable() const
{
    return focusable_;
}

const Rect &Component::bounds() const
{
    return bounds_;
}

bool Component::activate()
{
    return false;
}

Label::Label(Rect bounds, const char *text, Tone tone, uint8_t scale)
    : Component(bounds, false), text_(text), tone_(tone), scale_(scale)
{
}

void Label::setText(const char *text)
{
    text_ = text;
}

void Label::setTone(Tone tone)
{
    tone_ = tone;
}

uint8_t Label::scale() const
{
    return scale_;
}

ComponentView Label::describe(bool focused, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::Label;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), text_);
    view.color = theme.colorFor(tone_);
    view.statusColor = statusColor_;
    view.scale = scale_;
    view.focusable = false;
    (void)focused;
    return view;
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

ComponentView Button::describe(bool focused, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::Button;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), label_);
    view.color = theme.colorFor(Tone::Default);
    view.focused = focused;
    view.focusable = true;
    return view;
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

ComponentView TextBox::describe(bool focused, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::TextBox;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), text_);
    view.color = theme.colorFor(Tone::Default);
    view.focused = focused;
    view.focusable = true;
    return view;
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

ComponentView ListItem::describe(bool focused, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::ListItem;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), primary_);
    copyText(view.secondary, sizeof(view.secondary), secondary_);
    view.color = theme.colorFor(Tone::Default);
    view.focused = focused;
    view.selected = selected_;
    view.focusable = true;
    return view;
}

Divider::Divider(Rect bounds)
    : Component(bounds, false)
{
}

ComponentView Divider::describe(bool, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::Divider;
    view.bounds = bounds_;
    view.color = theme.colorFor(Tone::Muted);
    view.focusable = false;
    return view;
}

HeaderBar::HeaderBar(Rect bounds, const char *title, const char *right)
    : Component(bounds, false), title_(title), right_(right)
{
}

void HeaderBar::setTitle(const char *title)
{
    title_ = title;
}

void HeaderBar::setRight(const char *right)
{
    right_ = right;
}

ComponentView HeaderBar::describe(bool, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::Label;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), title_);
    copyText(view.secondary, sizeof(view.secondary), right_);
    view.color = theme.colorFor(Tone::Accent);
    view.focusable = false;
    return view;
}

StatusBar::StatusBar(Rect bounds, const char *text, Tone tone)
    : Component(bounds, false), text_(text), tone_(tone)
{
}

void StatusBar::setText(const char *text)
{
    text_ = text;
}

void StatusBar::setTone(Tone tone)
{
    tone_ = tone;
}

ComponentView StatusBar::describe(bool, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::StatusBar;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), text_);
    view.color = theme.colorFor(tone_);
    view.focusable = false;
    return view;
}

TabStrip::TabStrip(Rect bounds, const char *const *labels, size_t count, size_t active)
    : Component(bounds, false), labels_(labels), count_(count), active_(active)
{
    rebuild();
}

void TabStrip::setActive(size_t active)
{
    if (active == active_) {
        return;
    }
    active_ = active;
    rebuild();
}

size_t TabStrip::active() const
{
    return active_;
}

void TabStrip::rebuild()
{
    text_[0] = '\0';
    size_t used = 0;

    // Compose inside the available width. Bracketing the active tab made the
    // four labels 336 px wide in a 304 px box, which wrapped the tail onto the
    // next line, so the active tab is marked by a leading marker instead and the
    // spacing is single-character.
    for (size_t i = 0; i < count_; i++) {
        const char *label = labels_[i] != nullptr ? labels_[i] : "";
        const char marker = i == active_ ? '*' : ' ';

        const int written = std::snprintf(text_ + used, sizeof(text_) - used,
                                          i + 1 < count_ ? "%c%s " : "%c%s",
                                          marker, label);
        if (written <= 0) {
            break;
        }
        used += static_cast<size_t>(written);
        if (used >= sizeof(text_)) {
            break;
        }
    }
}

ComponentView TabStrip::describe(bool, const Theme &theme) const
{
    ComponentView view{};
    view.kind = ComponentKind::Label;
    view.bounds = bounds_;
    copyText(view.primary, sizeof(view.primary), text_);
    view.color = theme.colorFor(Tone::Muted);
    view.focusable = false;
    return view;
}

void Page::setComponents(Component **components, size_t count)
{
    count_ = count > MaxComponents ? MaxComponents : count;
    for (size_t i = 0; i < count_; i++) {
        components_[i] = components[i];
    }
    normalizeFocus();
}

void Page::describe(ScreenModel &model, const Theme &theme) const
{
    for (size_t i = 0; i < count_; i++) {
        const bool isFocused = i == focusedIndex_ && components_[i]->focusable();
        model.add(components_[i]->describe(isFocused, theme));
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

Component *Page::focusedComponent() const
{
    if (count_ == 0) {
        return nullptr;
    }
    return components_[focusedIndex_];
}

bool Page::focusComponent(const Component *component)
{
    if (component == nullptr) {
        return false;
    }
    for (size_t i = 0; i < count_; i++) {
        if (components_[i] == component) {
            focusedIndex_ = i;
            normalizeFocus();
            return true;
        }
    }
    return false;
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

