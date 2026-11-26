#include "ContactStore.hpp"

#include <cstring>

#include "hardware/flash.h"
#include "hardware/regs/addressmap.h"
#include "hardware/sync.h"

namespace lora {
namespace {
constexpr uint32_t FlashSectorSize = FLASH_SECTOR_SIZE;
constexpr uint32_t FlashPageSize = FLASH_PAGE_SIZE;
static_assert(sizeof(ContactStore::StoredImage) < FlashSectorSize, "contact store must fit in one flash sector");
}

bool ContactStore::load()
{
    const auto *stored = reinterpret_cast<const StoredImage *>(XIP_BASE + flashOffset());
    if (stored->magic != Magic || stored->version != Version) {
        reset();
        return false;
    }

    StoredImage candidate;
    std::memcpy(&candidate, stored, sizeof(candidate));
    uint32_t expected = candidate.crc;
    candidate.crc = 0;
    if (crc32(reinterpret_cast<const uint8_t *>(&candidate), sizeof(candidate)) != expected) {
        reset();
        return false;
    }

    image_ = *stored;
    return true;
}

bool ContactStore::save()
{
    StoredImage toWrite = image_;
    toWrite.magic = Magic;
    toWrite.version = Version;
    toWrite.reserved = 0;
    toWrite.crc = 0;
    toWrite.crc = crc32(reinterpret_cast<const uint8_t *>(&toWrite), sizeof(toWrite));

    uint8_t sector[FlashSectorSize];
    std::memset(sector, 0xff, sizeof(sector));
    std::memcpy(sector, &toWrite, sizeof(toWrite));

    uint32_t interruptState = save_and_disable_interrupts();
    flash_range_erase(flashOffset(), FlashSectorSize);
    flash_range_program(flashOffset(), sector, FlashSectorSize);
    restore_interrupts(interruptState);

    image_ = toWrite;
    return true;
}

bool ContactStore::hasIdentity() const
{
    return image_.hasIdentity == 1;
}

DeviceIdentity &ContactStore::identity()
{
    return image_.identity;
}

const DeviceIdentity &ContactStore::identity() const
{
    return image_.identity;
}

bool ContactStore::setIdentity(const DeviceIdentity &identity)
{
    image_.identity = identity;
    image_.hasIdentity = 1;
    return true;
}

size_t ContactStore::contactCount() const
{
    size_t count = 0;
    for (const auto &contact : image_.contacts) {
        if (contact.valid) {
            count++;
        }
    }
    return count;
}

const Contact *ContactStore::contactAt(size_t index) const
{
    size_t seen = 0;
    for (const auto &contact : image_.contacts) {
        if (contact.valid) {
            if (seen == index) {
                return &contact;
            }
            seen++;
        }
    }
    return nullptr;
}

const Contact *ContactStore::findByDeviceId(const uint8_t deviceId[DeviceIdSize]) const
{
    for (const auto &contact : image_.contacts) {
        if (contact.valid && std::memcmp(contact.deviceId, deviceId, DeviceIdSize) == 0) {
            return &contact;
        }
    }
    return nullptr;
}

bool ContactStore::nicknameCollisionWithDifferentKey(const Contact &candidate) const
{
    for (const auto &contact : image_.contacts) {
        if (!contact.valid) {
            continue;
        }
        if (std::strncmp(contact.nickname, candidate.nickname, NicknameSize) == 0 &&
            std::memcmp(contact.fingerprint, candidate.fingerprint, FingerprintSize) != 0) {
            return true;
        }
    }
    return false;
}

bool ContactStore::upsertContact(const Contact &contact)
{
    if (!contact.valid) {
        return false;
    }

    for (auto &slot : image_.contacts) {
        if (slot.valid && std::memcmp(slot.fingerprint, contact.fingerprint, FingerprintSize) == 0) {
            slot = contact;
            return true;
        }
    }

    for (auto &slot : image_.contacts) {
        if (!slot.valid) {
            slot = contact;
            return true;
        }
    }
    return false;
}

void ContactStore::clearContacts()
{
    std::memset(image_.contacts, 0, sizeof(image_.contacts));
}

uint32_t ContactStore::crc32(const uint8_t *data, size_t size)
{
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++) {
            uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1u)));
            crc = (crc >> 1) ^ (0xedb88320u & mask);
        }
    }
    return ~crc;
}

uint32_t ContactStore::flashOffset()
{
    return PICO_FLASH_SIZE_BYTES - FlashSectorSize;
}

void ContactStore::reset()
{
    std::memset(&image_, 0, sizeof(image_));
    image_.magic = Magic;
    image_.version = Version;
}

}
