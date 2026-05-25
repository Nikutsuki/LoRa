#pragma once

#include <cstddef>
#include <cstdint>

#include "CryptoTypes.hpp"

namespace crypto_chat {

class CryptoBox {
public:
    static void fingerprint(uint8_t out[FingerprintSize],
                            const uint8_t deviceId[DeviceIdSize],
                            const uint8_t signingPublicKey[PublicKeySize],
                            const uint8_t boxPublicKey[PublicKeySize]);

    static void hash32(uint8_t out[32], const uint8_t *data, size_t size);

    static void deriveDevelopmentSeed(uint8_t out[32], const uint8_t deviceId[DeviceIdSize]);

    static void makeIdentity(DeviceIdentity &identity,
                             const uint8_t deviceId[DeviceIdSize],
                             const char *nickname,
                             const uint8_t seed[32],
                             bool developmentIdentity);

    static void makeNonce(uint8_t nonce[NonceSize],
                          const uint8_t senderDeviceId[DeviceIdSize],
                          const uint8_t recipientDeviceId[DeviceIdSize],
                          uint32_t sequence);

    static bool encryptForContact(const DeviceIdentity &identity,
                                  const Contact &contact,
                                  const uint8_t nonce[NonceSize],
                                  const uint8_t *additionalData,
                                  size_t additionalDataLength,
                                  const uint8_t *plainText,
                                  size_t plainTextLength,
                                  uint8_t *cipherText,
                                  uint8_t mac[MacSize]);

    static bool decryptFromContact(const DeviceIdentity &identity,
                                   const Contact &contact,
                                   const uint8_t nonce[NonceSize],
                                   const uint8_t *additionalData,
                                   size_t additionalDataLength,
                                   const uint8_t *cipherText,
                                   size_t cipherTextLength,
                                   const uint8_t mac[MacSize],
                                   uint8_t *plainText);

    static bool signInvite(const DeviceIdentity &identity,
                           PacketType type,
                           uint8_t signature[SignatureSize]);

    static bool verifyInvite(const Contact &contact,
                             PacketType type,
                             const uint8_t signature[SignatureSize]);

private:
    static void deriveSharedKey(uint8_t out[32],
                                const uint8_t ownSecretKey[SecretKeySize],
                                const uint8_t ownPublicKey[PublicKeySize],
                                const uint8_t peerPublicKey[PublicKeySize]);

    static size_t inviteMessage(uint8_t *out, const Contact &contact, PacketType type);
};

}
