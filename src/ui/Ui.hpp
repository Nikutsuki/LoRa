#pragma once

#include <cstddef>
#include <cstdint>

#include "Input.hpp"
#include "ScreenModel.hpp"

namespace ui {

/*!
 * Semantic colour role.
 *
 * Components never pick concrete pixel values, so a screen can map tones onto
 * its own palette instead of inheriting the LCD's colour space.
 */
enum class Tone : uint8_t {
    Default,
    Accent,
    Muted,
    Danger,
    Success,
    Focus,
};

struct Theme {
    uint16_t background = 0x0000;
    uint16_t surface = 0x2104;
    uint16_t surfaceAlt = 0x0010;
    uint16_t text = 0xFFFF;
    uint16_t muted = 0x8410;
    uint16_t accent = 0x07FF;
    uint16_t focus = 0xFFE0;
    uint16_t danger = 0xF800;
    uint16_t success = 0x07E0;

    uint16_t colorFor(Tone tone) const;
};

class Component {
public:
    Component(Rect bounds, bool focusable);
    virtual ~Component() = default;

    bool focusable() const;
    const Rect &bounds() const;

    virtual ComponentView describe(bool focused, const Theme &theme) const = 0;
    virtual bool activate();

protected:
    Rect bounds_;
    bool focusable_;
};

class Label : public Component {
public:
    Label(Rect bounds, const char *text, Tone tone, uint8_t scale = 1);

    void setText(const char *text);
    void setTone(Tone tone);
    void setStatusColor(uint16_t color) { this->statusColor_ = color; }
    uint8_t scale() const;
    ComponentView describe(bool focused, const Theme &theme) const override;

private:
    const char *text_;
    Tone tone_;
    uint8_t scale_;
    uint16_t statusColor_ = 0;
};

class Button : public Component {
public:
    Button(Rect bounds, const char *label);

    void setLabel(const char *label);
    const char *label() const;
    ComponentView describe(bool focused, const Theme &theme) const override;
    bool activate() override;

private:
    const char *label_;
};

class TextBox : public Component {
public:
    TextBox(Rect bounds, const char *text);

    void setText(const char *text);
    ComponentView describe(bool focused, const Theme &theme) const override;

private:
    const char *text_;
};

class ListItem : public Component {
public:
    ListItem(Rect bounds, const char *primary, const char *secondary, bool selected);

    void setContent(const char *primary, const char *secondary, bool selected);
    ComponentView describe(bool focused, const Theme &theme) const override;

private:
    const char *primary_;
    const char *secondary_;
    bool selected_;
};

/*!
 * Thin horizontal rule, used to separate the header from the body.
 */
class Divider : public Component {
public:
    explicit Divider(Rect bounds);

    ComponentView describe(bool focused, const Theme &theme) const override;
};

/*!
 * Full width bar with a title on the left and optional right aligned text.
 *
 * Mirrors the Go UI's top bar so both front ends present the same information.
 */
class HeaderBar : public Component {
public:
    HeaderBar(Rect bounds, const char *title, const char *right);

    void setTitle(const char *title);
    void setRight(const char *right);
    ComponentView describe(bool focused, const Theme &theme) const override;

private:
    const char *title_;
    const char *right_;
};

/*!
 * Single status line pinned to the bottom of the screen.
 */
class StatusBar : public Component {
public:
    StatusBar(Rect bounds, const char *text, Tone tone);

    void setText(const char *text);
    void setTone(Tone tone);
    ComponentView describe(bool focused, const Theme &theme) const override;

private:
    const char *text_;
    Tone tone_;
};

/*!
 * Row of tab labels with the active one highlighted.
 *
 * Rendered as a single component so the remote terminal can lay it out as a
 * strip rather than guessing at individual widget positions.
 */
class TabStrip : public Component {
public:
    TabStrip(Rect bounds, const char *const *labels, size_t count, size_t active);

    void setActive(size_t active);
    size_t active() const;
    ComponentView describe(bool focused, const Theme &theme) const override;

private:
    static constexpr size_t MaxLabelLength = 96;

    void rebuild();

    const char *const *labels_;
    size_t count_;
    size_t active_;
    /*! Pre-joined label text such as "[1: Messages] [2: Contacts]". */
    char text_[MaxLabelLength]{};
};

class Page {
public:
    static constexpr size_t MaxComponents = ScreenModel::MaxComponents;

    void setComponents(Component **components, size_t count);
    void describe(ScreenModel &model, const Theme &theme) const;
    bool handle(KeyEvent event);
    size_t focusedIndex() const;
    void setFocusedIndex(size_t index);

    /*!
     * Component that currently holds focus, or nullptr when the page is empty.
     *
     * Callers compare pointers rather than indices so that adding widgets to a
     * layout cannot silently rewire which button a key press activates.
     */
    Component *focusedComponent() const;

    /*!
     * Focuses \p component if it belongs to this page and is focusable.
     *
     * \return false when the component is not on the page.
     */
    bool focusComponent(const Component *component);

private:
    Component *components_[MaxComponents]{};
    size_t count_ = 0;
    size_t focusedIndex_ = 0;

    void moveFocus(int direction);
    void normalizeFocus();
};

}
