#pragma once

#include <cstddef>
#include <cstdint>

#include "CardKbKeyboard.hpp"
#include "Ili9341Display.hpp"
#include "LoraTransmitter.hpp"
#include "pico/time.h"

class HardwareTestApp {
public:
    HardwareTestApp();

    void run();

private:
    static constexpr size_t InputMax = LoraTransmitter::MaxPayloadSize;

    void initializeDisplay();
    void initializeKeyboard();
    void drawStaticUi();
    void drawInput();
    void drawStatus(const char *status, uint16_t color);
    void drawKeyboardStatus(const char *status, uint16_t color);
    void handleKey(uint8_t key);
    void sendCurrentInput();
    void pollRadio();
    void pollKeyboard();
    void refreshKeyboardStatus();

    Ili9341Display display_;
    CardKbKeyboard keyboard_;
    LoraTransmitter radio_;
    char input_[InputMax]{};
    size_t inputLength_ = 0;
    absolute_time_t nextKeyboardStatus_{};
};
