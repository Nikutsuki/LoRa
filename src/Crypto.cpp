#include "Crypto.hpp"

#include <cstring>

#include "monocypher.h"
#include "pico/stdlib.h"

namespace lora {
namespace {

void copyNickname(char out[NicknameSize], const char *nickname)
{
    std::memset(out, 0, NicknameSize);
    if (nickname == nullptr) {
        return;
    }
    std::strncpy(out, nickname, NicknameSize - 1);
}

void append(uint8_t *&cursor, const uint8_t *data, size_t size)
{
    std::memcpy(cursor, data, size);
    cursor += size;
}

}

void Crypto::fingerprint(uint8_t out[FingerprintSize],
                            const uint8_t deviceId[DeviceIdSize],
                            const uint8_t signingPublicKey[PublicKeySize],
                            const uint8_t boxPublicKey[PublicKeySize])
{
    uint8_t material[DeviceIdSize + PublicKeySize + PublicKeySize];
    uint8_t *cursor = material;
    append(cursor, deviceId, DeviceIdSize);
    append(cursor, signingPublicKey, PublicKeySize);
    append(cursor, boxPublicKey, PublicKeySize);
    crypto_blake2b(out, FingerprintSize, material, sizeof(material));
}

void Crypto::hash32(uint8_t out[32], const uint8_t *data, size_t size)
{
    crypto_blake2b(out, 32, data, size);
}

void Crypto::deriveDevelopmentSeed(uint8_t out[32], const uint8_t deviceId[DeviceIdSize])
{
    uint8_t material[32]{};
    std::memcpy(material, "DEV-ONLY-IDENTITY", 17);
    std::memcpy(material + 17, deviceId, DeviceIdSize);
    uint64_t now = to_us_since_boot(get_absolute_time());
    std::memcpy(material + 25, &now, sizeof(now) < 7 ? sizeof(now) : 7);
    crypto_blake2b(out, 32, material, sizeof(material));
}

void Crypto::makeIdentity(DeviceIdentity &identity,
                             const uint8_t deviceId[DeviceIdSize],
                             const char *nickname,
                             const uint8_t seed[32],
                             bool developmentIdentity)
{
    std::memset(&identity, 0, sizeof(identity));
    std::memcpy(identity.deviceId, deviceId, DeviceIdSize);
    copyNickname(identity.nickname, nickname);

    uint8_t signSeed[32];
    uint8_t boxSeed[32];
    uint8_t material[40]{};
    std::memcpy(material, "sign", 4);
    std::memcpy(material + 8, seed, 32);
    crypto_blake2b(signSeed, sizeof(signSeed), material, sizeof(material));
    std::memcpy(material, "box", 3);
    crypto_blake2b(boxSeed, sizeof(boxSeed), material, sizeof(material));

    crypto_eddsa_key_pair(identity.signingSecretKey, identity.signingPublicKey, signSeed);
    std::memcpy(identity.boxSecretKey, boxSeed, SecretKeySize);
    crypto_x25519_public_key(identity.boxPublicKey, identity.boxSecretKey);
    fingerprint(identity.fingerprint, identity.deviceId, identity.signingPublicKey, identity.boxPublicKey);
    identity.developmentIdentity = developmentIdentity;

    crypto_wipe(signSeed, sizeof(signSeed));
    crypto_wipe(boxSeed, sizeof(boxSeed));
}

void Crypto::makeNonce(uint8_t nonce[NonceSize],
                          const uint8_t senderDeviceId[DeviceIdSize],
                          const uint8_t recipientDeviceId[DeviceIdSize],
                          uint32_t sequence)
{
    std::memset(nonce, 0, NonceSize);
    std::memcpy(nonce, senderDeviceId, DeviceIdSize);
    std::memcpy(nonce + DeviceIdSize, recipientDeviceId, DeviceIdSize);
    std::memcpy(nonce + 16, &sequence, sizeof(sequence));
}

bool Crypto::encryptForContact(const PeerSession &peer,
                               const AeadContext &ctx,
                               const uint8_t *plainText,
                               size_t plainTextLength,
                               uint8_t *cipherText,
                               uint8_t mac[MacSize])
{
    if (!peer.remote.valid || plainTextLength > MaxPlainTextSize) {
        return false;
    }
    uint8_t key[32];
    deriveSharedKey(key, peer.local.boxSecretKey, peer.local.boxPublicKey, peer.remote.boxPublicKey);
    crypto_aead_lock(cipherText, mac, key, ctx.nonce, ctx.aad, ctx.aadLength, plainText, plainTextLength);
    crypto_wipe(key, sizeof(key));
    return true;
}

bool Crypto::decryptFromContact(const PeerSession &peer,
                                const AeadContext &ctx,
                                const uint8_t *cipherText,
                                size_t cipherTextLength,
                                const uint8_t mac[MacSize],
                                uint8_t *plainText)
{
    if (!peer.remote.valid || cipherTextLength > MaxPlainTextSize) {
        return false;
    }
    uint8_t key[32];
    deriveSharedKey(key, peer.local.boxSecretKey, peer.local.boxPublicKey, peer.remote.boxPublicKey);
    int result = crypto_aead_unlock(plainText, mac, key, ctx.nonce, ctx.aad, ctx.aadLength, cipherText, cipherTextLength);
    crypto_wipe(key, sizeof(key));
    return result == 0;
}

bool Crypto::signInvite(const DeviceIdentity &identity, PacketType type, uint8_t signature[SignatureSize])
{
    Contact self{};
    self.valid = true;
    std::memcpy(self.deviceId, identity.deviceId, DeviceIdSize);
    std::memcpy(self.nickname, identity.nickname, NicknameSize);
    std::memcpy(self.signingPublicKey, identity.signingPublicKey, PublicKeySize);
    std::memcpy(self.boxPublicKey, identity.boxPublicKey, PublicKeySize);
    std::memcpy(self.fingerprint, identity.fingerprint, FingerprintSize);

    uint8_t message[1 + DeviceIdSize + NicknameSize + PublicKeySize + PublicKeySize];
    size_t size = inviteMessage(message, self, type);
    crypto_eddsa_sign(signature, identity.signingSecretKey, message, size);
    return true;
}

bool Crypto::verifyInvite(const Contact &contact, PacketType type, const uint8_t signature[SignatureSize])
{
    uint8_t message[1 + DeviceIdSize + NicknameSize + PublicKeySize + PublicKeySize];
    size_t size = inviteMessage(message, contact, type);
    return crypto_eddsa_check(signature, contact.signingPublicKey, message, size) == 0;
}

void Crypto::deriveSharedKey(uint8_t out[32],
                                const uint8_t ownSecretKey[SecretKeySize],
                                const uint8_t ownPublicKey[PublicKeySize],
                                const uint8_t peerPublicKey[PublicKeySize])
{
    uint8_t raw[32];
    uint8_t material[32 + PublicKeySize + PublicKeySize];
    crypto_x25519(raw, ownSecretKey, peerPublicKey);
    uint8_t *cursor = material;
    append(cursor, raw, sizeof(raw));
    if (std::memcmp(ownPublicKey, peerPublicKey, PublicKeySize) < 0) {
        append(cursor, ownPublicKey, PublicKeySize);
        append(cursor, peerPublicKey, PublicKeySize);
    } else {
        append(cursor, peerPublicKey, PublicKeySize);
        append(cursor, ownPublicKey, PublicKeySize);
    }
    crypto_blake2b(out, 32, material, sizeof(material));
    crypto_wipe(raw, sizeof(raw));
    crypto_wipe(material, sizeof(material));
}

size_t Crypto::inviteMessage(uint8_t *out, const Contact &contact, PacketType type)
{
    uint8_t *cursor = out;
    uint8_t typeByte = static_cast<uint8_t>(type);
    append(cursor, &typeByte, 1);
    append(cursor, contact.deviceId, DeviceIdSize);
    append(cursor, reinterpret_cast<const uint8_t *>(contact.nickname), NicknameSize);
    append(cursor, contact.signingPublicKey, PublicKeySize);
    append(cursor, contact.boxPublicKey, PublicKeySize);
    return static_cast<size_t>(cursor - out);
}

void Crypto::encryptFileChunk(const uint8_t key[32], uint32_t chunkIndex,
                                 const uint8_t *input, uint8_t *output, size_t length)
{
    uint8_t nonce[12]{};
    nonce[0] = static_cast<uint8_t>(chunkIndex & 0xFF);
    nonce[1] = static_cast<uint8_t>((chunkIndex >> 8) & 0xFF);
    nonce[2] = static_cast<uint8_t>((chunkIndex >> 16) & 0xFF);
    nonce[3] = static_cast<uint8_t>((chunkIndex >> 24) & 0xFF);

    crypto_chacha20_ietf(output, input, length, key, nonce, 0);
}

void Crypto::decryptFileChunk(const uint8_t key[32], uint32_t chunkIndex,
                                 const uint8_t *input, uint8_t *output, size_t length)
{
    encryptFileChunk(key, chunkIndex, input, output, length);
}

void Crypto::derivePasswordKey(uint8_t out[32], const char *password, size_t passwordLength)
{
    static const uint8_t Salt[16] = {
        'N', 'e', 'o', 'n', ' ', 'G', 'e', 'n', 'e', 's', 'i', 's', ' ', 'E', 'v', 'a'
    };

    // 16 KB work area is fast (<50ms) and lightweight on RP2040 (264KB RAM)
    static uint32_t workArea[4096]; // 16 KB

    crypto_argon2_config config;
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = 16;  // 16 KB
    config.nb_passes = 2;   // 2 passes
    config.nb_lanes = 1;

    crypto_argon2_inputs inputs;
    inputs.pass = reinterpret_cast<const uint8_t *>(password);
    inputs.pass_size = static_cast<uint32_t>(passwordLength);
    inputs.salt = Salt;
    inputs.salt_size = sizeof(Salt);

    crypto_argon2(out, 32, workArea, config, inputs, crypto_argon2_no_extras);
}

}
