#pragma once

#include <cstddef>
#include <cstdint>

#include "radio.h"

class LoraTransmitter {
public:
    enum class Event {
        None,
        TxDone,
        TxTimeout,
    };

    static constexpr size_t MaxPayloadSize = 128;

    void init();
    void processIrq();
    bool send(const char *message, size_t length);
    Event consumeEvent();
    bool isBusy() const;

private:
    static constexpr uint32_t Frequency = 433000000;
    static constexpr int8_t TxOutputPower = 22;
    static constexpr uint32_t Bandwidth = 2;
    static constexpr uint32_t SpreadingFactor = 12;
    static constexpr uint8_t CodingRate = 1;
    static constexpr uint16_t PreambleLength = 8;
    static constexpr bool FixLengthPayload = false;
    static constexpr bool IqInversion = false;
    static constexpr uint32_t TxTimeoutValue = 3000;

    static void onTxDone();
    static void onTxTimeout();

    RadioEvents_t radioEvents_{};
    uint8_t txBuffer_[MaxPayloadSize]{};
    volatile bool txDone_ = false;
    volatile bool txTimeout_ = false;
    bool txBusy_ = false;

    static LoraTransmitter *activeInstance_;
};
