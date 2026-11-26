#pragma once

#include <cstddef>
#include <cstdint>

#include "ChatHistory.hpp"
#include "ContactStore.hpp"
#include "NoticeSink.hpp"
#include "Radio.hpp"
#include "Types.hpp"
#include "pico/time.h"

class FileTransfer;

/*!
 * Manages the LoRa radio, device identity, contacts, pairing invites,
 * encrypted messaging, and delivery acknowledgement timeouts.
 */
class RadioSession {
public:
    explicit RadioSession(NoticeSink notice = {});

    void init();
    void poll(FileTransfer *fileTransfer = nullptr);

    void sendTextMessage(const char *text);
    void sendInvite(const char *password);
    void acceptPendingInvite(const char *password);

    void selectContact(size_t index);
    void selectNextContact();
    void setNickname(const char *nickname);
    void provisionSeed(const char *hexSeed);

    bool consumeDirty();

    lora::Radio &radio() { return radio_; }
    const lora::Radio &radio() const { return radio_; }

    const lora::DeviceIdentity &identity() const { return contacts_.identity(); }
    const lora::ContactStore &contacts() const { return contacts_; }
    const lora::ChatHistory &history() const { return history_; }
    const lora::Contact *selectedContact() const;
    size_t selectedContactIndex() const { return selectedContactIndex_; }
    uint32_t nextSequence() { return sequence_++; }

    bool hasPendingInvite() const { return hasPendingInvite_; }
    const lora::Contact &pendingInvite() const { return pendingInvite_; }
    bool hasPendingEncryptedInvite() const { return hasPendingEncryptedInvite_; }

private:
    void initializeIdentity();
    void handleReceivedPacket(const char *activePassword, FileTransfer *fileTransfer);
    void handleReceivedPacket(FileTransfer *fileTransfer);
    void handleAckPacket(const lora::DecodedPacket &packet);
    void handleInvitePacket(const lora::DecodedPacket &packet);
    void handleChatPacket(const lora::DecodedPacket &packet);
    void sendAck(const uint8_t recipientDeviceId[lora::DeviceIdSize], uint32_t sequence);
    bool parseHexSeed(const char *hexSeed, uint8_t out[32]) const;

    lora::Radio radio_;
    lora::ContactStore contacts_;
    lora::ChatHistory history_;
    NoticeSink notice_;

    size_t selectedContactIndex_ = 0;
    uint32_t sequence_ = 1;

    bool awaitingAck_ = false;
    absolute_time_t ackDeadline_{};
    size_t awaitingAckIndex_ = 0;
    uint32_t awaitingAckSeq_ = 0;

    lora::Contact pendingInvite_{};
    bool hasPendingInvite_ = false;
    bool hasPendingEncryptedInvite_ = false;
    lora::DecodedPacket pendingEncryptedPacket_{};
    uint8_t pendingEncryptedPayload_[lora::MaxPacketSize]{};

    bool dirty_ = true;
};
