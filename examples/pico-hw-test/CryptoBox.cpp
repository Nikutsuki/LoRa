#include "CryptoBox.hpp"

#include <cstring>

#include "monocypher.h"
#include "pico/stdlib.h"

namespace crypto_chat {
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

void CryptoBox::fingerprint(uint8_t out[FingerprintSize],
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

void CryptoBox::hash32(uint8_t out[32], const uint8_t *data, size_t size)
{
    crypto_blake2b(out, 32, data, size);
}

void CryptoBox::deriveDevelopmentSeed(uint8_t out[32], const uint8_t deviceId[DeviceIdSize])
{
    uint8_t material[32]{};
    std::memcpy(material, "DEV-ONLY-IDENTITY", 17);
    std::memcpy(material + 17, deviceId, DeviceIdSize);
    uint64_t now = to_us_since_boot(get_absolute_time());
    std::memcpy(material + 25, &now, sizeof(now) < 7 ? sizeof(now) : 7);
    crypto_blake2b(out, 32, material, sizeof(material));
}

void CryptoBox::makeIdentity(DeviceIdentity &identity,
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

void CryptoBox::makeNonce(uint8_t nonce[NonceSize],
                          const uint8_t senderDeviceId[DeviceIdSize],
                          const uint8_t recipientDeviceId[DeviceIdSize],
                          uint32_t sequence)
{
    std::memset(nonce, 0, NonceSize);
    std::memcpy(nonce, senderDeviceId, DeviceIdSize);
    std::memcpy(nonce + DeviceIdSize, recipientDeviceId, DeviceIdSize);
    std::memcpy(nonce + 16, &sequence, sizeof(sequence));
}

bool CryptoBox::encryptForContact(const DeviceIdentity &identity,
                                  const Contact &contact,
                                  const uint8_t nonce[NonceSize],
                                  const uint8_t *additionalData,
                                  size_t additionalDataLength,
                                  const uint8_t *plainText,
                                  size_t plainTextLength,
                                  uint8_t *cipherText,
                                  uint8_t mac[MacSize])
{
    if (!contact.valid || plainTextLength > MaxPlainTextSize) {
        return false;
    }
    uint8_t key[32];
    deriveSharedKey(key, identity.boxSecretKey, identity.boxPublicKey, contact.boxPublicKey);
    crypto_aead_lock(cipherText, mac, key, nonce, additionalData, additionalDataLength, plainText, plainTextLength);
    crypto_wipe(key, sizeof(key));
    return true;
}

bool CryptoBox::decryptFromContact(const DeviceIdentity &identity,
                                   const Contact &contact,
                                   const uint8_t nonce[NonceSize],
                                   const uint8_t *additionalData,
                                   size_t additionalDataLength,
                                   const uint8_t *cipherText,
                                   size_t cipherTextLength,
                                   const uint8_t mac[MacSize],
                                   uint8_t *plainText)
{
    if (!contact.valid || cipherTextLength > MaxPlainTextSize) {
        return false;
    }
    uint8_t key[32];
    deriveSharedKey(key, identity.boxSecretKey, identity.boxPublicKey, contact.boxPublicKey);
    int result = crypto_aead_unlock(plainText, mac, key, nonce, additionalData, additionalDataLength, cipherText, cipherTextLength);
    crypto_wipe(key, sizeof(key));
    return result == 0;
}

bool CryptoBox::signInvite(const DeviceIdentity &identity, PacketType type, uint8_t signature[SignatureSize])
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

bool CryptoBox::verifyInvite(const Contact &contact, PacketType type, const uint8_t signature[SignatureSize])
{
    uint8_t message[1 + DeviceIdSize + NicknameSize + PublicKeySize + PublicKeySize];
    size_t size = inviteMessage(message, contact, type);
    return crypto_eddsa_check(signature, contact.signingPublicKey, message, size) == 0;
}

void CryptoBox::deriveSharedKey(uint8_t out[32],
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

size_t CryptoBox::inviteMessage(uint8_t *out, const Contact &contact, PacketType type)
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

}
