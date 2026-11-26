#pragma once

#include <cstdint>

#include "NoticeSink.hpp"
#include "Types.hpp"
#include "protocol/LinkSession.hpp"

class RadioSession;

/*!
 * Coordinates encrypted file transfers between the USB host LinkSession
 * and the LoRa RadioSession.
 */
class FileTransfer {
public:
    enum class Role {
        None,
        Sending,
        Receiving,
    };

    FileTransfer(RadioSession &radioSession, proto::LinkSession &linkSession, NoticeSink notice = {});

    void poll();
    bool handlePacket(const lora::DecodedPacket &packet);
    bool onTxDone();
    bool onTxTimeout();
    void cancel(bool notifyPeer);

    bool consumeDirty();

    bool isActive() const { return role_ != Role::None; }
    bool isSending() const { return role_ == Role::Sending; }
    const char *filename() const { return filename_; }
    uint32_t fileSize() const { return fileSize_; }
    uint32_t bytesTransferred() const { return bytesTransferred_; }

private:
    void startOutgoing(const proto::FileExchangeStartMessage &msg);
    void sendChunk(const proto::FileExchangeDataMessage &msg);
    void finishOutgoing();

    void handleIncomingStart(const lora::DecodedPacket &packet);
    void handleIncomingData(const lora::DecodedPacket &packet);
    void handleIncomingEnd();

    RadioSession &radioSession_;
    proto::LinkSession &linkSession_;
    NoticeSink notice_;

    Role role_ = Role::None;
    uint8_t currentFileKey_[32]{};
    char filename_[64]{};
    uint32_t fileSize_ = 0;
    uint32_t bytesTransferred_ = 0;
    bool awaitingTxAck_ = false;
    uint32_t pendingAckChunk_ = 0;
    bool dirty_ = false;
};
