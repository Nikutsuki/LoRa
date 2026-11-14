#include "UartTransport.hpp"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

namespace proto {

UartTransport::UartTransport(UartConfig config)
    : config_(config)
{
}

void UartTransport::init()
{
    uart_init(config_.uart, config_.baud);
    gpio_set_function(config_.txPin, GPIO_FUNC_UART);
    gpio_set_function(config_.rxPin, GPIO_FUNC_UART);
    uart_set_format(config_.uart, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(config_.uart, true);
}

size_t UartTransport::read(uint8_t *buffer, size_t capacity)
{
    size_t count = 0;
    while (count < capacity && uart_is_readable(config_.uart)) {
        buffer[count++] = static_cast<uint8_t>(uart_getc(config_.uart));
    }
    return count;
}

void UartTransport::write(const uint8_t *data, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        uart_putc_raw(config_.uart, static_cast<char>(data[i]));
    }
}

}
