#include "Messages.hpp"

namespace proto {

namespace {

uint8_t flagsFor(const ComponentMessage &component)
{
    uint8_t flags = 0;
    if (component.focused) {
        flags |= FlagFocused;
    }
    if (component.selected) {
        flags |= FlagSelected;
    }
    if (component.focusable) {
        flags |= FlagFocusable;
    }
    return flags;
}

}

bool writeComponent(PayloadBuilder &builder, const ComponentMessage &component)
{
    const uint8_t flags = flagsFor(component);
    return builder.u8(component.kind) &&
           builder.u16(component.x) &&
           builder.u16(component.y) &&
           builder.u16(component.w) &&
           builder.u16(component.h) &&
           builder.u8(flags) &&
           builder.u16(component.color) &&
           builder.u16(component.statusColor) &&
           builder.u8(component.scale) &&
           builder.str(component.primary) &&
           builder.str(component.secondary);
}

bool readComponent(PayloadReader &reader, ComponentMessage &component)
{
    uint8_t flags = 0;
    if (!reader.u8(component.kind) ||
        !reader.u16(component.x) ||
        !reader.u16(component.y) ||
        !reader.u16(component.w) ||
        !reader.u16(component.h) ||
        !reader.u8(flags) ||
        !reader.u16(component.color) ||
        !reader.u16(component.statusColor) ||
        !reader.u8(component.scale) ||
        !reader.str(component.primary) ||
        !reader.str(component.secondary)) {
        return false;
    }

    component.focused = (flags & FlagFocused) != 0;
    component.selected = (flags & FlagSelected) != 0;
    component.focusable = (flags & FlagFocusable) != 0;
    return true;
}

bool writeScreenHeader(PayloadBuilder &builder, const ScreenMessageHeader &header)
{
    return builder.u16(header.width) &&
           builder.u16(header.height) &&
           builder.u8(header.fullRedraw ? 1 : 0) &&
           builder.u8(header.count);
}

bool readScreenHeader(PayloadReader &reader, ScreenMessageHeader &header)
{
    uint8_t fullRedraw = 0;
    if (!reader.u16(header.width) ||
        !reader.u16(header.height) ||
        !reader.u8(fullRedraw) ||
        !reader.u8(header.count)) {
        return false;
    }
    header.fullRedraw = fullRedraw != 0;
    return true;
}

bool writeNotice(PayloadBuilder &builder, const NoticeMessage &notice)
{
    return builder.u8(static_cast<uint8_t>(notice.level)) && builder.str(notice.text);
}

bool readNotice(PayloadReader &reader, NoticeMessage &notice)
{
    uint8_t level = 0;
    if (!reader.u8(level) || !reader.str(notice.text)) {
        return false;
    }
    notice.level = static_cast<NoticeLevel>(level);
    return true;
}

bool writeHello(PayloadBuilder &builder, const HelloMessage &hello)
{
    return builder.u16(hello.width) &&
           builder.u16(hello.height) &&
           builder.u8(hello.protocolVersion);
}

bool readHello(PayloadReader &reader, HelloMessage &hello)
{
    return reader.u16(hello.width) &&
           reader.u16(hello.height) &&
           reader.u8(hello.protocolVersion);
}

bool writeKey(PayloadBuilder &builder, const KeyMessage &key)
{
    return builder.u8(static_cast<uint8_t>(key.code)) &&
           builder.u8(static_cast<uint8_t>(key.character));
}

bool readKey(PayloadReader &reader, KeyMessage &key)
{
    uint8_t code = 0;
    uint8_t character = 0;
    if (!reader.u8(code) || !reader.u8(character)) {
        return false;
    }
    key.code = static_cast<KeyCode>(code);
    key.character = static_cast<char>(character);
    return true;
}

}
