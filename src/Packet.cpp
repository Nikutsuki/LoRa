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

struct __attribute__((packed)) WireAeadHeader {
    uint8_t nonce[NonceSize];
    uint8_t mac[MacSize];
};

struct __attribute__((packed)) WireMessageHeader {
    char nickname[NicknameSize];
    uint8_t fingerprint[FingerprintSize];
    uint8_t nonce[NonceSize];
    uint8_t mac[MacSize];
};

struct __attribute__((packed)) WireFileStart {
    uint8_t fileKey[FileKeySize];
    uint32_t fileSize;
    uint8_t nameLength;
    char filename[MaxFilenameSize];
};

struct __attribute__((packed)) WireFileDataHeader {
    uint32_t chunkIndex;
};

constexpr size_t HeaderSize = sizeof(WireHeader);
constexpr size_t InvitePayloadSize = sizeof(WireInvite);
constexpr size_t EncryptedFixedPayloadSize = sizeof(WireMessageHeader);
constexpr size_t FileStartFixedSize = offsetof(WireFileStart, filename);

static_assert(HeaderSize == 27, "WireHeader layout mismatch");

} // namespace

bool Packet::encodeInvite(const DeviceIdentity &identity,
                          const InviteParams &params,
                          PacketBuffer &out)
{
    if (params.type != PacketType::InviteBroadcast && params.type != PacketType::InviteAccept) {
        return false;
    }

    const bool encrypted = (params.pairingKey != nullptr);
    const size_t totalPayloadSize = encrypted
                                        ? (sizeof(WireAeadHeader) + InvitePayloadSize)
                                        : InvitePayloadSize;
    if (out.capacity() < HeaderSize + totalPayloadSize || totalPayloadSize > MaxPacketSize) {
        return false;
    }

    const PacketRoute route = PacketRoute::toDevice(identity.deviceId, params.recipientDeviceId, params.sequence);
    const PacketHeader header = makeHeader(params.type, route, static_cast<uint8_t>(totalPayloadSize));
    writeHeader(header, out.data);

    WireInvite rawInvite{};
    std::strncpy(rawInvite.nickname, identity.nickname, NicknameSize - 1);
    std::memcpy(rawInvite.signingPublicKey, identity.signingPublicKey, PublicKeySize);
    std::memcpy(rawInvite.boxPublicKey, identity.boxPublicKey, PublicKeySize);
    Crypto::signInvite(identity, params.type, rawInvite.signature);

    uint8_t *payload = out.data + HeaderSize;
    if (encrypted) {
        WireAeadHeader aead{};
        Crypto::makeNonce(aead.nonce, identity.deviceId, params.recipientDeviceId, params.sequence);
        uint8_t *cipherText = payload + sizeof(WireAeadHeader);
        crypto_aead_lock(cipherText, aead.mac, params.pairingKey, aead.nonce,
                         out.data, HeaderSize,
                         reinterpret_cast<const uint8_t *>(&rawInvite), InvitePayloadSize);
        std::memcpy(payload, &aead, sizeof(aead));
    } else {
        std::memcpy(payload, &rawInvite, InvitePayloadSize);
    }

    out.length = HeaderSize + totalPayloadSize;
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

    const PacketRoute route = PacketRoute::toContact(peer.local, peer.remote, sequence);
    const PacketHeader header = makeHeader(PacketType::EncryptedMessage, route, static_cast<uint8_t>(payloadLength));
    writeHeader(header, out.data);

    WireMessageHeader msgHeader{};
    std::strncpy(msgHeader.nickname, peer.local.nickname, NicknameSize - 1);
    std::memcpy(msgHeader.fingerprint, peer.local.fingerprint, FingerprintSize);
    Crypto::makeNonce(msgHeader.nonce, peer.local.deviceId, peer.remote.deviceId, sequence);

    uint8_t *payload = out.data + HeaderSize;
    std::memcpy(payload, &msgHeader, sizeof(msgHeader));

    // AAD covers the wire header plus nickname, fingerprint, and nonce.
    constexpr size_t PrefixLength = HeaderSize + offsetof(WireMessageHeader, mac);
    uint8_t *cipherText = payload + sizeof(msgHeader);
    uint8_t mac[MacSize];
    if (!Crypto::encryptForContact(peer, {msgHeader.nonce, out.data, PrefixLength},
                                   reinterpret_cast<const uint8_t *>(message), plainLength,
                                   cipherText, mac)) {
        return false;
    }

    std::memcpy(payload + offsetof(WireMessageHeader, mac), mac, MacSize);
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

    writeHeader(makeHeader(type, route, 0), out.data);
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
    std::memcpy(plain.fileKey, info.fileKey, FileKeySize);
    plain.fileSize = info.fileSize;
    plain.nameLength = static_cast<uint8_t>(nameLen);
    std::memcpy(plain.filename, info.filename, nameLen);

    const size_t plainLen = FileStartFixedSize + nameLen;
    const size_t payloadLength = sizeof(WireAeadHeader) + plainLen;
    if (out.capacity() < HeaderSize + payloadLength || payloadLength > MaxPacketSize) {
        return false;
    }

    const PacketRoute route = PacketRoute::toContact(peer.local, peer.remote, sequence);
    writeHeader(makeHeader(PacketType::FileExchangeStart, route, static_cast<uint8_t>(payloadLength)), out.data);

    WireAeadHeader aead{};
    Crypto::makeNonce(aead.nonce, peer.local.deviceId, peer.remote.deviceId, sequence);

    uint8_t *payload = out.data + HeaderSize;
    uint8_t *cipherText = payload + sizeof(WireAeadHeader);
    if (!Crypto::encryptForContact(peer, {aead.nonce, out.data, HeaderSize},
                                   reinterpret_cast<const uint8_t *>(&plain), plainLen,
                                   cipherText, aead.mac)) {
        return false;
    }

    std::memcpy(payload, &aead, sizeof(aead));
    out.length = HeaderSize + payloadLength;
    return true;
}

bool Packet::encodeFileData(const PacketRoute &route,
                            const FileChunkView &chunk,
                            PacketBuffer &out)
{
    const size_t payloadLength = sizeof(WireFileDataHeader) + chunk.length;
    if (out.capacity() < HeaderSize + payloadLength || payloadLength > MaxPacketSize || chunk.data == nullptr) {
        return false;
    }

    writeHeader(makeHeader(PacketType::FileExchangeData, route, static_cast<uint8_t>(payloadLength)), out.data);

    const WireFileDataHeader dataHeader{chunk.chunkIndex};
    uint8_t *payload = out.data + HeaderSize;
    std::memcpy(payload, &dataHeader, sizeof(dataHeader));
    std::memcpy(payload + sizeof(dataHeader), chunk.data, chunk.length);

    out.length = HeaderSize + payloadLength;
    return true;
}

bool Packet::decode(const uint8_t *data, size_t length, DecodedPacket &decoded)
{
    if (data == nullptr || length < HeaderSize) {
        return false;
    }
    if (!readHeader(data, length, decoded.header)) {
        return false;
    }
    if (HeaderSize + decoded.header.payloadLength != length) {
        return false;
    }
    decoded.payload = data + HeaderSize;
    decoded.payloadLength = decoded.header.payloadLength;
    return true;
}

bool Packet::decodeInvite(const DecodedPacket &packet, const uint8_t *pairingKey, Contact &contact)
{
    if (packet.header.type != PacketType::InviteBroadcast && packet.header.type != PacketType::InviteAccept) {
        return false;
    }

    WireInvite rawInvite{};

    if (packet.payloadLength == sizeof(WireAeadHeader) + InvitePayloadSize) {
        if (pairingKey == nullptr) {
            return false;
        }

        WireAeadHeader aead{};
        std::memcpy(&aead, packet.payload, sizeof(aead));
        const uint8_t *cipherText = packet.payload + sizeof(aead);

        uint8_t ad[HeaderSize];
        writeHeader(packet.header, ad);

        if (crypto_aead_unlock(reinterpret_cast<uint8_t *>(&rawInvite), aead.mac, pairingKey, aead.nonce,
                               ad, HeaderSize, cipherText, InvitePayloadSize) != 0) {
            return false;
        }
    } else if (packet.payloadLength == InvitePayloadSize) {
        std::memcpy(&rawInvite, packet.payload, InvitePayloadSize);
    } else {
        return false;
    }

    contact = Contact{};
    contact.valid = true;
    std::memcpy(contact.deviceId, packet.header.senderDeviceId, DeviceIdSize);
    std::strncpy(contact.nickname, rawInvite.nickname, NicknameSize - 1);
    std::memcpy(contact.signingPublicKey, rawInvite.signingPublicKey, PublicKeySize);
    std::memcpy(contact.boxPublicKey, rawInvite.boxPublicKey, PublicKeySize);
    Crypto::fingerprint(contact.fingerprint, contact.deviceId, contact.signingPublicKey, contact.boxPublicKey);
    return Crypto::verifyInvite(contact, packet.header.type, rawInvite.signature);
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

    WireMessageHeader msgHeader{};
    std::memcpy(&msgHeader, packet.payload, sizeof(msgHeader));

    if (std::memcmp(msgHeader.fingerprint, peer.remote.fingerprint, FingerprintSize) != 0) {
        return false;
    }

    constexpr size_t PrefixLength = HeaderSize + offsetof(WireMessageHeader, mac);
    uint8_t ad[PrefixLength];
    writeHeader(packet.header, ad);
    std::memcpy(ad + HeaderSize, packet.payload, offsetof(WireMessageHeader, mac));

    const uint8_t *cipherText = packet.payload + sizeof(msgHeader);
    if (!Crypto::decryptFromContact(peer, {msgHeader.nonce, ad, PrefixLength},
                                    cipherText, cipherLength, msgHeader.mac,
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
    if (packet.payloadLength < sizeof(WireAeadHeader) + FileStartFixedSize) {
        return false;
    }

    WireAeadHeader aead{};
    std::memcpy(&aead, packet.payload, sizeof(aead));
    const uint8_t *cipherText = packet.payload + sizeof(aead);
    const size_t cipherLen = packet.payloadLength - sizeof(aead);

    uint8_t ad[HeaderSize];
    writeHeader(packet.header, ad);

    WireFileStart plain{};
    if (!Crypto::decryptFromContact(peer, {aead.nonce, ad, HeaderSize},
                                    cipherText, cipherLen, aead.mac,
                                    reinterpret_cast<uint8_t *>(&plain))) {
        return false;
    }

    outInfo = FileStartInfo{};
    std::memcpy(outInfo.fileKey, plain.fileKey, FileKeySize);
    outInfo.fileSize = plain.fileSize;
    const size_t nameLen = (plain.nameLength > MaxFilenameSize) ? MaxFilenameSize : plain.nameLength;
    std::memcpy(outInfo.filename, plain.filename, nameLen);
    outInfo.filename[nameLen] = '\0';
    return true;
}

bool Packet::decodeFileData(const DecodedPacket &packet, FileChunkView &outChunk)
{
    if (packet.payloadLength < sizeof(WireFileDataHeader)) {
        return false;
    }
    WireFileDataHeader dataHeader{};
    std::memcpy(&dataHeader, packet.payload, sizeof(dataHeader));
    outChunk.chunkIndex = dataHeader.chunkIndex;
    outChunk.data = packet.payload + sizeof(dataHeader);
    outChunk.length = packet.payloadLength - sizeof(dataHeader);
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

PacketHeader Packet::makeHeader(PacketType type,
                                const PacketRoute &route,
                                uint8_t payloadLength)
{
    PacketHeader header{};
    header.type = type;
    std::memcpy(header.senderDeviceId, route.senderDeviceId, DeviceIdSize);
    std::memcpy(header.recipientDeviceId, route.recipientDeviceId, DeviceIdSize);
    header.sequence = route.sequence;
    header.payloadLength = payloadLength;
    return header;
}

void Packet::writeHeader(const PacketHeader &header, uint8_t *out)
{
    WireHeader wire{};
    wire.magic = Magic;
    wire.version = Version;
    wire.type = static_cast<uint8_t>(header.type);
    std::memcpy(wire.senderDeviceId, header.senderDeviceId, DeviceIdSize);
    std::memcpy(wire.recipientDeviceId, header.recipientDeviceId, DeviceIdSize);
    wire.sequence = header.sequence;
    wire.payloadLength = header.payloadLength;
    std::memcpy(out, &wire, sizeof(wire));
}

bool Packet::readHeader(const uint8_t *data, size_t size, PacketHeader &header)
{
    if (size < HeaderSize) {
        return false;
    }
    WireHeader wire{};
    std::memcpy(&wire, data, sizeof(wire));
    if (wire.magic != Magic || wire.version != Version) {
        return false;
    }
    if (wire.type < static_cast<uint8_t>(PacketType::InviteBroadcast) ||
        wire.type > static_cast<uint8_t>(PacketType::FileExchangeCancel)) {
        return false;
    }
    header.type = static_cast<PacketType>(wire.type);
    std::memcpy(header.senderDeviceId, wire.senderDeviceId, DeviceIdSize);
    std::memcpy(header.recipientDeviceId, wire.recipientDeviceId, DeviceIdSize);
    header.sequence = wire.sequence;
    header.payloadLength = wire.payloadLength;
    return true;
}

} // namespace lora
