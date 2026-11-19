#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto_chat {

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

}
