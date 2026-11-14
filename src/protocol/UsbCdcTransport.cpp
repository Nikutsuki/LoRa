#include "UsbCdcTransport.hpp"

#include "pico/stdio.h"
#include "pico/stdio_usb.h"

namespace proto {

void UsbCdcTransport::init()
{
    // stdio_init_all() has already brought the USB stack up by the time the link
    // is initialised, so there is nothing to configure here. Connectivity is
    // deliberately not cached: the host usually attaches after boot.
}

bool UsbCdcTransport::connected() const
{
    return stdio_usb_connected();
}

size_t UsbCdcTransport::read(uint8_t *buffer, size_t capacity)
{
    size_t count = 0;
    while (count < capacity) {
        const int value = getchar_timeout_us(0);
        if (value == PICO_ERROR_TIMEOUT) {
            break;
        }
        buffer[count++] = static_cast<uint8_t>(value);
    }
    return count;
}

void UsbCdcTransport::write(const uint8_t *data, size_t length)
{
    if (!stdio_usb_connected()) {
        // Nothing is listening; dropping the frame is correct because the host
        // requests a full redraw when it attaches.
        return;
    }
    for (size_t i = 0; i < length; i++) {
        putchar_raw(static_cast<int>(data[i]));
    }
}

}
