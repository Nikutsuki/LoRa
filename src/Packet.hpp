#pragma once

#include <cstddef>
#include <cstdint>

#include "Types.hpp"

namespace lora {

class Packet {
public:
    static bool encodeInvite(const DeviceIdentity &identity,
                             const InviteParams &params,
                             PacketBuffer &out);

    static bool encodeMessage(const PeerSession &peer,
                              uint32_t sequence,
                              const char *message,
                              PacketBuffer &out);

    static bool encodeControl(PacketType type,
                              const PacketRoute &route,
                              PacketBuffer &out);

    static bool encodeFileStart(const PeerSession &peer,
                                uint32_t sequence,
                                const FileStartInfo &info,
                                PacketBuffer &out);

    static bool encodeFileData(const PacketRoute &route,
                               const FileChunkView &chunk,
                               PacketBuffer &out);

    static bool decode(const uint8_t *data, size_t length, DecodedPacket &decoded);
    static bool decodeInvite(const DecodedPacket &packet, const uint8_t *pairingKey, Contact &contact);
    static bool decodeMessage(const PeerSession &peer,
                              const DecodedPacket &packet,
                              char *outMessage,
                              size_t outCapacity);
    static bool decodeFileStart(const PeerSession &peer,
                                const DecodedPacket &packet,
                                FileStartInfo &outInfo);
    static bool decodeFileData(const DecodedPacket &packet, FileChunkView &outChunk);

    static bool isBroadcast(const uint8_t deviceId[DeviceIdSize]);
    static bool isForDevice(const DecodedPacket &packet, const uint8_t deviceId[DeviceIdSize]);

private:
    static constexpr uint32_t Magic = 0x3148314c; // "LCL1"
    static constexpr uint8_t Version = 1;

    static PacketHeader makeHeader(PacketType type,
                                   const PacketRoute &route,
                                   uint8_t payloadLength);
    static void writeHeader(const PacketHeader &header, uint8_t *out);
    static bool readHeader(const uint8_t *data, size_t size, PacketHeader &header);
};

} // namespace lora
