#pragma once

#include <cstddef>
#include <cstdint>

#include "hardware/spi.h"
#include "pico/types.h"

class Ili9341Display {
public:
    struct Config {
        spi_inst_t *spi;
        uint baud;
        uint16_t width;
        uint16_t height;
        uint csPin;
        uint sckPin;
        uint mosiPin;
        uint dcPin;
        uint resetPin;
        uint8_t madctlValue;
    };

    static constexpr uint16_t Black = 0x0000;
    static constexpr uint16_t White = 0xFFFF;
    static constexpr uint16_t Green = 0x07E0;
    static constexpr uint16_t Cyan = 0x07FF;
    static constexpr uint16_t Yellow = 0xFFE0;
    static constexpr uint16_t Red = 0xF800;
    static constexpr uint16_t Gray = 0x8410;
    static constexpr uint16_t DarkGray = 0x2104;
    static constexpr uint16_t Blue = 0x025F;
    static constexpr uint16_t Navy = 0x0010;

    explicit Ili9341Display(Config config);

    void init();
    void fillRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
    void drawRect(uint16_t x, uint16_t y, uint16_t width, uint16_t height, uint16_t color);
    void drawText(uint16_t x, uint16_t y, const char *text, uint16_t foreground, uint16_t background, uint8_t scale);
    void clearLine(uint16_t y, uint16_t height);

private:
    static constexpr uint8_t SoftwareReset = 0x01;
    static constexpr uint8_t SleepOut = 0x11;
    static constexpr uint8_t DisplayOn = 0x29;
    static constexpr uint8_t ColumnAddressSet = 0x2A;
    static constexpr uint8_t PageAddressSet = 0x2B;
    static constexpr uint8_t MemoryWrite = 0x2C;
    static constexpr uint8_t MemoryAccessControl = 0x36;
    static constexpr uint8_t PixelFormatSet = 0x3A;

    void chipSelect(bool value);
    void dataCommand(bool value);
    void writeByte(uint8_t value);
    void writeCommand(uint8_t command);
    void writeData(const uint8_t *data, size_t length);
    void writeData(uint8_t value);
    void writeColorRun(uint16_t color, uint32_t count);
    void setWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void drawChar(uint16_t x, uint16_t y, char value, uint16_t foreground, uint16_t background, uint8_t scale);

    Config config_;
};
