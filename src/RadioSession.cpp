#include "RadioSession.hpp"

#include <cstdio>
#include <cstring>

#include "Crypto.hpp"
#include "FileTransfer.hpp"
#include "Packet.hpp"
#include "board.h"

namespace {

const uint8_t *deriveOptionalKey(const char *password, uint8_t out[lora::FileKeySize])
{
    if (password == nullptr || password[0] == '\0') {
        return nullptr;
    }
    lora::Crypto::derivePasswordKey(out, password, std::strlen(password));
    return out;
}

} // namespace

RadioSession::RadioSession(NoticeSink notice)
    : notice_(notice)
{
}

void RadioSession::init()
{
    initializeIdentity();

    notice_.emit("Initializing LoRa radio", ui::Tone::Focus);
    radio_.init();
    notice_.emit("Listening on 433 MHz", ui::Tone::Success);
    dirty_ = true;
}

void RadioSession::initializeIdentity()
{
    contacts_.load();
    if (contacts_.hasIdentity()) {
        notice_.emit("Identity loaded from flash", ui::Tone::Success);
        return;
    }

    uint8_t deviceId[lora::DeviceIdSize];
    BoardGetUniqueId(deviceId);

    uint8_t seed[32];
    lora::Crypto::deriveDevelopmentSeed(seed, deviceId);

    lora::DeviceIdentity identity;
    lora::Crypto::makeIdentity(identity, deviceId, "pico", seed, true);
    contacts_.setIdentity(identity);
    contacts_.save();
    notice_.emit("DEV identity saved", ui::Tone::Focus);
}

void RadioSession::poll(FileTransfer *fileTransfer)
{
    radio_.processIrq();

    switch (radio_.consumeEvent()) {
    case lora::Radio::Event::PacketReceived:
        handleReceivedPacket(fileTransfer);
        break;
    case lora::Radio::Event::TxDone:
        if (fileTransfer != nullptr && fileTransfer->onTxDone()) {
            break;
        }
        notice_.emit("LoRa TX complete", ui::Tone::Success);
        break;
    case lora::Radio::Event::TxTimeout:
        if (fileTransfer != nullptr && fileTransfer->onTxTimeout()) {
            break;
        }
        notice_.emit("LoRa TX timeout", ui::Tone::Danger);
        break;
    case lora::Radio::Event::RxError:
        notice_.emit("LoRa RX error", ui::Tone::Danger);
        break;
    case lora::Radio::Event::None:
        break;
    }

    if (awaitingAck_ && time_reached(ackDeadline_)) {
        lora::ChatEntry &message = history_.at(awaitingAckIndex_);
        message.status = lora::ChatEntry::Status::Failed;
        notice_.emit("Message ack timeout", ui::Tone::Danger);
        awaitingAck_ = false;
        awaitingAckIndex_ = 0;
        awaitingAckSeq_ = 0;
        dirty_ = true;
    }
}

void RadioSession::sendTextMessage(const char *text)
{
    if (text == nullptr || text[0] == '\0' || radio_.isBusy()) {
        return;
    }

    const lora::Contact *contact = selectedContact();
    if (contact == nullptr) {
        notice_.emit("No selected contact", ui::Tone::Danger);
        return;
    }

    lora::PacketBuffer packet;
    const lora::PeerSession peer{contacts_.identity(), *contact};
    if (!lora::Packet::encodeMessage(peer, sequence_++, text, packet)) {
        notice_.emit("Encrypt failed", ui::Tone::Danger);
        return;
    }

    if (!radio_.sendPacket(packet)) {
        notice_.emit("LoRa TX rejected", ui::Tone::Danger);
        return;
    }

    // Record outgoing text only after the radio accepts the packet so the UI
    // never displays a message that failed to transmit.
    const size_t messageIndex = history_.addOutgoing(text);
    history_.at(messageIndex).status = lora::ChatEntry::Status::Sending;

    awaitingAck_ = true;
    ackDeadline_ = make_timeout_time_ms(AckTimeoutMs);
    awaitingAckIndex_ = messageIndex;
    awaitingAckSeq_ = sequence_ - 1;
    dirty_ = true;
}

void RadioSession::sendInvite(const char *password)
{
    uint8_t pairingKey[lora::FileKeySize];
    const uint8_t *keyPtr = deriveOptionalKey(password, pairingKey);

    lora::PacketBuffer packet;
    if (!lora::Packet::encodeInvite(contacts_.identity(),
                                    lora::InviteParams::broadcast(sequence_++, keyPtr),
                                    packet)) {
        notice_.emit("Invite encode failed", ui::Tone::Danger);
        return;
    }
    if (!radio_.sendPacket(packet)) {
        notice_.emit("Invite TX rejected", ui::Tone::Danger);
        return;
    }
    notice_.emit(keyPtr != nullptr ? "Password invite sent" : "Open invite sent", ui::Tone::Success);
}

void RadioSession::acceptPendingInvite(const char *password)
{
    uint8_t pairingKey[lora::FileKeySize];
    const uint8_t *keyPtr = deriveOptionalKey(password, pairingKey);

    if (hasPendingEncryptedInvite_) {
        if (keyPtr == nullptr) {
            notice_.emit("Enter password to accept", ui::Tone::Danger);
            return;
        }

        lora::Contact decryptedContact;
        if (!lora::Packet::decodeInvite(pendingEncryptedPacket_, keyPtr, decryptedContact)) {
            notice_.emit("Wrong password!", ui::Tone::Danger);
            return;
        }

        pendingInvite_ = decryptedContact;
        hasPendingEncryptedInvite_ = false;
        hasPendingInvite_ = true;
    }

    if (!hasPendingInvite_) {
        notice_.emit("No pending invite", ui::Tone::Danger);
        return;
    }
    if (contacts_.nicknameCollisionWithDifferentKey(pendingInvite_)) {
        notice_.emit("Nickname impersonation rejected", ui::Tone::Danger);
        hasPendingInvite_ = false;
        dirty_ = true;
        return;
    }
    if (!contacts_.upsertContact(pendingInvite_)) {
        notice_.emit("Contact store full", ui::Tone::Danger);
        return;
    }
    contacts_.save();
    selectedContactIndex_ = contacts_.contactCount() - 1;
    dirty_ = true;

    lora::PacketBuffer packet;
    if (lora::Packet::encodeInvite(contacts_.identity(),
                                   lora::InviteParams::accept(pendingInvite_.deviceId, sequence_++, keyPtr),
                                   packet)) {
        radio_.sendPacket(packet);
    }
    hasPendingInvite_ = false;
    hasPendingEncryptedInvite_ = false;

    notice_.emit("Contact accepted & paired", ui::Tone::Success);
}

void RadioSession::selectContact(size_t index)
{
    if (index < contacts_.contactCount()) {
        selectedContactIndex_ = index;
        dirty_ = true;
    }
}

void RadioSession::selectNextContact()
{
    const size_t count = contacts_.contactCount();
    if (count == 0) {
        notice_.emit("No contacts", ui::Tone::Danger);
        return;
    }
    selectedContactIndex_ = (selectedContactIndex_ + 1) % count;
    dirty_ = true;
    notice_.emit("Contact selected", ui::Tone::Success);
}

void RadioSession::setNickname(const char *nickname)
{
    if (nickname == nullptr || nickname[0] == '\0') {
        notice_.emit("Nickname missing", ui::Tone::Danger);
        return;
    }
    std::snprintf(contacts_.identity().nickname, lora::NicknameSize, "%s", nickname);
    contacts_.save();
    dirty_ = true;
    notice_.emit("Nickname saved", ui::Tone::Success);
}

void RadioSession::provisionSeed(const char *hexSeed)
{
    uint8_t seed[lora::FileKeySize];
    if (!parseHexSeed(hexSeed, seed)) {
        notice_.emit("Seed must be 64 hex chars", ui::Tone::Danger);
        return;
    }

    lora::DeviceIdentity identity;
    lora::Crypto::makeIdentity(identity, contacts_.identity().deviceId, contacts_.identity().nickname, seed, false);
    contacts_.setIdentity(identity);
    contacts_.clearContacts();
    contacts_.save();
    selectedContactIndex_ = 0;
    hasPendingInvite_ = false;
    hasPendingEncryptedInvite_ = false;
    dirty_ = true;
    notice_.emit("Seed provisioned; contacts cleared", ui::Tone::Success);
}

bool RadioSession::consumeDirty()
{
    const bool wasDirty = dirty_;
    dirty_ = false;
    return wasDirty;
}

const lora::Contact *RadioSession::selectedContact() const
{
    return contacts_.contactAt(selectedContactIndex_);
}

void RadioSession::handleReceivedPacket(FileTransfer *fileTransfer)
{
    lora::DecodedPacket packet;
    if (!lora::Packet::decode(radio_.receivedPacket(), radio_.receivedPacketLength(), packet) ||
        !lora::Packet::isForDevice(packet, contacts_.identity().deviceId)) {
        return;
    }

    if (std::memcmp(packet.header.senderDeviceId, contacts_.identity().deviceId, lora::DeviceIdSize) == 0) {
        return;
    }

    if (fileTransfer != nullptr && fileTransfer->handlePacket(packet)) {
        return;
    }

    switch (packet.header.type) {
    case lora::PacketType::MessageAck:
        handleAckPacket(packet);
        break;
    case lora::PacketType::InviteBroadcast:
    case lora::PacketType::InviteAccept:
        handleInvitePacket(packet);
        break;
    case lora::PacketType::EncryptedMessage:
        handleChatPacket(packet);
        break;
    default:
        break;
    }
}

void RadioSession::handleAckPacket(const lora::DecodedPacket &packet)
{
    if (awaitingAck_ && packet.header.sequence == awaitingAckSeq_) {
        history_.at(awaitingAckIndex_).status = lora::ChatEntry::Status::Delivered;
        notice_.emit("Message acknowledged", ui::Tone::Success);

        awaitingAck_ = false;
        awaitingAckIndex_ = 0;
        awaitingAckSeq_ = 0;
        dirty_ = true;
    }
}

void RadioSession::handleInvitePacket(const lora::DecodedPacket &packet)
{
    lora::Contact contact;
    if (lora::Packet::decodeInvite(packet, nullptr, contact)) {
        if (contacts_.nicknameCollisionWithDifferentKey(contact)) {
            notice_.emit("Impersonation invite rejected", ui::Tone::Danger);
            return;
        }
        if (packet.header.type == lora::PacketType::InviteAccept) {
            contacts_.upsertContact(contact);
            contacts_.save();
            dirty_ = true;
            notice_.emit("Invite accepted by peer", ui::Tone::Success);
            return;
        }
        pendingInvite_ = contact;
        hasPendingInvite_ = true;
        hasPendingEncryptedInvite_ = false;
        char status[64];
        std::snprintf(status, sizeof(status), "Invite from %s", contact.nickname);
        notice_.emit(status, ui::Tone::Focus);
        dirty_ = true;
        return;
    }

    constexpr size_t EncryptedInviteSize =
        lora::NonceSize + lora::MacSize +
        (lora::NicknameSize + lora::PublicKeySize + lora::PublicKeySize + lora::SignatureSize);
    if (packet.payloadLength == EncryptedInviteSize && packet.payloadLength <= sizeof(pendingEncryptedPayload_)) {
        pendingEncryptedPacket_ = packet;
        std::memcpy(pendingEncryptedPayload_, packet.payload, packet.payloadLength);
        pendingEncryptedPacket_.payload = pendingEncryptedPayload_;
        hasPendingEncryptedInvite_ = true;
        hasPendingInvite_ = false;
        notice_.emit("Encrypted invite received! Enter pass", ui::Tone::Focus);
        dirty_ = true;
        return;
    }

    notice_.emit("Bad invite payload/key", ui::Tone::Danger);
}

void RadioSession::handleChatPacket(const lora::DecodedPacket &packet)
{
    const lora::Contact *contact = contacts_.findByDeviceId(packet.header.senderDeviceId);
    if (contact == nullptr) {
        notice_.emit("Encrypted msg from unknown", ui::Tone::Danger);
        return;
    }

    char message[lora::MaxPlainTextSize + 1];
    const lora::PeerSession peer{contacts_.identity(), *contact};
    if (!lora::Packet::decodeMessage(peer, packet, message, sizeof(message))) {
        notice_.emit("Message auth failed", ui::Tone::Danger);
        return;
    }

    history_.addIncoming(contact->nickname, message,
                         static_cast<float>(radio_.lastRssi()),
                         static_cast<float>(radio_.lastSnr()));
    sendAck(contact->deviceId, packet.header.sequence);
    dirty_ = true;
    notice_.emit("Encrypted message received", ui::Tone::Success);
}

void RadioSession::sendAck(const uint8_t recipientDeviceId[lora::DeviceIdSize], uint32_t sequence)
{
    lora::PacketBuffer packet;
    const auto route = lora::PacketRoute::toDevice(contacts_.identity().deviceId, recipientDeviceId, sequence);
    if (!lora::Packet::encodeControl(lora::PacketType::MessageAck, route, packet)) {
        notice_.emit("Ack encode failed", ui::Tone::Danger);
        return;
    }
    if (!radio_.sendPacket(packet)) {
        notice_.emit("Ack TX rejected", ui::Tone::Danger);
    }
}

bool RadioSession::parseHexSeed(const char *hexSeed, uint8_t out[lora::FileKeySize]) const
{
    if (hexSeed == nullptr || std::strlen(hexSeed) != lora::HexSeedSize) {
        return false;
    }

    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };

    for (size_t i = 0; i < lora::FileKeySize; i++) {
        const int high = nibble(hexSeed[i * 2]);
        const int low = nibble(hexSeed[i * 2 + 1]);
        if (high < 0 || low < 0) {
            return false;
        }
        out[i] = static_cast<uint8_t>((high << 4) | low);
    }
    return true;
}
