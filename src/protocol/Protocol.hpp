#pragma once

#include <cstddef>
#include <cstdint>

/*!
 * Frame format shared by the device firmware and the desktop UI.
 *
 *   offset  size  field
 *   0       1     sync 0xA5
 *   1       1     sync 0x5A
 *   2       1     message type
 *   3       2     payload length, little endian
 *   5       n     payload
 *   5+n     2     CRC-16/CCITT-FALSE over bytes [2, 5+n)
 *
 * Using two fixed sync bytes plus a length and a checksum means a host that
 * attaches mid-stream resynchronises on the next frame instead of feeding
 * garbage into the parser.
 */
namespace proto {

constexpr uint8_t SyncByte0 = 0xA5;
constexpr uint8_t SyncByte1 = 0x5A;
constexpr size_t HeaderSize = 5;
constexpr size_t TrailerSize = 2;

/*!
 * Maximum payload carried by one frame.
 *
 * A dense chat screen is now the worst case: a header, a tab strip, up to eight
 * history rows of 96 bytes each, the input box and a button bar, which measures
 * about 1.2 KB. This leaves headroom for longer nicknames without splitting a
 * screen across frames. The length field is 16 bit, so the wire format does not
 * change when this grows.
 */
constexpr size_t MaxPayload = 1536;
constexpr size_t MaxFrame = HeaderSize + MaxPayload + TrailerSize;

enum class MessageType : uint8_t {
    /*! Device -> host: firmware is alive and announces its layout. */
    Hello = 1,
    /*! Device -> host: complete screen description. */
    Screen = 2,
    /*! Device -> host: transient status line. */
    Notice = 3,
    /*! Host -> device: one key press. */
    Key = 4,
    /*! Host -> device: liveness probe. */
    Ping = 5,
    /*! Device -> host: reply to Ping. */
    Pong = 6,
    FileExchangeStart = 7,
    FileExchangeData = 8,
    FileExchangeAck = 9,
    FileExchangeEnd = 10,
    FileExchangeCancel = 11,
};

enum class KeyCode : uint8_t {
    None = 0,
    Up = 1,
    Down = 2,
    Left = 3,
    Right = 4,
    Enter = 5,
    Backspace = 6,
    Character = 7,
};

enum class NoticeLevel : uint8_t {
    Info = 0,
    Success = 1,
    Danger = 2,
};

/*!
 * CRC-16/CCITT-FALSE (polynomial 0x1021, initial value 0xFFFF).
 */
uint16_t crc16(const uint8_t *data, size_t length);

}
