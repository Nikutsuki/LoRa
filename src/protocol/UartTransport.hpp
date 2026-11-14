#pragma once

#include <cstddef>
#include <cstdint>

#include "ByteStream.hpp"
#include "hardware/uart.h"
#include "pico/types.h"

namespace proto {

struct UartConfig {
    uart_inst_t *uart;
    uint txPin;
    uint rxPin;
    uint baud;
};

/*!
 * Protocol transport over a hardware UART.
 *
 * Useful for bring-up and for boards whose link is wired to external pins
 * rather than the USB port.
 */
class UartTransport : public ByteStream {
public:
    explicit UartTransport(UartConfig config);

    void init() override;
    size_t read(uint8_t *buffer, size_t capacity) override;
    void write(const uint8_t *data, size_t length) override;

private:
    UartConfig config_;
};

}
