#pragma once

#include <cstddef>
#include <cstdint>

#include "CryptoTypes.hpp"
#include "radio.h"

namespace crypto_chat {

class LoraMessenger {
public:
    enum class Event {
        None,
        PacketReceived,
        TxDone,
        TxTimeout,
        RxError,
    };

    void init();
    void processIrq();
    bool sendPacket(const uint8_t *packet, size_t length);
    Event consumeEvent();
    bool isBusy() const;
    const uint8_t *receivedPacket() const;
    size_t receivedPacketLength() const;
    void startReceive();

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
    static void onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
    static void onRxTimeout();
    static void onRxError();

    void configureRadio();

    RadioEvents_t radioEvents_{};
    uint8_t txBuffer_[MaxPacketSize]{};
    uint8_t rxBuffer_[MaxPacketSize]{};
    size_t rxLength_ = 0;
    volatile bool txDone_ = false;
    volatile bool txTimeout_ = false;
    volatile bool rxDone_ = false;
    volatile bool rxError_ = false;
    bool radioBusy_ = false;

    static LoraMessenger *activeInstance_;
};

}
