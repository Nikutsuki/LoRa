#pragma once

#include <cstddef>
#include <cstdint>

namespace lora {

constexpr size_t DeviceIdSize = 8;
constexpr size_t NicknameSize = 16;
constexpr size_t PublicKeySize = 32;
constexpr size_t SecretKeySize = 32;
constexpr size_t SigningSecretKeySize = 64;
constexpr size_t SignatureSize = 64;
constexpr size_t FingerprintSize = 8;
constexpr size_t NonceSize = 24;
constexpr size_t MacSize = 16;
constexpr size_t MaxContacts = 8;
constexpr size_t MaxPacketSize = 255;
constexpr size_t MaxPlainTextSize = 96;
constexpr size_t MaxFilenameSize = 64;
constexpr size_t FileKeySize = 32;

constexpr uint8_t BroadcastDeviceId[DeviceIdSize] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
};

struct DeviceIdentity {
    uint8_t deviceId[DeviceIdSize]{};
    char nickname[NicknameSize]{};
    uint8_t signingSecretKey[SigningSecretKeySize]{};
    uint8_t signingPublicKey[PublicKeySize]{};
    uint8_t boxSecretKey[SecretKeySize]{};
    uint8_t boxPublicKey[PublicKeySize]{};
    uint8_t fingerprint[FingerprintSize]{};
    bool developmentIdentity = false;
};

struct Contact {
    bool valid = false;
    uint8_t deviceId[DeviceIdSize]{};
    char nickname[NicknameSize]{};
    uint8_t signingPublicKey[PublicKeySize]{};
    uint8_t boxPublicKey[PublicKeySize]{};
    uint8_t fingerprint[FingerprintSize]{};
};

enum class PacketType : uint8_t {
    InviteBroadcast = 1,
    InviteAccept = 2,
    EncryptedMessage = 3,
    MessageAck = 4,
    FileExchangeStart = 5,
    FileExchangeData = 6,
    FileExchangeEnd = 8,
    FileExchangeCancel = 9,
};

struct PacketHeader {
    PacketType type = PacketType::InviteBroadcast;
    uint8_t senderDeviceId[DeviceIdSize]{};
    uint8_t recipientDeviceId[DeviceIdSize]{};
    uint32_t sequence = 0;
    uint8_t payloadLength = 0;
};

struct DecodedPacket {
    PacketHeader header{};
    const uint8_t *payload = nullptr;
    size_t payloadLength = 0;
};

struct PacketBuffer {
    uint8_t data[MaxPacketSize]{};
    size_t length = 0;

    size_t capacity() const { return sizeof(data); }
};

struct PacketRoute {
    const uint8_t *senderDeviceId = nullptr;
    const uint8_t *recipientDeviceId = nullptr;
    uint32_t sequence = 0;

    static PacketRoute toBroadcast(const DeviceIdentity &sender, uint32_t sequence)
    {
        return {sender.deviceId, BroadcastDeviceId, sequence};
    }

    static PacketRoute toContact(const DeviceIdentity &sender, const Contact &recipient, uint32_t sequence)
    {
        return {sender.deviceId, recipient.deviceId, sequence};
    }

    static PacketRoute toDevice(const uint8_t sender[DeviceIdSize],
                                const uint8_t recipient[DeviceIdSize],
                                uint32_t sequence)
    {
        return {sender, recipient, sequence};
    }
};

struct PeerSession {
    const DeviceIdentity &local;
    const Contact &remote;
};

struct AeadContext {
    const uint8_t *nonce = nullptr;
    const uint8_t *aad = nullptr;
    size_t aadLength = 0;
};

struct InviteParams {
    PacketType type = PacketType::InviteBroadcast;
    const uint8_t *recipientDeviceId = BroadcastDeviceId;
    uint32_t sequence = 0;
    const uint8_t *pairingKey = nullptr;

    static InviteParams broadcast(uint32_t sequence, const uint8_t *pairingKey = nullptr)
    {
        return {PacketType::InviteBroadcast, BroadcastDeviceId, sequence, pairingKey};
    }

    static InviteParams accept(const uint8_t recipient[DeviceIdSize],
                               uint32_t sequence,
                               const uint8_t *pairingKey = nullptr)
    {
        return {PacketType::InviteAccept, recipient, sequence, pairingKey};
    }
};

struct FileStartInfo {
    uint8_t fileKey[FileKeySize]{};
    char filename[MaxFilenameSize + 1]{};
    uint32_t fileSize = 0;
};

struct FileChunkView {
    uint32_t chunkIndex = 0;
    const uint8_t *data = nullptr;
    size_t length = 0;
};

} // namespace lora
