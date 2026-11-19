#pragma once

#include <cstddef>
#include <cstdint>

#include "CryptoTypes.hpp"

namespace crypto_chat
{

    class InviteProtocol
    {
    public:
        static constexpr uint8_t BroadcastDeviceId[DeviceIdSize] = {
            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        static size_t headerSize();

        static bool encodeInvite(
            const DeviceIdentity &identity,
            PacketType type,
            const uint8_t recipientDeviceId[DeviceIdSize],
            uint32_t sequence,
            uint8_t *out,
            size_t capacity,
            size_t &outLength);

        static bool encodeEncryptedMessage(
            const DeviceIdentity &identity,
            const Contact &contact,
            uint32_t sequence,
            const char *message,
            uint8_t *out,
            size_t capacity,
            size_t &outLength);
        static bool encodeAck(
            const uint8_t senderDeviceId[DeviceIdSize],
            const uint8_t recipientDeviceId[DeviceIdSize],
            uint32_t sequence,
            uint8_t *out,
            size_t capacity,
            size_t &outLength);

        static bool encodeFileExchangeStart(
            const DeviceIdentity &sender,
            const Contact &recipient,
            uint32_t sequence,
            const uint8_t fileKey[32],
            const char *filename,
            uint32_t fileSize,
            uint8_t *out,
            size_t capacity,
            size_t &outLength);
        static bool encodeFileExchangeData(
            const uint8_t senderDeviceId[DeviceIdSize],
            const uint8_t recipientDeviceId[DeviceIdSize],
            uint32_t sequence,
            uint32_t chunkIndex,
            const uint8_t *chunkData,
            uint16_t chunkLen,
            uint8_t *out,
            size_t capacity,
            size_t &outLength);
        static bool encodeFileExchangeEnd(
            const uint8_t senderDeviceId[DeviceIdSize],
            const uint8_t recipientDeviceId[DeviceIdSize],
            uint32_t sequence,
            uint8_t *out,
            size_t capacity,
            size_t &outLength);
        static bool encodeFileExchangeCancel(
            const uint8_t senderDeviceId[DeviceIdSize],
            const uint8_t recipientDeviceId[DeviceIdSize],
            uint32_t sequence,
            uint8_t *out, size_t capacity, size_t &outLength
        );

        static bool decode(const uint8_t *packet, size_t packetLength, DecodedPacket &decoded);
        static bool decodeInvitePayload(const DecodedPacket &packet, Contact &contact);
        static bool decodeEncryptedMessage(
            const DeviceIdentity &identity,
            const Contact &contact,
            const DecodedPacket &packet,
            char *outMessage,
            size_t outCapacity);
        static bool decodeFileExchangeStart(
            const DeviceIdentity &recipient,
            const Contact &sender,
            const DecodedPacket &packet,
            uint8_t fileKey[32],
            char *filename,
            size_t maxFilenameLen,
            uint32_t &fileSize);
        static bool decodeFileExchangeData(const DecodedPacket &packet, uint32_t &chunkIndex, const uint8_t *&ChunkData, size_t &chunkLen);

        static bool isBroadcast(const uint8_t deviceId[DeviceIdSize]);
        static bool isForDevice(const DecodedPacket &packet, const uint8_t deviceId[DeviceIdSize]);

    private:
        static constexpr uint32_t Magic = 0x3143484c; // "LHC1"
        static constexpr uint8_t Version = 1;

        static void writeHeader(const PacketHeader &header, uint8_t *out);
        static bool readHeader(const uint8_t *data, size_t size, PacketHeader &header);
    };

}
