#include "Ili9341Display.hpp"

#include "font8x8_basic.h"
#include "hardware/gpio.h"
#include "pico/stdlib.h"

Ili9341Display::Ili9341Display(Config config)
    : config_(config)
{
}

void Ili9341Display::init()
{
    spi_init(config_.spi, config_.baud);
    spi_set_format(config_.spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(config_.sckPin, GPIO_FUNC_SPI);
    gpio_set_function(config_.mosiPin, GPIO_FUNC_SPI);

    gpio_init(config_.csPin);
    gpio_init(config_.dcPin);
    gpio_init(config_.resetPin);
    gpio_set_dir(config_.csPin, GPIO_OUT);
    gpio_set_dir(config_.dcPin, GPIO_OUT);
    gpio_set_dir(config_.resetPin, GPIO_OUT);
    gpio_put(config_.csPin, 1);
    gpio_put(config_.dcPin, 1);
    gpio_put(config_.resetPin, 1);

    gpio_put(config_.resetPin, 0);
    sleep_ms(20);
    gpio_put(config_.resetPin, 1);
    sleep_ms(150);

    writeCommand(SoftwareReset);
    sleep_ms(150);
    writeCommand(SleepOut);
    sleep_ms(120);

    writeCommand(PixelFormatSet);
    writeData(0x55);

    writeCommand(MemoryAccessControl);
    writeData(config_.madctlValue);

    writeCommand(DisplayOn);
    sleep_ms(120);

    fillRect(0, 0, config_.width, config_.height, Black);
}

void Ili9341Display::fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (x >= config_.width || y >= config_.height) {
        return;
    }
    if (x + width > config_.width) {
        width = config_.width - x;
    }
    if (y + height > config_.height) {
        height = config_.height - y;
    }

    setWindow(x, y, x + width - 1, y + height - 1);
    chipSelect(false);
    dataCommand(true);
    writeColorRun(color, static_cast<uint32_t>(width) * height);
    chipSelect(true);
}

void Ili9341Display::drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color)
{
    if (width == 0 || height == 0) {
        return;
    }

    fillRect(x, y, width, 1, color);
    fillRect(x, static_cast<uint16_t>(y + height - 1), width, 1, color);
    fillRect(x, y, 1, height, color);
    fillRect(static_cast<uint16_t>(x + width - 1), y, 1, height, color);
}

void Ili9341Display::drawText(uint16_t x, uint16_t y, const char *text, uint16_t foreground, uint16_t background, uint8_t scale)
{
    uint16_t cursorX = x;
    uint16_t cursorY = y;
    uint16_t charStep = static_cast<uint16_t>(8 * scale);

    while (*text) {
        if (*text == '\n' || cursorX + charStep > config_.width) {
            cursorX = x;
            cursorY += charStep + scale;
            if (*text == '\n') {
                text++;
                continue;
            }
        }

        if (cursorY + charStep > config_.height) {
            return;
        }

        drawChar(cursorX, cursorY, *text, foreground, background, scale);
        cursorX += charStep;
        text++;
    }
}

void Ili9341Display::clearLine(uint16_t y, uint16_t height)
{
    fillRect(0, y, config_.width, height, Black);
}

void Ili9341Display::chipSelect(bool value)
{
    gpio_put(config_.csPin, value);
}

void Ili9341Display::dataCommand(bool value)
{
    gpio_put(config_.dcPin, value);
}

void Ili9341Display::writeByte(uint8_t value)
{
    spi_write_blocking(config_.spi, &value, 1);
}

void Ili9341Display::writeCommand(uint8_t command)
{
    chipSelect(false);
    dataCommand(false);
    writeByte(command);
    chipSelect(true);
}

void Ili9341Display::writeData(const uint8_t *data, size_t length)
{
    chipSelect(false);
    dataCommand(true);
    spi_write_blocking(config_.spi, data, length);
    chipSelect(true);
}

void Ili9341Display::writeData(uint8_t value)
{
    writeData(&value, 1);
}

void Ili9341Display::writeColorRun(uint16_t color, uint32_t count)
{
    uint8_t bytes[64];
    for (size_t i = 0; i < sizeof(bytes); i += 2) {
        bytes[i] = static_cast<uint8_t>(color >> 8);
        bytes[i + 1] = static_cast<uint8_t>(color);
    }

    while (count > 0) {
        uint32_t pixels = count > 32 ? 32 : count;
        spi_write_blocking(config_.spi, bytes, pixels * 2);
        count -= pixels;
    }
}

void Ili9341Display::setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t data[4];

    writeCommand(ColumnAddressSet);
    data[0] = static_cast<uint8_t>(x0 >> 8);
    data[1] = static_cast<uint8_t>(x0);
    data[2] = static_cast<uint8_t>(x1 >> 8);
    data[3] = static_cast<uint8_t>(x1);
    writeData(data, sizeof(data));

    writeCommand(PageAddressSet);
    data[0] = static_cast<uint8_t>(y0 >> 8);
    data[1] = static_cast<uint8_t>(y0);
    data[2] = static_cast<uint8_t>(y1 >> 8);
    data[3] = static_cast<uint8_t>(y1);
    writeData(data, sizeof(data));

    writeCommand(MemoryWrite);
}

void Ili9341Display::drawChar(uint16_t x, uint16_t y, char value, uint16_t foreground, uint16_t background, uint8_t scale)
{
    if (static_cast<uint8_t>(value) >= 128) {
        value = '?';
    }

    for (uint8_t row = 0; row < 8; row++) {
        uint8_t bits = static_cast<uint8_t>(font8x8_basic[static_cast<uint8_t>(value)][row]);
        for (uint8_t col = 0; col < 8; col++) {
            uint16_t color = ((bits >> col) & 0x01u) ? foreground : background;
            fillRect(x + col * scale, y + row * scale, scale, scale, color);
        }
    }
}
