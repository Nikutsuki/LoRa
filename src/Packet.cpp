#include "Packet.hpp"

#include <cstring>

#include "Crypto.hpp"
#include "monocypher.h"

namespace lora {
namespace {

struct __attribute__((packed)) WireHeader {
    uint32_t magic;
    uint8_t version;
    uint8_t type;
    uint8_t senderDeviceId[DeviceIdSize];
    uint8_t recipientDeviceId[DeviceIdSize];
    uint32_t sequence;
    uint8_t payloadLength;
};

struct __attribute__((packed)) WireInvite {
    char nickname[NicknameSize];
    uint8_t signingPublicKey[PublicKeySize];
    uint8_t boxPublicKey[PublicKeySize];
    uint8_t signature[SignatureSize];
};

struct __attribute__((packed)) WireAead {
    uint8_t nonce[NonceSize];
    uint8_t mac[MacSize];
    uint8_t cipherText[];
};

struct __attribute__((packed)) WireMessage {
    char nickname[NicknameSize];
    uint8_t fingerprint[FingerprintSize];
    uint8_t nonce[NonceSize];
    uint8_t mac[MacSize];
    uint8_t cipherText[];
};

struct __attribute__((packed)) WireFileStart {
    uint8_t fileKey[FileKeySize];
    uint32_t fileSize;
    uint8_t nameLength;
    char filename[MaxFilenameSize];
};

struct __attribute__((packed)) WireFileData {
    uint32_t chunkIndex;
    uint8_t data[];
};

template <typename Payload>
struct __attribute__((packed)) WireFrame {
    WireHeader header;
    Payload payload;
};

constexpr size_t HeaderSize = sizeof(WireHeader);
constexpr size_t InvitePayloadSize = sizeof(WireInvite);
constexpr size_t EncryptedFixedPayloadSize = sizeof(WireMessage);
constexpr size_t FileStartFixedSize = offsetof(WireFileStart, filename);

static_assert(HeaderSize == 27, "WireHeader layout mismatch");

template <size_t N>
void copyBytes(uint8_t (&dst)[N], const uint8_t *src)
{
    std::memcpy(dst, src, N);
}

void initHeader(WireHeader &wire,
                uint32_t magic,
                uint8_t version,
                PacketType type,
                const PacketRoute &route,
                size_t payloadLength)
{
    wire.magic = magic;
    wire.version = version;
    wire.type = static_cast<uint8_t>(type);
    copyBytes(wire.senderDeviceId, route.senderDeviceId);
    copyBytes(wire.recipientDeviceId, route.recipientDeviceId);
    wire.sequence = route.sequence;
    wire.payloadLength = static_cast<uint8_t>(payloadLength);
}

WireHeader wireFromDecoded(const PacketHeader &header, uint32_t magic, uint8_t version)
{
    WireHeader wire{};
    initHeader(wire, magic, version, header.type,
               PacketRoute::toDevice(header.senderDeviceId, header.recipientDeviceId, header.sequence),
               header.payloadLength);
    return wire;
}

} // namespace

bool Packet::encodeInvite(const DeviceIdentity &identity,
                          const InviteParams &params,
                          PacketBuffer &out)
{
    if (params.type != PacketType::InviteBroadcast && params.type != PacketType::InviteAccept) {
        return false;
    }

    const bool encrypted = (params.pairingKey != nullptr);
    const size_t payloadLength = encrypted ? (sizeof(WireAead) + InvitePayloadSize) : InvitePayloadSize;
    if (out.capacity() < HeaderSize + payloadLength || payloadLength > MaxPacketSize) {
        return false;
    }

    const auto route = PacketRoute::toDevice(identity.deviceId, params.recipientDeviceId, params.sequence);

    WireInvite invite{};
    std::strncpy(invite.nickname, identity.nickname, NicknameSize - 1);
    copyBytes(invite.signingPublicKey, identity.signingPublicKey);
    copyBytes(invite.boxPublicKey, identity.boxPublicKey);
    Crypto::signInvite(identity, params.type, invite.signature);

    if (encrypted) {
        auto *frame = reinterpret_cast<WireFrame<WireAead> *>(out.data);
        initHeader(frame->header, Magic, Version, params.type, route, payloadLength);
        Crypto::makeNonce(frame->payload.nonce, identity.deviceId, params.recipientDeviceId, params.sequence);
        crypto_aead_lock(frame->payload.cipherText, frame->payload.mac, params.pairingKey,
                         frame->payload.nonce, reinterpret_cast<const uint8_t *>(&frame->header), HeaderSize,
                         reinterpret_cast<const uint8_t *>(&invite), InvitePayloadSize);
    } else {
        auto *frame = reinterpret_cast<WireFrame<WireInvite> *>(out.data);
        initHeader(frame->header, Magic, Version, params.type, route, payloadLength);
        frame->payload = invite;
    }

    out.length = HeaderSize + payloadLength;
    return true;
}

bool Packet::encodeMessage(const PeerSession &peer,
                           uint32_t sequence,
                           const char *message,
                           PacketBuffer &out)
{
    const size_t plainLength = (message != nullptr) ? std::strlen(message) : 0;
    if (!peer.remote.valid || plainLength == 0 || plainLength > MaxPlainTextSize) {
        return false;
    }

    const size_t payloadLength = EncryptedFixedPayloadSize + plainLength;
    if (payloadLength > MaxPacketSize || out.capacity() < HeaderSize + payloadLength) {
        return false;
    }

    auto *frame = reinterpret_cast<WireFrame<WireMessage> *>(out.data);
    initHeader(frame->header, Magic, Version, PacketType::EncryptedMessage,
               PacketRoute::toContact(peer.local, peer.remote, sequence), payloadLength);

    std::memset(frame->payload.nickname, 0, NicknameSize);
    std::strncpy(frame->payload.nickname, peer.local.nickname, NicknameSize - 1);
    copyBytes(frame->payload.fingerprint, peer.local.fingerprint);
    Crypto::makeNonce(frame->payload.nonce, peer.local.deviceId, peer.remote.deviceId, sequence);

    // AAD covers the wire header plus nickname, fingerprint, and nonce.
    constexpr size_t PrefixLength = HeaderSize + offsetof(WireMessage, mac);
    uint8_t mac[MacSize];
    if (!Crypto::encryptForContact(peer, {frame->payload.nonce, out.data, PrefixLength},
                                   reinterpret_cast<const uint8_t *>(message), plainLength,
                                   frame->payload.cipherText, mac)) {
        return false;
    }

    copyBytes(frame->payload.mac, mac);
    out.length = HeaderSize + payloadLength;
    return true;
}

bool Packet::encodeControl(PacketType type,
                           const PacketRoute &route,
                           PacketBuffer &out)
{
    if (out.capacity() < HeaderSize || route.senderDeviceId == nullptr || route.recipientDeviceId == nullptr) {
        return false;
    }

    auto *header = reinterpret_cast<WireHeader *>(out.data);
    initHeader(*header, Magic, Version, type, route, 0);
    out.length = HeaderSize;
    return true;
}

bool Packet::encodeFileStart(const PeerSession &peer,
                             uint32_t sequence,
                             const FileStartInfo &info,
                             PacketBuffer &out)
{
    size_t nameLen = std::strlen(info.filename);
    if (nameLen > MaxFilenameSize) {
        nameLen = MaxFilenameSize;
    }

    WireFileStart plain{};
    copyBytes(plain.fileKey, info.fileKey);
    plain.fileSize = info.fileSize;
    plain.nameLength = static_cast<uint8_t>(nameLen);
    std::memcpy(plain.filename, info.filename, nameLen);

    const size_t plainLen = FileStartFixedSize + nameLen;
    const size_t payloadLength = sizeof(WireAead) + plainLen;
    if (out.capacity() < HeaderSize + payloadLength || payloadLength > MaxPacketSize) {
        return false;
    }

    auto *frame = reinterpret_cast<WireFrame<WireAead> *>(out.data);
    initHeader(frame->header, Magic, Version, PacketType::FileExchangeStart,
               PacketRoute::toContact(peer.local, peer.remote, sequence), payloadLength);
    Crypto::makeNonce(frame->payload.nonce, peer.local.deviceId, peer.remote.deviceId, sequence);

    uint8_t mac[MacSize];
    if (!Crypto::encryptForContact(peer, {frame->payload.nonce, out.data, HeaderSize},
                                   reinterpret_cast<const uint8_t *>(&plain), plainLen,
                                   frame->payload.cipherText, mac)) {
        return false;
    }

    copyBytes(frame->payload.mac, mac);
    out.length = HeaderSize + payloadLength;
    return true;
}

bool Packet::encodeFileData(const PacketRoute &route,
                            const FileChunkView &chunk,
                            PacketBuffer &out)
{
    const size_t payloadLength = sizeof(WireFileData) + chunk.length;
    if (out.capacity() < HeaderSize + payloadLength || payloadLength > MaxPacketSize || chunk.data == nullptr) {
        return false;
    }

    auto *frame = reinterpret_cast<WireFrame<WireFileData> *>(out.data);
    initHeader(frame->header, Magic, Version, PacketType::FileExchangeData, route, payloadLength);
    frame->payload.chunkIndex = chunk.chunkIndex;
    std::memcpy(frame->payload.data, chunk.data, chunk.length);

    out.length = HeaderSize + payloadLength;
    return true;
}

bool Packet::decode(const uint8_t *data, size_t length, DecodedPacket &decoded)
{
    if (data == nullptr || length < HeaderSize) {
        return false;
    }

    const auto *wire = reinterpret_cast<const WireHeader *>(data);
    if (wire->magic != Magic || wire->version != Version) {
        return false;
    }
    if (wire->type < static_cast<uint8_t>(PacketType::InviteBroadcast) ||
        wire->type > static_cast<uint8_t>(PacketType::FileExchangeCancel)) {
        return false;
    }
    if (HeaderSize + wire->payloadLength != length) {
        return false;
    }

    decoded.header.type = static_cast<PacketType>(wire->type);
    copyBytes(decoded.header.senderDeviceId, wire->senderDeviceId);
    copyBytes(decoded.header.recipientDeviceId, wire->recipientDeviceId);
    decoded.header.sequence = wire->sequence;
    decoded.header.payloadLength = wire->payloadLength;
    decoded.payload = data + HeaderSize;
    decoded.payloadLength = wire->payloadLength;
    return true;
}

bool Packet::decodeInvite(const DecodedPacket &packet, const uint8_t *pairingKey, Contact &contact)
{
    if (packet.header.type != PacketType::InviteBroadcast && packet.header.type != PacketType::InviteAccept) {
        return false;
    }

    WireInvite invite{};
    if (packet.payloadLength == sizeof(WireAead) + InvitePayloadSize) {
        if (pairingKey == nullptr) {
            return false;
        }
        const auto *aead = reinterpret_cast<const WireAead *>(packet.payload);
        const WireHeader ad = wireFromDecoded(packet.header, Magic, Version);
        if (crypto_aead_unlock(reinterpret_cast<uint8_t *>(&invite), aead->mac, pairingKey, aead->nonce,
                               reinterpret_cast<const uint8_t *>(&ad), sizeof(ad),
                               aead->cipherText, InvitePayloadSize) != 0) {
            return false;
        }
    } else if (packet.payloadLength == InvitePayloadSize) {
        invite = *reinterpret_cast<const WireInvite *>(packet.payload);
    } else {
        return false;
    }

    contact = Contact{};
    contact.valid = true;
    copyBytes(contact.deviceId, packet.header.senderDeviceId);
    std::strncpy(contact.nickname, invite.nickname, NicknameSize - 1);
    copyBytes(contact.signingPublicKey, invite.signingPublicKey);
    copyBytes(contact.boxPublicKey, invite.boxPublicKey);
    Crypto::fingerprint(contact.fingerprint, contact.deviceId, contact.signingPublicKey, contact.boxPublicKey);
    return Crypto::verifyInvite(contact, packet.header.type, invite.signature);
}

bool Packet::decodeMessage(const PeerSession &peer,
                           const DecodedPacket &packet,
                           char *outMessage,
                           size_t outCapacity)
{
    if (packet.header.type != PacketType::EncryptedMessage ||
        packet.payloadLength <= EncryptedFixedPayloadSize ||
        outCapacity == 0) {
        return false;
    }

    const size_t cipherLength = packet.payloadLength - EncryptedFixedPayloadSize;
    if (cipherLength >= outCapacity) {
        return false;
    }

    const auto *msg = reinterpret_cast<const WireMessage *>(packet.payload);
    if (std::memcmp(msg->fingerprint, peer.remote.fingerprint, FingerprintSize) != 0) {
        return false;
    }

    constexpr size_t PrefixLength = HeaderSize + offsetof(WireMessage, mac);
    uint8_t ad[PrefixLength];
    const WireHeader wireHeader = wireFromDecoded(packet.header, Magic, Version);
    std::memcpy(ad, &wireHeader, HeaderSize);
    std::memcpy(ad + HeaderSize, packet.payload, offsetof(WireMessage, mac));

    if (!Crypto::decryptFromContact(peer, {msg->nonce, ad, PrefixLength},
                                    msg->cipherText, cipherLength, msg->mac,
                                    reinterpret_cast<uint8_t *>(outMessage))) {
        return false;
    }
    outMessage[cipherLength] = '\0';
    return true;
}

bool Packet::decodeFileStart(const PeerSession &peer,
                             const DecodedPacket &packet,
                             FileStartInfo &outInfo)
{
    if (packet.payloadLength < sizeof(WireAead) + FileStartFixedSize) {
        return false;
    }

    const auto *aead = reinterpret_cast<const WireAead *>(packet.payload);
    const size_t cipherLen = packet.payloadLength - sizeof(WireAead);
    const WireHeader ad = wireFromDecoded(packet.header, Magic, Version);

    WireFileStart plain{};
    if (!Crypto::decryptFromContact(peer, {aead->nonce, reinterpret_cast<const uint8_t *>(&ad), sizeof(ad)},
                                    aead->cipherText, cipherLen, aead->mac,
                                    reinterpret_cast<uint8_t *>(&plain))) {
        return false;
    }

    outInfo = FileStartInfo{};
    copyBytes(outInfo.fileKey, plain.fileKey);
    outInfo.fileSize = plain.fileSize;
    const size_t nameLen = (plain.nameLength > MaxFilenameSize) ? MaxFilenameSize : plain.nameLength;
    std::memcpy(outInfo.filename, plain.filename, nameLen);
    outInfo.filename[nameLen] = '\0';
    return true;
}

bool Packet::decodeFileData(const DecodedPacket &packet, FileChunkView &outChunk)
{
    if (packet.payloadLength < sizeof(WireFileData)) {
        return false;
    }
    const auto *data = reinterpret_cast<const WireFileData *>(packet.payload);
    outChunk.chunkIndex = data->chunkIndex;
    outChunk.data = data->data;
    outChunk.length = packet.payloadLength - sizeof(WireFileData);
    return true;
}

bool Packet::isBroadcast(const uint8_t deviceId[DeviceIdSize])
{
    return std::memcmp(deviceId, BroadcastDeviceId, DeviceIdSize) == 0;
}

bool Packet::isForDevice(const DecodedPacket &packet, const uint8_t deviceId[DeviceIdSize])
{
    return isBroadcast(packet.header.recipientDeviceId) ||
           std::memcmp(packet.header.recipientDeviceId, deviceId, DeviceIdSize) == 0;
}

} // namespace lora
