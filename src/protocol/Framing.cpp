#include "Framing.hpp"

namespace proto {

namespace {

uint16_t crcStep(uint16_t crc, uint8_t value)
{
    crc ^= static_cast<uint16_t>(value) << 8;
    for (int bit = 0; bit < 8; bit++) {
        crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                             : static_cast<uint16_t>(crc << 1);
    }
    return crc;
}

}

uint16_t crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; i++) {
        crc = crcStep(crc, data[i]);
    }
    return crc;
}

FrameWriter::FrameWriter(uint8_t *buffer, size_t capacity)
    : buffer_(buffer), capacity_(capacity)
{
}

size_t FrameWriter::frameLength() const
{
    return frameLength_;
}

uint8_t *FrameWriter::frameData()
{
    return buffer_;
}

bool FrameWriter::begin(MessageType type, size_t payloadLength)
{
    frameLength_ = 0;
    payloadLength_ = 0;

    if (payloadLength > MaxPayload) {
        return false;
    }
    if (HeaderSize + payloadLength + TrailerSize > capacity_) {
        return false;
    }

    buffer_[0] = SyncByte0;
    buffer_[1] = SyncByte1;
    buffer_[2] = static_cast<uint8_t>(type);
    buffer_[3] = static_cast<uint8_t>(payloadLength & 0xFF);
    buffer_[4] = static_cast<uint8_t>((payloadLength >> 8) & 0xFF);
    payloadLength_ = payloadLength;
    return true;
}

uint8_t *FrameWriter::payload()
{
    return buffer_ + HeaderSize;
}

bool FrameWriter::commit()
{
    const size_t crcOffset = HeaderSize + payloadLength_;
    // CRC covers the type byte, the two length bytes and the payload.
    const uint16_t crc = crc16(buffer_ + 2, 3 + payloadLength_);
    buffer_[crcOffset] = static_cast<uint8_t>(crc & 0xFF);
    buffer_[crcOffset + 1] = static_cast<uint8_t>((crc >> 8) & 0xFF);
    frameLength_ = crcOffset + TrailerSize;
    return true;
}

FrameReader::FrameReader(uint8_t *scratch, size_t capacity)
    : scratch_(scratch), capacity_(capacity)
{
}

void FrameReader::reset()
{
    state_ = State::Sync0;
    type_ = MessageType::Ping;
    payloadLength_ = 0;
    payloadIndex_ = 0;
    runningCrc_ = 0xFFFF;
    expectedCrc_ = 0;
}

bool FrameReader::feed(uint8_t byte, Handler handler, void *context)
{
    switch (state_) {
    case State::Sync0:
        if (byte == SyncByte0) {
            state_ = State::Sync1;
        }
        return false;

    case State::Sync1:
        state_ = byte == SyncByte1 ? State::Type : State::Sync0;
        return false;

    case State::Type:
        type_ = static_cast<MessageType>(byte);
        runningCrc_ = crcStep(0xFFFF, byte);
        state_ = State::LengthLow;
        return false;

    case State::LengthLow:
        runningCrc_ = crcStep(runningCrc_, byte);
        payloadLength_ = byte;
        state_ = State::LengthHigh;
        return false;

    case State::LengthHigh:
        runningCrc_ = crcStep(runningCrc_, byte);
        payloadLength_ |= static_cast<size_t>(byte) << 8;
        if (payloadLength_ > MaxPayload || payloadLength_ > capacity_) {
            reset();
            return false;
        }
        payloadIndex_ = 0;
        state_ = payloadLength_ == 0 ? State::CrcLow : State::Payload;
        return false;

    case State::Payload:
        runningCrc_ = crcStep(runningCrc_, byte);
        scratch_[payloadIndex_++] = byte;
        if (payloadIndex_ >= payloadLength_) {
            state_ = State::CrcLow;
        }
        return false;

    case State::CrcLow:
        expectedCrc_ = byte;
        state_ = State::CrcHigh;
        return false;

    case State::CrcHigh:
        expectedCrc_ |= static_cast<uint16_t>(byte) << 8;
        if (expectedCrc_ != runningCrc_) {
            reset();
            return false;
        }
        if (handler != nullptr) {
            handler(type_, scratch_, payloadLength_, context);
        }
        reset();
        return true;
    }

    return false;
}

PayloadBuilder::PayloadBuilder(uint8_t *data, size_t capacity)
    : data_(data), capacity_(capacity)
{
}

bool PayloadBuilder::u8(uint8_t value)
{
    if (size_ + 1 > capacity_) {
        return false;
    }
    data_[size_++] = value;
    return true;
}

bool PayloadBuilder::u16(uint16_t value)
{
    if (size_ + 2 > capacity_) {
        return false;
    }
    data_[size_++] = static_cast<uint8_t>(value & 0xFF);
    data_[size_++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    return true;
}

bool PayloadBuilder::u32(uint32_t value)
{
    if (size_ + 4 > capacity_) {
        return false;
    }
    data_[size_++] = static_cast<uint8_t>(value & 0xFF);
    data_[size_++] = static_cast<uint8_t>((value >> 8) & 0xFF);
    data_[size_++] = static_cast<uint8_t>((value >> 16) & 0xFF);
    data_[size_++] = static_cast<uint8_t>((value >> 24) & 0xFF);
    return true;
}

bool PayloadBuilder::str(const char *value)
{
    size_t length = 0;
    if (value != nullptr) {
        while (value[length] != '\0') {
            length++;
        }
    }
    if (length > 255 || size_ + 1 + length > capacity_) {
        return false;
    }
    data_[size_++] = static_cast<uint8_t>(length);
    for (size_t i = 0; i < length; i++) {
        data_[size_++] = static_cast<uint8_t>(value[i]);
    }
    return true;
}

size_t PayloadBuilder::size() const
{
    return size_;
}

PayloadReader::PayloadReader(const uint8_t *data, size_t length)
    : data_(data), length_(length)
{
}

bool PayloadReader::u8(uint8_t &value)
{
    if (offset_ + 1 > length_) {
        return false;
    }
    value = data_[offset_++];
    return true;
}

bool PayloadReader::u16(uint16_t &value)
{
    if (offset_ + 2 > length_) {
        return false;
    }
    const uint16_t low = data_[offset_];
    const uint16_t high = data_[offset_ + 1];
    value = static_cast<uint16_t>(low | (high << 8));
    offset_ += 2;
    return true;
}

bool PayloadReader::u32(uint32_t &value)
{
    if (offset_ + 4 > length_) {
        return false;
    }
    const uint32_t b0 = data_[offset_];
    const uint32_t b1 = data_[offset_ + 1];
    const uint32_t b2 = data_[offset_ + 2];
    const uint32_t b3 = data_[offset_ + 3];
    value = static_cast<uint32_t>(b0 | (b1 << 8) | (b2 << 16) | (b3 << 24));
    offset_ += 4;
    return true;
}

bool PayloadReader::str(const char *&value)
{
    if (offset_ + 1 > length_) {
        return false;
    }
    const size_t stringLength = data_[offset_++];
    if (offset_ + stringLength > length_) {
        return false;
    }
    value = reinterpret_cast<const char *>(data_ + offset_);
    offset_ += stringLength;
    return true;
}

bool PayloadReader::empty() const
{
    return offset_ >= length_;
}

size_t PayloadReader::offset() const
{
    return offset_;
}

}
