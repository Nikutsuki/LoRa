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
    int16_t lastRssi() const { return lastRssi_; }
    int8_t lastSnr() const { return lastSnr_; }

private:
    static constexpr uint32_t Frequency = 433000000;

    /*!
     * Transmit power in dBm.
     *
     * Capped well below the 22 dBm maximum because bench setups park the boards
     * centimetres apart, where full power saturates the receiver front end and
     * shows up as CRC failures on received packets.
     */
    static constexpr int8_t TxOutputPower = 14;

    /*!
     * LoRa bandwidth, as an index into Radio's Bandwidths table
     * { LORA_BW_125, LORA_BW_250, LORA_BW_500 }.
     *
     * So 0 = 125 kHz, 1 = 250 kHz, 2 = 500 kHz. Prefer the named constants below
     * over raw numbers: 2 does not mean 125 kHz.
     *
     * The previous value was 2 (500 kHz), the widest and least sensitive option:
     * it roughly halves the link budget versus 125 kHz because the receiver
     * integrates over a wider noise band. Combined with boards sitting
     * centimetres apart that produced CRC failures on received packets.
     */
    static constexpr uint32_t BandwidthIndex125kHz = 0;
    static constexpr uint32_t Bandwidth = BandwidthIndex125kHz;

    /*!
     * Spreading factor.
     *
     * SF12 is the slowest and most sensitive setting; it also spends the longest
     * time on air, so a marginal link produces CRC errors for longer before
     * either succeeding or failing. SF9 keeps ample margin for a bench test while
     * being roughly 8x quicker, which makes the failure mode far easier to
     * observe.
     */
    static constexpr uint32_t SpreadingFactor = 9;

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
    int16_t lastRssi_ = 0;
    int8_t lastSnr_ = 0;
    bool radioBusy_ = false;

    static LoraMessenger *activeInstance_;
};

}
