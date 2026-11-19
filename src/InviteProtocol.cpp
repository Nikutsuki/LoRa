#include "InviteProtocol.hpp"

#include <cstring>

#include "CryptoBox.hpp"

namespace crypto_chat
{
    namespace
    {

        constexpr size_t HeaderSize = 27;
        constexpr size_t InvitePayloadSize = NicknameSize + PublicKeySize + PublicKeySize + SignatureSize;
        constexpr size_t EncryptedFixedPayloadSize = NicknameSize + FingerprintSize + NonceSize + MacSize;

        void putU32(uint8_t *out, uint32_t value)
        {
            out[0] = static_cast<uint8_t>(value);
            out[1] = static_cast<uint8_t>(value >> 8);
            out[2] = static_cast<uint8_t>(value >> 16);
            out[3] = static_cast<uint8_t>(value >> 24);
        }

        uint32_t getU32(const uint8_t *data)
        {
            return static_cast<uint32_t>(data[0]) |
                   (static_cast<uint32_t>(data[1]) << 8) |
                   (static_cast<uint32_t>(data[2]) << 16) |
                   (static_cast<uint32_t>(data[3]) << 24);
        }

        void copyFixedNickname(uint8_t out[NicknameSize], const char *nickname)
        {
            std::memset(out, 0, NicknameSize);
            if (nickname != nullptr)
            {
                std::strncpy(reinterpret_cast<char *>(out), nickname, NicknameSize - 1);
            }
        }

    }

    size_t InviteProtocol::headerSize()
    {
        return HeaderSize;
    }

    bool InviteProtocol::encodeInvite(const DeviceIdentity &identity,
                                      PacketType type,
                                      const uint8_t recipientDeviceId[DeviceIdSize],
                                      uint32_t sequence,
                                      uint8_t *out,
                                      size_t capacity,
                                      size_t &outLength)
    {
        if (type != PacketType::InviteBroadcast && type != PacketType::InviteAccept)
        {
            return false;
        }
        if (capacity < HeaderSize + InvitePayloadSize)
        {
            return false;
        }

        PacketHeader header{};
        header.type = type;
        std::memcpy(header.senderDeviceId, identity.deviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, recipientDeviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = InvitePayloadSize;
        writeHeader(header, out);

        uint8_t *payload = out + HeaderSize;
        std::memcpy(payload, identity.nickname, NicknameSize);
        payload += NicknameSize;
        std::memcpy(payload, identity.signingPublicKey, PublicKeySize);
        payload += PublicKeySize;
        std::memcpy(payload, identity.boxPublicKey, PublicKeySize);
        payload += PublicKeySize;
        CryptoBox::signInvite(identity, type, payload);

        outLength = HeaderSize + InvitePayloadSize;
        return true;
    }

    bool InviteProtocol::encodeEncryptedMessage(const DeviceIdentity &identity,
                                                const Contact &contact,
                                                uint32_t sequence,
                                                const char *message,
                                                uint8_t *out,
                                                size_t capacity,
                                                size_t &outLength)
    {
        size_t plainLength = message == nullptr ? 0 : std::strlen(message);
        if (!contact.valid || plainLength == 0 || plainLength > MaxPlainTextSize)
        {
            return false;
        }

        size_t payloadLength = EncryptedFixedPayloadSize + plainLength;
        if (payloadLength > 255 || capacity < HeaderSize + payloadLength)
        {
            return false;
        }

        PacketHeader header{};
        header.type = PacketType::EncryptedMessage;
        std::memcpy(header.senderDeviceId, identity.deviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, contact.deviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = static_cast<uint8_t>(payloadLength);
        writeHeader(header, out);

        uint8_t *payload = out + HeaderSize;
        copyFixedNickname(payload, identity.nickname);
        payload += NicknameSize;
        std::memcpy(payload, identity.fingerprint, FingerprintSize);
        payload += FingerprintSize;
        CryptoBox::makeNonce(payload, identity.deviceId, contact.deviceId, sequence);
        uint8_t *nonce = payload;
        payload += NonceSize;
        uint8_t *mac = payload;
        payload += MacSize;
        uint8_t *cipherText = payload;

        size_t adLength = HeaderSize + NicknameSize + FingerprintSize + NonceSize;
        uint8_t ad[HeaderSize + NicknameSize + FingerprintSize + NonceSize];
        std::memcpy(ad, out, HeaderSize);
        std::memcpy(ad + HeaderSize, out + HeaderSize, NicknameSize + FingerprintSize + NonceSize);

        if (!CryptoBox::encryptForContact(identity, contact, nonce, ad, adLength,
                                          reinterpret_cast<const uint8_t *>(message), plainLength,
                                          cipherText, mac))
        {
            return false;
        }

        outLength = HeaderSize + payloadLength;
        return true;
    }

    bool InviteProtocol::encodeAck(const uint8_t senderDeviceId[DeviceIdSize],
                                   const uint8_t recipientDeviceId[DeviceIdSize],
                                   uint32_t sequence,
                                   uint8_t *out,
                                   size_t capacity,
                                   size_t &outLength)
    {
        if (capacity < HeaderSize)
        {
            return false;
        }

        PacketHeader header{};
        header.type = PacketType::MessageAck;
        std::memcpy(header.senderDeviceId, senderDeviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, recipientDeviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = 0;
        writeHeader(header, out);

        outLength = HeaderSize;
        return true;
    }

    bool InviteProtocol::encodeFileExchangeStart(const DeviceIdentity &sender, const Contact &recipient, uint32_t sequence, const uint8_t fileKey[32], const char *filename, uint32_t fileSize, uint8_t *out, size_t capacity, size_t &outLength)
    {
        size_t nameLen = (filename != nullptr) ? std::strlen(filename) : 0;
        if (nameLen > 64) nameLen = 64;

        uint8_t plainText[32 + 4 + 1 + 64];
        size_t plainLen = 32 + 4 + 1 + nameLen;

        std::memcpy(plainText, fileKey, 32);
        putU32(plainText + 32, fileSize);
        plainText[36] = static_cast<uint8_t>(nameLen);
        std::memcpy(plainText + 37, filename, nameLen);

        size_t payloadLength = NonceSize + MacSize + plainLen;
        if (capacity < HeaderSize + payloadLength || payloadLength > 255)
            return false;

        PacketHeader header{};
        header.type = PacketType::FileExchangeStart;
        std::memcpy(header.senderDeviceId, sender.deviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, recipient.deviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = static_cast<uint8_t>(payloadLength);
        writeHeader(header, out);

        uint8_t *payload = out + HeaderSize;
        uint8_t *nonce = payload;
        uint8_t *mac = payload + NonceSize;
        uint8_t *cipherText = payload + NonceSize + MacSize;

        CryptoBox::makeNonce(nonce, sender.deviceId, recipient.deviceId, sequence);

        uint8_t ad[HeaderSize];
        writeHeader(header, ad);

        if (!CryptoBox::encryptForContact(sender, recipient, nonce, ad, HeaderSize,
                                          plainText, plainLen, cipherText, mac))
        {
            return false;
        }

        outLength = HeaderSize + payloadLength;
        return true;
    }

    bool InviteProtocol::encodeFileExchangeData(const uint8_t senderDeviceId[DeviceIdSize], const uint8_t recipientDeviceId[DeviceIdSize], uint32_t sequence, uint32_t chunkIndex, const uint8_t *chunkData, uint16_t chunkLen, uint8_t *out, size_t capacity, size_t &outLength)
    {
        size_t payloadLength = 4 + chunkLen;
        if (capacity < HeaderSize + payloadLength || payloadLength > 255) return false;

        PacketHeader header{};
        header.type = PacketType::FileExchangeData;
        std::memcpy(header.senderDeviceId, senderDeviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, recipientDeviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = static_cast<uint8_t>(payloadLength);
        writeHeader(header,out);

        uint8_t *payload = out + HeaderSize;
        putU32(payload, chunkIndex);
        std::memcpy(payload + 4, chunkData, chunkLen);

        outLength = HeaderSize + payloadLength;
        
        return true;
    }

    bool InviteProtocol::encodeFileExchangeEnd(const uint8_t senderDeviceId[DeviceIdSize], const uint8_t recipientDeviceId[DeviceIdSize], uint32_t sequence, uint8_t *out, size_t capacity, size_t &outLength)
    {
        if (capacity < HeaderSize)
        {
            return false;
        }

        PacketHeader header{};
        header.type = PacketType::FileExchangeEnd;
        std::memcpy(header.senderDeviceId, senderDeviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, recipientDeviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = 0;

        writeHeader(header, out);
        outLength = HeaderSize;
        return true;
    }

    bool InviteProtocol::encodeFileExchangeCancel(const uint8_t senderDeviceId[DeviceIdSize], const uint8_t recipientDeviceId[DeviceIdSize], uint32_t sequence, uint8_t *out, size_t capacity, size_t &outLength)
    {
        if (capacity < HeaderSize) return false;
        PacketHeader header{};
        header.type = PacketType::FileExchangeCancel;
        std::memcpy(header.senderDeviceId, senderDeviceId, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, recipientDeviceId, DeviceIdSize);
        header.sequence = sequence;
        header.payloadLength = 0;
        writeHeader(header, out);
        outLength = HeaderSize;
        return true;
    }

    bool InviteProtocol::decode(const uint8_t *packet, size_t packetLength, DecodedPacket &decoded)
    {
        if (packet == nullptr || packetLength < HeaderSize)
        {
            return false;
        }
        if (!readHeader(packet, packetLength, decoded.header))
        {
            return false;
        }
        if (HeaderSize + decoded.header.payloadLength != packetLength)
        {
            return false;
        }
        decoded.payload = packet + HeaderSize;
        decoded.payloadLength = decoded.header.payloadLength;
        return true;
    }

    bool InviteProtocol::decodeInvitePayload(const DecodedPacket &packet, Contact &contact)
    {
        if ((packet.header.type != PacketType::InviteBroadcast && packet.header.type != PacketType::InviteAccept) ||
            packet.payloadLength != InvitePayloadSize)
        {
            return false;
        }

        std::memset(&contact, 0, sizeof(contact));
        contact.valid = true;
        std::memcpy(contact.deviceId, packet.header.senderDeviceId, DeviceIdSize);
        const uint8_t *payload = packet.payload;
        std::memcpy(contact.nickname, payload, NicknameSize);
        contact.nickname[NicknameSize - 1] = '\0';
        payload += NicknameSize;
        std::memcpy(contact.signingPublicKey, payload, PublicKeySize);
        payload += PublicKeySize;
        std::memcpy(contact.boxPublicKey, payload, PublicKeySize);
        payload += PublicKeySize;
        CryptoBox::fingerprint(contact.fingerprint, contact.deviceId, contact.signingPublicKey, contact.boxPublicKey);
        return CryptoBox::verifyInvite(contact, packet.header.type, payload);
    }

    bool InviteProtocol::decodeEncryptedMessage(const DeviceIdentity &identity,
                                                const Contact &contact,
                                                const DecodedPacket &packet,
                                                char *outMessage,
                                                size_t outCapacity)
    {
        if (packet.header.type != PacketType::EncryptedMessage ||
            packet.payloadLength <= EncryptedFixedPayloadSize ||
            outCapacity == 0)
        {
            return false;
        }

        size_t cipherLength = packet.payloadLength - EncryptedFixedPayloadSize;
        if (cipherLength >= outCapacity)
        {
            return false;
        }

        const uint8_t *payload = packet.payload;
        const uint8_t *senderFingerprint = payload + NicknameSize;
        const uint8_t *nonce = senderFingerprint + FingerprintSize;
        const uint8_t *mac = nonce + NonceSize;
        const uint8_t *cipherText = mac + MacSize;

        if (std::memcmp(senderFingerprint, contact.fingerprint, FingerprintSize) != 0)
        {
            return false;
        }

        size_t adLength = HeaderSize + NicknameSize + FingerprintSize + NonceSize;
        uint8_t ad[HeaderSize + NicknameSize + FingerprintSize + NonceSize];
        uint8_t header[HeaderSize];
        writeHeader(packet.header, header);
        std::memcpy(ad, header, HeaderSize);
        std::memcpy(ad + HeaderSize, packet.payload, NicknameSize + FingerprintSize + NonceSize);

        if (!CryptoBox::decryptFromContact(identity, contact, nonce, ad, adLength,
                                           cipherText, cipherLength, mac,
                                           reinterpret_cast<uint8_t *>(outMessage)))
        {
            return false;
        }
        outMessage[cipherLength] = '\0';
        return true;
    }

    bool InviteProtocol::decodeFileExchangeStart(const DeviceIdentity &recipient, const Contact &sender, const DecodedPacket &packet, uint8_t fileKey[32], char *filename, size_t maxFilenameLen, uint32_t &fileSize)
    {
        if (packet.payloadLength < NonceSize + MacSize + 37) return false;

        const uint8_t *nonce = packet.payload;
        const uint8_t *mac = packet.payload + NonceSize;
        const uint8_t *cipherText = packet.payload + NonceSize + MacSize;
        size_t cipherLen = packet.payloadLength - (NonceSize + MacSize);

        uint8_t ad[HeaderSize];
        writeHeader(packet.header, ad);

        uint8_t plainText[MaxPlainTextSize];
        if (!CryptoBox::decryptFromContact(recipient, sender, nonce, ad, HeaderSize,
                                           cipherText, cipherLen, mac, plainText))
        {
            return false;
        }

        std::memcpy(fileKey, plainText, 32);
        fileSize = getU32(plainText + 32);
        uint8_t nameLen = plainText[36];
        if (nameLen >= maxFilenameLen) nameLen = maxFilenameLen - 1;

        std::memcpy(filename, plainText + 37, nameLen);
        filename[nameLen] = '\0';
        return true;
    }

    bool InviteProtocol::decodeFileExchangeData(const DecodedPacket &packet, uint32_t &chunkIndex, const uint8_t *&ChunkData, size_t &chunkLen)
    {
        if (packet.payloadLength < 4) return false;
        chunkIndex = getU32(packet.payload);
        ChunkData = packet.payload + 4;
        chunkLen = packet.payloadLength - 4;
        return true;
    }

    bool InviteProtocol::isBroadcast(const uint8_t deviceId[DeviceIdSize])
    {
        return std::memcmp(deviceId, BroadcastDeviceId, DeviceIdSize) == 0;
    }

    bool InviteProtocol::isForDevice(const DecodedPacket &packet, const uint8_t deviceId[DeviceIdSize])
    {
        return isBroadcast(packet.header.recipientDeviceId) ||
               std::memcmp(packet.header.recipientDeviceId, deviceId, DeviceIdSize) == 0;
    }

    void InviteProtocol::writeHeader(const PacketHeader &header, uint8_t *out)
    {
        putU32(out, Magic);
        out[4] = Version;
        out[5] = static_cast<uint8_t>(header.type);
        std::memcpy(out + 6, header.senderDeviceId, DeviceIdSize);
        std::memcpy(out + 14, header.recipientDeviceId, DeviceIdSize);
        putU32(out + 22, header.sequence);
        out[26] = header.payloadLength;
    }

    bool InviteProtocol::readHeader(const uint8_t *data, size_t size, PacketHeader &header)
    {
        if (size < HeaderSize || getU32(data) != Magic || data[4] != Version)
        {
            return false;
        }
        uint8_t type = data[5];
        if (type < static_cast<uint8_t>(PacketType::InviteBroadcast) ||
            type > static_cast<uint8_t>(PacketType::FileExchangeCancel))
        {
            return false;
        }
        header.type = static_cast<PacketType>(type);
        std::memcpy(header.senderDeviceId, data + 6, DeviceIdSize);
        std::memcpy(header.recipientDeviceId, data + 14, DeviceIdSize);
        header.sequence = getU32(data + 22);
        header.payloadLength = data[26];
        return true;
    }

}
