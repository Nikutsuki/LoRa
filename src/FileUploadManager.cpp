#include "FileUploadManager.hpp"

#include "protocol/LinkSession.hpp"
#include "CryptoTypes.hpp"
#include "InviteProtocol.hpp"

namespace proto
{
    bool FileUploadManager::handleStartFrame(const uint8_t *payload, size_t length)
    {
        FileExchangeStartMessage message;
        PayloadReader reader(payload, length);

        if (!readFileExchangeStart(reader, message)) return false;

        std::strncpy(filename_, message.filename, sizeof(filename_) - 1);
        filename_[sizeof(filename_) - 1] = '\0';

        fileSize_ = message.fileSize;
        bytesSent_ = 0;
        state_ = State::Uploading;

        uint8_t packet[crypto_chat::MaxPacketSize];
        size_t packetLength = 0;

        // if (!crypto_chat::InviteProtocol::encodeFileExchangeStart())

        this->session_.sendExchangeAck(bytesSent_, 0);

        return true;
    }

    bool FileUploadManager::handleDataFrame(const uint8_t *payload, size_t length)
    {
        FileExchangeDataMessage message;
        PayloadReader reader(payload, length);

        if (!readFileExchangeData(reader, message)) return false;

        return true;
    }
}
