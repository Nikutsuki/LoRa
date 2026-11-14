#pragma once

#include <cstddef>
#include <cstdint>

#include "Protocol.hpp"

namespace proto {

/*!
 * Writes outbound frames into a caller provided buffer.
 *
 * The writer never allocates and never performs I/O, so it works identically on
 * the RP2040 and in a host side test harness.
 */
class FrameWriter {
public:
    FrameWriter(uint8_t *buffer, size_t capacity);

    /*! Number of bytes written by the most recent start/succeed pair. */
    size_t frameLength() const;
    uint8_t *frameData();

    bool begin(MessageType type, size_t payloadLength);
    uint8_t *payload();
    bool commit();

private:
    uint8_t *buffer_;
    size_t capacity_;
    size_t frameLength_ = 0;
    size_t payloadLength_ = 0;
};

/*!
 * Incremental frame reader.
 *
 * Bytes are fed one at a time so a partially received frame survives across
 * poll cycles. A frame that fails validation is dropped and the reader hunts
 * for the next sync pair.
 */
class FrameReader {
public:
    using Handler = void (*)(MessageType type, const uint8_t *payload, size_t length, void *context);

    FrameReader(uint8_t *scratch, size_t capacity);

    void reset();

    /*!
     * Consumes one byte.
     *
     * \param handler Invoked synchronously when a valid frame completes.
     * \return true when a frame was dispatched during this call.
     */
    bool feed(uint8_t byte, Handler handler, void *context);

private:
    enum class State : uint8_t {
        Sync0,
        Sync1,
        Type,
        LengthLow,
        LengthHigh,
        Payload,
        CrcLow,
        CrcHigh,
    };

    bool pushPayload(uint8_t byte);
    bool validate();

    uint8_t *scratch_;
    size_t capacity_;
    State state_ = State::Sync0;
    MessageType type_ = MessageType::Ping;
    size_t payloadLength_ = 0;
    size_t payloadIndex_ = 0;
    uint16_t runningCrc_ = 0xFFFF;
    uint16_t expectedCrc_ = 0;
};

/*!
 * Helpers for appending little endian scalars and length prefixed strings.
 */
class PayloadBuilder {
public:
    PayloadBuilder(uint8_t *data, size_t capacity);

    bool u8(uint8_t value);
    bool u16(uint16_t value);
    bool str(const char *value);

    size_t size() const;

private:
    uint8_t *data_;
    size_t capacity_;
    size_t size_ = 0;
};

/*!
 * Bounds checked reader for values written by PayloadBuilder.
 */
class PayloadReader {
public:
    PayloadReader(const uint8_t *data, size_t length);

    bool u8(uint8_t &value);
    bool u16(uint16_t &value);
    bool str(const char *&value);

    bool empty() const;
    size_t offset() const;

private:
    const uint8_t *data_;
    size_t length_;
    size_t offset_ = 0;
};

}
