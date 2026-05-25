#include "HardwareTestApp.hpp"

#include <cstdio>
#include <cstring>

#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

namespace {
constexpr uint TftCs = 5;
constexpr uint TftSck = 6;
constexpr uint TftMosi = 7;
constexpr uint TftDc = 8;
constexpr uint TftReset = 9;
constexpr uint8_t TftMadctlValue = 0x68;
constexpr uint TftSpiBaud = 30000000u;
constexpr uint16_t TftWidth = 320;
constexpr uint16_t TftHeight = 240;

constexpr uint8_t CardKbAddress = 0x5F;
constexpr uint CardKbSda = 26;
constexpr uint CardKbScl = 27;
constexpr uint CardKbBaud = 100000;
}

HardwareTestApp::HardwareTestApp()
    : display_({ spi0, TftSpiBaud, TftWidth, TftHeight, TftCs, TftSck, TftMosi, TftDc, TftReset, TftMadctlValue }),
      keyboard_({ i2c1, CardKbAddress, CardKbSda, CardKbScl, CardKbBaud })
{
}

void HardwareTestApp::run()
{
    stdio_init_all();
    sleep_ms(1000);

    initializeDisplay();
    initializeKeyboard();

    drawStatus("Initializing LoRa radio", Ili9341Display::Yellow);
    radio_.init();
    drawStatus("LoRa TX ready on 433 MHz", Ili9341Display::Green);

    nextKeyboardStatus_ = make_timeout_time_ms(1000);

    while (true) {
        pollRadio();
        pollKeyboard();
        refreshKeyboardStatus();
        sleep_ms(20);
    }
}

void HardwareTestApp::initializeDisplay()
{
    display_.init();
    drawStaticUi();
    drawStatus("Display initialized", Ili9341Display::Green);
}

void HardwareTestApp::initializeKeyboard()
{
    keyboard_.init();
    if (keyboard_.probe()) {
        drawKeyboardStatus("CardKB ACK on I2C1 addr 0x5F", Ili9341Display::Green);
        return;
    }

    char status[64];
    uint8_t foundAddress = keyboard_.scanFirstAddress();
    if (foundAddress != 0) {
        std::snprintf(status, sizeof(status), "I2C found 0x%02X, CardKB 0x5F result %d", foundAddress, keyboard_.lastResult());
    } else {
        std::snprintf(status, sizeof(status), "I2C found nothing, 0x5F result %d", keyboard_.lastResult());
    }
    drawKeyboardStatus(status, Ili9341Display::Red);
}

void HardwareTestApp::drawStaticUi()
{
    display_.fillRect(0, 0, TftWidth, TftHeight, Ili9341Display::Black);
    display_.drawText(8, 8, "ABC 123", Ili9341Display::White, Ili9341Display::Black, 2);
    display_.drawText(8, 32, "RP2040 LoRa hardware test", Ili9341Display::Cyan, Ili9341Display::Black, 2);
    display_.drawText(8, 56, "Type on CardKB. Enter sends LoRa.", Ili9341Display::Gray, Ili9341Display::Black, 1);
    display_.drawText(8, 76, "Input:", Ili9341Display::Yellow, Ili9341Display::Black, 2);
    display_.drawText(8, 188, "Status:", Ili9341Display::Yellow, Ili9341Display::Black, 2);
}

void HardwareTestApp::drawInput()
{
    display_.clearLine(102, 48);
    display_.drawText(8, 102, input_, Ili9341Display::White, Ili9341Display::Black, 2);
}

void HardwareTestApp::drawStatus(const char *status, uint16_t color)
{
    display_.clearLine(214, 18);
    display_.drawText(8, 214, status, color, Ili9341Display::Black, 1);
    std::printf("%s\r\n", status);
}

void HardwareTestApp::drawKeyboardStatus(const char *status, uint16_t color)
{
    display_.clearLine(158, 16);
    display_.drawText(8, 158, status, color, Ili9341Display::Black, 1);
    std::printf("%s\r\n", status);
}

void HardwareTestApp::handleKey(uint8_t key)
{
    if (key == '\r' || key == '\n') {
        sendCurrentInput();
        return;
    }

    if (key == 8 || key == 127) {
        if (inputLength_ > 0) {
            input_[--inputLength_] = '\0';
            drawInput();
        }
        return;
    }

    if (key >= 32 && key <= 126 && inputLength_ < InputMax - 1) {
        input_[inputLength_++] = static_cast<char>(key);
        input_[inputLength_] = '\0';
        drawInput();
    }
}

void HardwareTestApp::sendCurrentInput()
{
    if (inputLength_ == 0 || radio_.isBusy()) {
        return;
    }

    char status[64];
    std::snprintf(status, sizeof(status), "TX %u bytes: %s", static_cast<unsigned>(inputLength_), input_);
    drawStatus(status, Ili9341Display::Green);

    if (!radio_.send(input_, inputLength_)) {
        drawStatus("LoRa TX rejected", Ili9341Display::Red);
        return;
    }

    std::memset(input_, 0, sizeof(input_));
    inputLength_ = 0;
    drawInput();
}

void HardwareTestApp::pollRadio()
{
    radio_.processIrq();

    switch (radio_.consumeEvent()) {
    case LoraTransmitter::Event::TxDone:
        drawStatus("LoRa TX complete", Ili9341Display::Green);
        break;
    case LoraTransmitter::Event::TxTimeout:
        drawStatus("LoRa TX timeout", Ili9341Display::Red);
        break;
    case LoraTransmitter::Event::None:
        break;
    }
}

void HardwareTestApp::pollKeyboard()
{
    uint8_t key = 0;
    if (!keyboard_.readKey(key)) {
        return;
    }

    char status[48];
    std::snprintf(status, sizeof(status), "CardKB key 0x%02X '%c'", key, key >= 32 && key <= 126 ? key : '.');
    drawKeyboardStatus(status, Ili9341Display::Green);
    std::printf("CardKB key: 0x%02X '%c'\r\n", key, key >= 32 && key <= 126 ? key : '.');
    handleKey(key);
}

void HardwareTestApp::refreshKeyboardStatus()
{
    if (absolute_time_diff_us(get_absolute_time(), nextKeyboardStatus_) > 0) {
        return;
    }

    if (keyboard_.lastResult() == 1) {
        drawKeyboardStatus("CardKB ACK, waiting for key", Ili9341Display::Green);
    } else {
        char status[64];
        std::snprintf(status, sizeof(status), "CardKB read failed, result %d", keyboard_.lastResult());
        drawKeyboardStatus(status, Ili9341Display::Red);
        keyboard_.init();
    }

    nextKeyboardStatus_ = make_timeout_time_ms(1000);
}
