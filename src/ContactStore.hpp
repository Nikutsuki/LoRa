#pragma once

#include <cstddef>
#include <cstdint>

#include "CryptoTypes.hpp"

namespace crypto_chat {

class ContactStore {
public:
    struct StoredImage {
        uint32_t magic;
        uint16_t version;
        uint16_t reserved;
        uint32_t crc;
        uint8_t hasIdentity;
        DeviceIdentity identity;
        Contact contacts[MaxContacts];
    };

    bool load();
    bool save();
    bool hasIdentity() const;
    DeviceIdentity &identity();
    const DeviceIdentity &identity() const;
    bool setIdentity(const DeviceIdentity &identity);

    size_t contactCount() const;
    const Contact *contactAt(size_t index) const;
    Contact *contactAt(size_t index);
    const Contact *findByDeviceId(const uint8_t deviceId[DeviceIdSize]) const;
    const Contact *findByFingerprint(const uint8_t fingerprint[FingerprintSize]) const;
    bool nicknameCollisionWithDifferentKey(const Contact &candidate) const;
    bool upsertContact(const Contact &contact);
    void clearContacts();

private:
    static constexpr uint32_t Magic = 0x4354434c; // "LCTC"
    static constexpr uint16_t Version = 1;

    StoredImage image_{};

    static uint32_t crc32(const uint8_t *data, size_t size);
    static uint32_t flashOffset();
    void reset();
};

}
