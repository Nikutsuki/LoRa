#include "CardKbKeyboard.hpp"

#include "hardware/gpio.h"
#include "pico/stdlib.h"

CardKbKeyboard::CardKbKeyboard(Config config)
    : config_(config)
{
}

void CardKbKeyboard::init()
{
    i2c_deinit(config_.i2c);
    i2c_init(config_.i2c, config_.baud);
    gpio_set_function(config_.sdaPin, GPIO_FUNC_I2C);
    gpio_set_function(config_.sclPin, GPIO_FUNC_I2C);
    gpio_pull_up(config_.sdaPin);
    gpio_pull_up(config_.sclPin);
    sleep_ms(10);
}

bool CardKbKeyboard::probe()
{
    uint8_t value = 0;
    lastResult_ = i2c_read_timeout_us(config_.i2c, config_.address, &value, 1, false, 5000);
    return lastResult_ == 1;
}

bool CardKbKeyboard::readKey(uint8_t &key)
{
    uint8_t value = 0;
    lastResult_ = i2c_read_timeout_us(config_.i2c, config_.address, &value, 1, false, 1000);
    if (lastResult_ == 1 && value != 0) {
        key = value;
        return true;
    }
    return false;
}

uint8_t CardKbKeyboard::scanFirstAddress()
{
    uint8_t value = 0;

    for (uint8_t address = 0x08; address <= 0x77; address++) {
        int result = i2c_read_timeout_us(config_.i2c, address, &value, 1, false, 1000);
        if (result == 1) {
            lastFoundAddress_ = address;
            return address;
        }
    }

    lastFoundAddress_ = 0;
    return 0;
}

int CardKbKeyboard::lastResult() const
{
    return lastResult_;
}

uint8_t CardKbKeyboard::lastFoundAddress() const
{
    return lastFoundAddress_;
}
