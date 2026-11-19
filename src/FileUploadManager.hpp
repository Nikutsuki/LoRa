#pragma once
#include "protocol/Protocol.hpp"
#include "protocol/Framing.hpp"
#include "protocol/Messages.hpp"
#include <cstdint>
#include <cstring>

namespace crypto_chat {
    class LoraMessenger;
    class ContactStore;
    class InviteProtocol;
    struct Contact;
}

namespace proto
{

    class LinkSession;

    class FileUploadManager
    {
    public:
        explicit FileUploadManager(
            LinkSession &session,
            crypto_chat::LoraMessenger &radio,
            crypto_chat::ContactStore &contacts) : session_(session),
                                                   radio_(radio),
                                                   contacts_(contacts) {}

        enum class State
        {
            Idle,
            Uploading,
            Completed,
            Failed
        };

        bool handleStartFrame(const uint8_t *payload, size_t length);
        bool handleDataFrame(const uint8_t *payload, size_t length);

    private:
        LinkSession &session_;
        crypto_chat::LoraMessenger &radio_;
        crypto_chat::ContactStore &contacts_;

        State state_ = State::Idle;
        char filename_[256]{};
        uint32_t fileSize_ = 0;
        uint32_t bytesSent_ = 0;
        const crypto_chat::Contact *recipient_ = nullptr;
    };

}