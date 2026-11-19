#pragma once

#include <cstddef>
#include <cstdint>

#include "Framing.hpp"
#include "Protocol.hpp"

namespace proto {

/*!
 * Wire representation of one widget inside a Screen frame.
 *
 * Layout:
 *   u8  kind
 *   u16 x, u16 y, u16 w, u16 h
 *   u8  flags   (bit0 focused, bit1 selected, bit2 focusable)
 *   u16 color
 *   u8  scale
 *   str primary
 *   str secondary
 */
struct ComponentMessage {
    uint8_t kind = 0;
    uint16_t x = 0;
    uint16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
    bool focused = false;
    bool selected = false;
    bool focusable = false;
    uint16_t color = 0;
    uint16_t statusColor = 0;
    uint8_t scale = 1;
    const char *primary = "";
    const char *secondary = "";
};

constexpr uint8_t FlagFocused = 0x01;
constexpr uint8_t FlagSelected = 0x02;
constexpr uint8_t FlagFocusable = 0x04;

bool writeComponent(PayloadBuilder &builder, const ComponentMessage &component);
bool readComponent(PayloadReader &reader, ComponentMessage &component);

/*!
 * Screen payload: u16 width, u16 height, u8 fullRedraw, u8 count, then
 * \p count component records.
 */
struct ScreenMessageHeader {
    uint16_t width = 0;
    uint16_t height = 0;
    bool fullRedraw = false;
    uint8_t count = 0;
};

bool writeScreenHeader(PayloadBuilder &builder, const ScreenMessageHeader &header);
bool readScreenHeader(PayloadReader &reader, ScreenMessageHeader &header);

/*!
 * Notice payload: u8 level, str text.
 */
struct NoticeMessage {
    NoticeLevel level = NoticeLevel::Info;
    const char *text = "";
};

bool writeNotice(PayloadBuilder &builder, const NoticeMessage &notice);
bool readNotice(PayloadReader &reader, NoticeMessage &notice);

/*!
 * Hello payload: u16 width, u16 height, u8 protocolVersion.
 */
struct HelloMessage {
    uint16_t width = 0;
    uint16_t height = 0;
    uint8_t protocolVersion = 0;
};

bool writeHello(PayloadBuilder &builder, const HelloMessage &hello);
bool readHello(PayloadReader &reader, HelloMessage &hello);

/*!
 * Key payload: u8 keyCode, u8 character.
 */
struct KeyMessage {
    KeyCode code = KeyCode::None;
    char character = '\0';
};

bool writeKey(PayloadBuilder &builder, const KeyMessage &key);
bool readKey(PayloadReader &reader, KeyMessage &key);

}
