#include "LoraTransmitter.hpp"

#include <cstring>

#include "board.h"
#include "delay.h"
#include "sx126x-board.h"

LoraTransmitter *LoraTransmitter::activeInstance_ = nullptr;

void LoraTransmitter::init()
{
    BoardInitMcu();
    BoardInitPeriph();

    activeInstance_ = this;
    std::memset(&radioEvents_, 0, sizeof(radioEvents_));
    radioEvents_.TxDone = onTxDone;
    radioEvents_.TxTimeout = onTxTimeout;

    Radio.Init(&radioEvents_);
    Radio.SetChannel(Frequency);
    Radio.SetTxConfig(MODEM_LORA, TxOutputPower, 0, Bandwidth,
                      SpreadingFactor, CodingRate,
                      PreambleLength, FixLengthPayload,
                      true, 0, 0, IqInversion, TxTimeoutValue);
}

void LoraTransmitter::processIrq()
{
    if (Radio.IrqProcess != nullptr) {
        Radio.IrqProcess();
    }
}

bool LoraTransmitter::send(const char *message, size_t length)
{
    if (message == nullptr || length == 0 || length > MaxPayloadSize || txBusy_) {
        return false;
    }

    std::memcpy(txBuffer_, message, length);
    txDone_ = false;
    txTimeout_ = false;
    txBusy_ = true;

    SX126xAntSwOff();
    DelayMs(10);
    Radio.Send(txBuffer_, static_cast<uint8_t>(length));
    return true;
}

LoraTransmitter::Event LoraTransmitter::consumeEvent()
{
    if (txBusy_ && txDone_) {
        txBusy_ = false;
        txDone_ = false;
        SX126xAntSwOn();
        return Event::TxDone;
    }

    if (txBusy_ && txTimeout_) {
        txBusy_ = false;
        txTimeout_ = false;
        SX126xAntSwOn();
        return Event::TxTimeout;
    }

    return Event::None;
}

bool LoraTransmitter::isBusy() const
{
    return txBusy_;
}

void LoraTransmitter::onTxDone()
{
    if (activeInstance_ != nullptr) {
        activeInstance_->txDone_ = true;
    }
}

void LoraTransmitter::onTxTimeout()
{
    if (activeInstance_ != nullptr) {
        activeInstance_->txTimeout_ = true;
    }
}
