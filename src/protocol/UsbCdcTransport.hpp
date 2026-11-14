#pragma once

#include <cstddef>
#include <cstdint>

#include "ByteStream.hpp"

namespace proto {

/*!
 * Protocol transport over the USB CDC serial port.
 *
 * This is the stream the desktop UI connects to, so it is the default carrier.
 * It deliberately bypasses printf: frames are binary, and any interleaved text
 * would corrupt the CRC protected payloads.
 */
class UsbCdcTransport : public ByteStream {
public:
    void init() override;
    size_t read(uint8_t *buffer, size_t capacity) override;
    void write(const uint8_t *data, size_t length) override;

    /*!
     * True once the host has opened the port and configured the line.
     *
     * USB enumeration finishes long after boot, so this is queried on every
     * transfer rather than cached during init().
     */
    bool connected() const;
};

}
