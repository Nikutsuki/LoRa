#pragma once

#include <cstdint>

#include "hardware/i2c.h"
#include "pico/types.h"

class CardKbKeyboard {
public:
    struct Config {
        i2c_inst_t *i2c;
        uint8_t address;
        uint sdaPin;
        uint sclPin;
        uint baud;
    };

    explicit CardKbKeyboard(Config config);

    void init();
    bool probe();
    bool readKey(uint8_t &key);
    uint8_t scanFirstAddress();

    int lastResult() const;
    uint8_t lastFoundAddress() const;

private:
    Config config_;
    int lastResult_ = 0;
    uint8_t lastFoundAddress_ = 0;
};
