#include "Crypto.hpp"

#include <cstdio>
#include <cstring>

#include "monocypher.h"
#include "pico/stdlib.h"

namespace lora {
namespace {

template <size_t N>
void copyBytes(uint8_t (&dst)[N], const uint8_t *src)
{
    std::memcpy(dst, src, N);
}

struct __attribute__((packed)) FingerprintMaterial {
    uint8_t deviceId[DeviceIdSize];
    uint8_t signingPublicKey[PublicKeySize];
    uint8_t boxPublicKey[PublicKeySize];
};

struct __attribute__((packed)) DevSeedMaterial {
    char tag[17];
    uint8_t deviceId[DeviceIdSize];
    uint64_t timestamp;
};

struct __attribute__((packed)) KeySeedMaterial {
    char tag[8];
    uint8_t seed[FileKeySize];
};

struct __attribute__((packed)) NonceLayout {
    uint8_t senderDeviceId[DeviceIdSize];
    uint8_t recipientDeviceId[DeviceIdSize];
    uint32_t sequence;
    uint32_t zero;
};

struct __attribute__((packed)) InviteSigMaterial {
    uint8_t type;
    uint8_t deviceId[DeviceIdSize];
    char nickname[NicknameSize];
    uint8_t signingPublicKey[PublicKeySize];
    uint8_t boxPublicKey[PublicKeySize];
};

struct __attribute__((packed)) SharedKeyMaterial {
    uint8_t sharedSecret[FileKeySize];
    uint8_t firstPublicKey[PublicKeySize];
    uint8_t secondPublicKey[PublicKeySize];
};

constexpr size_t ChunkNonceSize = 12;
constexpr size_t Argon2WorkWords = 4096;
constexpr uint32_t Argon2Blocks = 16;
constexpr uint32_t Argon2Passes = 2;
constexpr uint32_t Argon2Lanes = 1;

struct __attribute__((packed)) ChunkNonce {
    uint32_t chunkIndex;
    uint8_t zero[ChunkNonceSize - sizeof(uint32_t)];
};

static_assert(sizeof(NonceLayout) == NonceSize, "NonceLayout size mismatch");
static_assert(sizeof(ChunkNonce) == ChunkNonceSize, "ChunkNonce size mismatch");

InviteSigMaterial makeInviteMaterial(PacketType type,
                                       const uint8_t deviceId[DeviceIdSize],
                                       const char nickname[NicknameSize],
                                       const uint8_t signingPublicKey[PublicKeySize],
                                       const uint8_t boxPublicKey[PublicKeySize])
{
    InviteSigMaterial material{};
    material.type = static_cast<uint8_t>(type);
    copyBytes(material.deviceId, deviceId);
    std::snprintf(material.nickname, sizeof(material.nickname), "%s", nickname);
    copyBytes(material.signingPublicKey, signingPublicKey);
    copyBytes(material.boxPublicKey, boxPublicKey);
    return material;
}

} // namespace

void Crypto::fingerprint(uint8_t out[FingerprintSize],
                         const uint8_t deviceId[DeviceIdSize],
                         const uint8_t signingPublicKey[PublicKeySize],
                         const uint8_t boxPublicKey[PublicKeySize])
{
    FingerprintMaterial material{};
    copyBytes(material.deviceId, deviceId);
    copyBytes(material.signingPublicKey, signingPublicKey);
    copyBytes(material.boxPublicKey, boxPublicKey);
    crypto_blake2b(out, FingerprintSize, reinterpret_cast<const uint8_t *>(&material), sizeof(material));
}

void Crypto::hash32(uint8_t out[FileKeySize], const uint8_t *data, size_t size)
{
    crypto_blake2b(out, FileKeySize, data, size);
}

void Crypto::deriveDevelopmentSeed(uint8_t out[FileKeySize], const uint8_t deviceId[DeviceIdSize])
{
    DevSeedMaterial material{};
    static constexpr char Tag[] = "DEV-ONLY-IDENTITY";
    std::memcpy(material.tag, Tag, sizeof(material.tag));
    copyBytes(material.deviceId, deviceId);
    material.timestamp = to_us_since_boot(get_absolute_time());
    crypto_blake2b(out, FileKeySize, reinterpret_cast<const uint8_t *>(&material), sizeof(material));
}

void Crypto::makeIdentity(DeviceIdentity &identity,
                          const uint8_t deviceId[DeviceIdSize],
                          const char *nickname,
                          const uint8_t seed[FileKeySize],
                          bool developmentIdentity)
{
    identity = DeviceIdentity{};
    copyBytes(identity.deviceId, deviceId);
    std::snprintf(identity.nickname, sizeof(identity.nickname), "%s", (nickname != nullptr) ? nickname : "");

    uint8_t signSeed[FileKeySize];
    uint8_t boxSeed[FileKeySize];
    KeySeedMaterial material{};
    copyBytes(material.seed, seed);

    std::snprintf(material.tag, sizeof(material.tag), "sign");
    crypto_blake2b(signSeed, sizeof(signSeed), reinterpret_cast<const uint8_t *>(&material), sizeof(material));

    std::snprintf(material.tag, sizeof(material.tag), "box");
    crypto_blake2b(boxSeed, sizeof(boxSeed), reinterpret_cast<const uint8_t *>(&material), sizeof(material));

    crypto_eddsa_key_pair(identity.signingSecretKey, identity.signingPublicKey, signSeed);
    copyBytes(identity.boxSecretKey, boxSeed);
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
    NonceLayout layout{};
    copyBytes(layout.senderDeviceId, senderDeviceId);
    copyBytes(layout.recipientDeviceId, recipientDeviceId);
    layout.sequence = sequence;
    std::memcpy(nonce, &layout, sizeof(layout));
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
    uint8_t key[FileKeySize];
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
    uint8_t key[FileKeySize];
    deriveSharedKey(key, peer.local.boxSecretKey, peer.local.boxPublicKey, peer.remote.boxPublicKey);
    const int result = crypto_aead_unlock(plainText, mac, key, ctx.nonce, ctx.aad, ctx.aadLength, cipherText, cipherTextLength);
    crypto_wipe(key, sizeof(key));
    return result == 0;
}

bool Crypto::signInvite(const DeviceIdentity &identity, PacketType type, uint8_t signature[SignatureSize])
{
    const InviteSigMaterial material = makeInviteMaterial(
        type, identity.deviceId, identity.nickname, identity.signingPublicKey, identity.boxPublicKey);
    crypto_eddsa_sign(signature, identity.signingSecretKey,
                      reinterpret_cast<const uint8_t *>(&material), sizeof(material));
    return true;
}

bool Crypto::verifyInvite(const Contact &contact, PacketType type, const uint8_t signature[SignatureSize])
{
    const InviteSigMaterial material = makeInviteMaterial(
        type, contact.deviceId, contact.nickname, contact.signingPublicKey, contact.boxPublicKey);
    return crypto_eddsa_check(signature, contact.signingPublicKey,
                              reinterpret_cast<const uint8_t *>(&material), sizeof(material)) == 0;
}

void Crypto::deriveSharedKey(uint8_t out[FileKeySize],
                             const uint8_t ownSecretKey[SecretKeySize],
                             const uint8_t ownPublicKey[PublicKeySize],
                             const uint8_t peerPublicKey[PublicKeySize])
{
    SharedKeyMaterial material{};
    crypto_x25519(material.sharedSecret, ownSecretKey, peerPublicKey);
    if (std::memcmp(ownPublicKey, peerPublicKey, PublicKeySize) < 0) {
        copyBytes(material.firstPublicKey, ownPublicKey);
        copyBytes(material.secondPublicKey, peerPublicKey);
    } else {
        copyBytes(material.firstPublicKey, peerPublicKey);
        copyBytes(material.secondPublicKey, ownPublicKey);
    }
    crypto_blake2b(out, FileKeySize, reinterpret_cast<const uint8_t *>(&material), sizeof(material));
    crypto_wipe(&material, sizeof(material));
}

void Crypto::encryptFileChunk(const uint8_t key[FileKeySize], uint32_t chunkIndex,
                              const uint8_t *input, uint8_t *output, size_t length)
{
    const ChunkNonce nonce{chunkIndex, {}};
    crypto_chacha20_ietf(output, input, length, key, reinterpret_cast<const uint8_t *>(&nonce), 0);
}

void Crypto::decryptFileChunk(const uint8_t key[FileKeySize], uint32_t chunkIndex,
                              const uint8_t *input, uint8_t *output, size_t length)
{
    encryptFileChunk(key, chunkIndex, input, output, length);
}

void Crypto::derivePasswordKey(uint8_t out[FileKeySize], const char *password, size_t passwordLength)
{
    static constexpr char Salt[] = "Neon Genesis Eva";

    static uint32_t workArea[Argon2WorkWords];

    crypto_argon2_config config{};
    config.algorithm = CRYPTO_ARGON2_ID;
    config.nb_blocks = Argon2Blocks;
    config.nb_passes = Argon2Passes;
    config.nb_lanes = Argon2Lanes;

    crypto_argon2_inputs inputs{};
    inputs.pass = reinterpret_cast<const uint8_t *>(password);
    inputs.pass_size = static_cast<uint32_t>(passwordLength);
    inputs.salt = reinterpret_cast<const uint8_t *>(Salt);
    inputs.salt_size = sizeof(Salt) - 1;

    crypto_argon2(out, FileKeySize, workArea, config, inputs, crypto_argon2_no_extras);
}

} // namespace lora
