#include "FileTransfer.hpp"

#include <cstdio>
#include <cstring>

#include "Crypto.hpp"
#include "Packet.hpp"
#include "RadioSession.hpp"
#include "pico/time.h"

FileTransfer::FileTransfer(RadioSession &radioSession, proto::LinkSession &linkSession, NoticeSink notice)
    : radioSession_(radioSession),
      linkSession_(linkSession),
      notice_(notice)
{
}

void FileTransfer::poll()
{
    if (awaitingTxAck_ || radioSession_.radio().isBusy()) {
        return;
    }

    proto::FileExchangeStartMessage startMsg;
    if (linkSession_.nextFileStart(startMsg)) {
        startOutgoing(startMsg);
    }

    proto::FileExchangeDataMessage dataMsg;
    if (linkSession_.nextFileData(dataMsg)) {
        sendChunk(dataMsg);
    }

    if (linkSession_.nextFileEnd()) {
        finishOutgoing();
        return;
    }

    if (linkSession_.nextFileCancel()) {
        cancel(true);
    }
}

bool FileTransfer::handlePacket(const lora::DecodedPacket &packet)
{
    switch (packet.header.type) {
    case lora::PacketType::FileExchangeStart:
        handleIncomingStart(packet);
        return true;
    case lora::PacketType::FileExchangeData:
        handleIncomingData(packet);
        return true;
    case lora::PacketType::FileExchangeEnd:
        handleIncomingEnd();
        return true;
    case lora::PacketType::FileExchangeCancel:
        cancel(false);
        notice_.emit("Peer cancelled file transfer", ui::Tone::Danger);
        return true;
    default:
        return false;
    }
}

bool FileTransfer::onTxDone()
{
    if (!awaitingTxAck_) {
        return false;
    }
    awaitingTxAck_ = false;
    linkSession_.sendExchangeAck(pendingAckChunk_, 0);
    return true;
}

bool FileTransfer::onTxTimeout()
{
    if (!awaitingTxAck_) {
        return false;
    }
    awaitingTxAck_ = false;
    linkSession_.sendExchangeAck(pendingAckChunk_, 3);
    notice_.emit("LoRa TX timeout", ui::Tone::Danger);
    return true;
}

void FileTransfer::cancel(bool notifyPeer)
{
    if (role_ == Role::None) {
        return;
    }

    if (notifyPeer) {
        const lora::Contact *contact = radioSession_.selectedContact();
        if (contact != nullptr) {
            lora::PacketBuffer packet;
            const auto route = lora::PacketRoute::toContact(
                radioSession_.identity(), *contact, radioSession_.nextSequence());
            if (lora::Packet::encodeControl(lora::PacketType::FileExchangeCancel, route, packet)) {
                radioSession_.radio().sendPacket(packet);
            }
        }
    }

    role_ = Role::None;
    fileSize_ = 0;
    bytesTransferred_ = 0;
    awaitingTxAck_ = false;
    std::memset(currentFileKey_, 0, sizeof(currentFileKey_));

    linkSession_.sendFileExchangeCancel();
    dirty_ = true;
    notice_.emit("File transfer cancelled", ui::Tone::Danger);
}

bool FileTransfer::consumeDirty()
{
    const bool wasDirty = dirty_;
    dirty_ = false;
    return wasDirty;
}

void FileTransfer::startOutgoing(const proto::FileExchangeStartMessage &msg)
{
    const lora::Contact *contact = radioSession_.selectedContact();
    if (contact == nullptr) {
        linkSession_.sendExchangeAck(0, 1);
        notice_.emit("No contact selected", ui::Tone::Danger);
        return;
    }

    std::strncpy(filename_, msg.filename, sizeof(filename_) - 1);
    filename_[sizeof(filename_) - 1] = '\0';
    fileSize_ = msg.fileSize;
    bytesTransferred_ = 0;

    uint8_t seedMaterial[40]{};
    std::memcpy(seedMaterial, radioSession_.identity().deviceId, lora::DeviceIdSize);
    const uint64_t now = to_us_since_boot(get_absolute_time());
    std::memcpy(seedMaterial + 8, &now, sizeof(now));
    const uint32_t seq = radioSession_.nextSequence();
    std::memcpy(seedMaterial + 16, &seq, sizeof(seq));
    lora::Crypto::hash32(currentFileKey_, seedMaterial, sizeof(seedMaterial));

    lora::FileStartInfo info{};
    std::memcpy(info.fileKey, currentFileKey_, 32);
    std::strncpy(info.filename, filename_, sizeof(info.filename) - 1);
    info.fileSize = fileSize_;

    lora::PacketBuffer packet;
    const lora::PeerSession peer{radioSession_.identity(), *contact};
    if (!lora::Packet::encodeFileStart(peer, seq, info, packet)) {
        linkSession_.sendExchangeAck(0, 2);
        return;
    }

    if (!radioSession_.radio().sendPacket(packet)) {
        linkSession_.sendExchangeAck(0, 3);
        return;
    }

    awaitingTxAck_ = true;
    pendingAckChunk_ = 0;
    role_ = Role::Sending;
    dirty_ = true;
    notice_.emit("File transfer started", ui::Tone::Focus);
}

void FileTransfer::sendChunk(const proto::FileExchangeDataMessage &msg)
{
    const lora::Contact *contact = radioSession_.selectedContact();
    if (contact == nullptr) {
        linkSession_.sendExchangeAck(msg.chunkIndex, 1);
        return;
    }

    uint8_t encryptedChunk[256];
    if (msg.length > sizeof(encryptedChunk)) {
        linkSession_.sendExchangeAck(msg.chunkIndex, 2);
        return;
    }

    lora::Crypto::encryptFileChunk(currentFileKey_, msg.chunkIndex,
                                   msg.chunkData, encryptedChunk, msg.length);

    lora::PacketBuffer packet;
    const auto route = lora::PacketRoute::toContact(
        radioSession_.identity(), *contact, radioSession_.nextSequence());
    const lora::FileChunkView chunk{msg.chunkIndex, encryptedChunk, msg.length};
    if (!lora::Packet::encodeFileData(route, chunk, packet)) {
        linkSession_.sendExchangeAck(msg.chunkIndex, 2);
        return;
    }

    if (!radioSession_.radio().sendPacket(packet)) {
        linkSession_.sendExchangeAck(msg.chunkIndex, 3);
        return;
    }

    bytesTransferred_ += msg.length;
    awaitingTxAck_ = true;
    pendingAckChunk_ = msg.chunkIndex;
    dirty_ = true;

    if (fileSize_ > 0) {
        int percent = static_cast<int>((bytesTransferred_ * 100ULL) / fileSize_);
        if (percent > 100) {
            percent = 100;
        }
        char status[64];
        std::snprintf(status, sizeof(status), "Sending %s (%d%%)", filename_, percent);
        notice_.emit(status, ui::Tone::Focus);
    }
}

void FileTransfer::finishOutgoing()
{
    const lora::Contact *contact = radioSession_.selectedContact();
    if (contact == nullptr) {
        return;
    }

    lora::PacketBuffer packet;
    const auto route = lora::PacketRoute::toContact(
        radioSession_.identity(), *contact, radioSession_.nextSequence());
    if (!lora::Packet::encodeControl(lora::PacketType::FileExchangeEnd, route, packet)) {
        return;
    }

    if (!radioSession_.radio().sendPacket(packet)) {
        return;
    }

    role_ = Role::None;
    std::memset(currentFileKey_, 0, sizeof(currentFileKey_));
    dirty_ = true;
    notice_.emit("File transfer complete", ui::Tone::Success);
}

void FileTransfer::handleIncomingStart(const lora::DecodedPacket &packet)
{
    const lora::Contact *contact = radioSession_.contacts().findByDeviceId(packet.header.senderDeviceId);
    if (contact == nullptr) {
        return;
    }

    lora::FileStartInfo info{};
    const lora::PeerSession peer{radioSession_.identity(), *contact};
    if (!lora::Packet::decodeFileStart(peer, packet, info)) {
        return;
    }

    std::memcpy(currentFileKey_, info.fileKey, sizeof(currentFileKey_));
    linkSession_.sendFileExchangeStart(info.filename, info.fileSize);
    std::strncpy(filename_, info.filename, sizeof(filename_) - 1);
    filename_[sizeof(filename_) - 1] = '\0';
    fileSize_ = info.fileSize;
    bytesTransferred_ = 0;
    role_ = Role::Receiving;
    dirty_ = true;
    notice_.emit("Receiving file...", ui::Tone::Focus);
}

void FileTransfer::handleIncomingData(const lora::DecodedPacket &packet)
{
    lora::FileChunkView chunk{};
    if (!lora::Packet::decodeFileData(packet, chunk)) {
        return;
    }

    uint8_t decryptedChunk[256];
    if (chunk.length <= sizeof(decryptedChunk)) {
        lora::Crypto::decryptFileChunk(currentFileKey_, chunk.chunkIndex,
                                       chunk.data, decryptedChunk, chunk.length);
        linkSession_.sendFileExchangeData(chunk.chunkIndex, decryptedChunk, chunk.length);
    }

    bytesTransferred_ += chunk.length;
    dirty_ = true;
    notice_.emit("Receiving file chunks...", ui::Tone::Focus);
}

void FileTransfer::handleIncomingEnd()
{
    linkSession_.sendFileExchangeEnd();
    role_ = Role::None;
    std::memset(currentFileKey_, 0, sizeof(currentFileKey_));
    dirty_ = true;
    notice_.emit("File received (100%)", ui::Tone::Success);
}
