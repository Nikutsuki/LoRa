#pragma once

#include <cstddef>
#include <cstdint>

namespace ui {

/*!
 * Copies \p source into \p destination, always null terminating.
 *
 * Component descriptions come from application buffers that may be shorter than
 * the fixed view text, so every copy is bounded.
 */
void copyText(char *destination, size_t destinationSize, const char *source);

struct Rect {
    uint16_t x;
    uint16_t y;
    uint16_t w;
    uint16_t h;
};

enum class ComponentKind : uint8_t {
    Label,
    Button,
    TextBox,
    ListItem,
    /*! Horizontal divider, mirrors the Go UI's row separators. */
    Divider,
    /*! Fixed-width status line rendered as a bar at the bottom of the screen. */
    StatusBar,
};

/*!
 * Screen independent description of a single widget.
 *
 * Text is copied into the view rather than borrowed. Components read from the
 * application's character buffers, which are edited in place, so borrowing would
 * make two views alias the same memory and any comparison between them would
 * always report "unchanged".
 */
struct ComponentView {
    static constexpr size_t TextSize = 96;

    ComponentKind kind = ComponentKind::Label;
    Rect bounds{};
    char primary[TextSize]{};
    char secondary[TextSize]{};
    uint16_t color = 0;
    uint16_t statusColor = 0;
    uint8_t scale = 1;
    bool focused = false;
    bool selected = false;
    bool focusable = false;
};

/*!
 * Complete description of one screen frame.
 *
 * Screens consume this model instead of talking to widgets directly, which lets
 * the same application logic drive a local display or a remote UART terminal.
 */
class ScreenModel {
public:
    /*!
     * Upper bound on widgets per frame.
     *
     * The chat page is the densest: header, tab strip, up to twelve history rows,
     * the input box, and a button bar. Each row is its own component so the
     * remote terminal can lay them out independently.
     */
    static constexpr size_t MaxComponents = 24;

    void clear();
    bool add(const ComponentView &view);

    size_t count() const;
    const ComponentView &at(size_t index) const;

    void setFullRedraw(bool fullRedraw);
    bool fullRedraw() const;

    /*!
     * True when \p other describes the same frame.
     *
     * Text is compared by content, not pointer, because components borrow the
     * application's character buffers: those addresses stay the same while the
     * text inside them changes. Used to skip repainting an unchanged screen,
     * which is what makes the display flicker.
     */
    bool sameContent(const ScreenModel &other) const;

private:
    ComponentView components_[MaxComponents]{};
    size_t count_ = 0;
    bool fullRedraw_ = true;
};

}
