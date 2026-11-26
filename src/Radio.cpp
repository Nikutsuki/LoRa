#include "Radio.hpp"

#include <cstring>

#include "board.h"
#include "delay.h"
#include "sx126x-board.h"

namespace lora {

Radio *Radio::activeInstance_ = nullptr;

void Radio::init()
{
    BoardInitMcu();
    BoardInitPeriph();

    activeInstance_ = this;
    std::memset(&radioEvents_, 0, sizeof(radioEvents_));
    radioEvents_.TxDone = onTxDone;
    radioEvents_.TxTimeout = onTxTimeout;
    radioEvents_.RxDone = onRxDone;
    radioEvents_.RxTimeout = onRxTimeout;
    radioEvents_.RxError = onRxError;

    ::Radio.Init(&radioEvents_);
    configureRadio();
    startReceive();
}

void Radio::processIrq()
{
    if (::Radio.IrqProcess != nullptr) {
        ::Radio.IrqProcess();
    }
}

bool Radio::sendPacket(const uint8_t *packet, size_t length)
{
    if (packet == nullptr || length == 0 || length > MaxPacketSize || radioBusy_) {
        return false;
    }

    std::memcpy(txBuffer_, packet, length);
    txDone_ = false;
    txTimeout_ = false;
    radioBusy_ = true;

    SX126xAntSwOff();
    DelayMs(10);
    ::Radio.Send(txBuffer_, static_cast<uint8_t>(length));
    return true;
}

bool Radio::sendPacket(const PacketBuffer &packet)
{
    return sendPacket(packet.data, packet.length);
}

Radio::Event Radio::consumeEvent()
{
    if (rxDone_) {
        rxDone_ = false;
        return Event::PacketReceived;
    }

    if (rxError_) {
        rxError_ = false;
        startReceive();
        return Event::RxError;
    }

    if (radioBusy_ && txDone_) {
        radioBusy_ = false;
        txDone_ = false;
        SX126xAntSwOn();
        startReceive();
        return Event::TxDone;
    }

    if (radioBusy_ && txTimeout_) {
        radioBusy_ = false;
        txTimeout_ = false;
        SX126xAntSwOn();
        startReceive();
        return Event::TxTimeout;
    }

    return Event::None;
}

bool Radio::isBusy() const
{
    return radioBusy_;
}

const uint8_t *Radio::receivedPacket() const
{
    return rxBuffer_;
}

size_t Radio::receivedPacketLength() const
{
    return rxLength_;
}

void Radio::startReceive()
{
    if (radioBusy_) {
        return;
    }
    SX126xAntSwOn();
    ::Radio.Rx(0);
}

void Radio::configureRadio()
{
    ::Radio.SetChannel(Frequency);
    ::Radio.SetTxConfig(MODEM_LORA, TxOutputPower, 0, Bandwidth,
                        SpreadingFactor, CodingRate,
                        PreambleLength, FixLengthPayload,
                        true, 0, 0, IqInversion, TxTimeoutValue);
    ::Radio.SetRxConfig(MODEM_LORA, Bandwidth, SpreadingFactor, CodingRate,
                        0, PreambleLength, 0, FixLengthPayload,
                        MaxPacketSize, true, 0, 0, IqInversion, true);
}

void Radio::onTxDone()
{
    if (activeInstance_ != nullptr) {
        activeInstance_->txDone_ = true;
    }
}

void Radio::onTxTimeout()
{
    if (activeInstance_ != nullptr) {
        activeInstance_->txTimeout_ = true;
    }
}

void Radio::onRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
    if (activeInstance_ == nullptr) {
        return;
    }
    size_t copySize = size > MaxPacketSize ? MaxPacketSize : size;
    std::memcpy(activeInstance_->rxBuffer_, payload, copySize);
    activeInstance_->rxLength_ = copySize;
    activeInstance_->lastRssi_ = rssi;
    activeInstance_->lastSnr_ = snr;
    activeInstance_->rxDone_ = true;
    activeInstance_->startReceive();
}

void Radio::onRxTimeout()
{
    if (activeInstance_ != nullptr) {
        activeInstance_->startReceive();
    }
}

void Radio::onRxError()
{
    if (activeInstance_ != nullptr) {
        activeInstance_->rxError_ = true;
    }
}

}
