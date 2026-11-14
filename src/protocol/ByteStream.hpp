#pragma once

#include <cstddef>
#include <cstdint>

namespace proto {

/*!
 * Bidirectional byte stream used by the protocol layer.
 *
 * The protocol must not care which peripheral carries it: the desktop app talks
 * to the board over USB CDC, while a bare UART is useful for bring-up and for
 * boards wired to an external transceiver.
 */
class ByteStream {
public:
    virtual ~ByteStream() = default;

    /*! Performs one-time hardware setup. */
    virtual void init() = 0;

    /*!
     * Reads whatever is already buffered, without blocking.
     *
     * \return number of bytes stored in \p buffer.
     */
    virtual size_t read(uint8_t *buffer, size_t capacity) = 0;

    /*!
     * Writes every byte, blocking only while the transmit buffer is full.
     */
    virtual void write(const uint8_t *data, size_t length) = 0;
};

}
