#include "LinkSession.hpp"

namespace proto
{

    LinkSession::LinkSession(ByteStream &stream)
        : stream_(stream), writer_(writeBuffer_, sizeof(writeBuffer_)), reader_(readBuffer_, sizeof(readBuffer_))
    {
    }

    void LinkSession::init()
    {
        stream_.init();
    }

    void LinkSession::poll()
    {
        uint8_t chunk[32];
        size_t available = 0;
        do
        {
            available = stream_.read(chunk, sizeof(chunk));
            for (size_t i = 0; i < available; i++)
            {
                reader_.feed(chunk[i], &LinkSession::onFrame, this);
            }
        } while (available == sizeof(chunk));
    }

    void LinkSession::onFrame(MessageType type, const uint8_t *payload, size_t length, void *context)
    {
        auto *session = static_cast<LinkSession *>(context);
        session->hostSeen_ = true;

        switch (type)
        {
        case MessageType::Key:
            session->handleKeyFrame(payload, length);
            break;
        case MessageType::Ping:
            // Answer immediately so a probing host can identify this device without
            // having to understand Screen frames.
            session->emit(MessageType::Pong, nullptr, 0);
            session->resyncRequested_ = true;
            break;
        case MessageType::FileExchangeStart:
            session->handleFileStartFrame(payload, length);
            break;
        case MessageType::FileExchangeData:
            session->handleFileDataFrame(payload, length);
            break;
        case MessageType::FileExchangeEnd:
        case MessageType::FileExchangeCancel:
        default:
            break;
        }
    }

    bool LinkSession::consumeResyncRequest()
    {
        const bool requested = resyncRequested_;
        resyncRequested_ = false;
        return requested;
    }

    void LinkSession::handleKeyFrame(const uint8_t *payload, size_t length)
    {
        if (keyCount_ >= KeyQueueSize)
        {
            return;
        }

        KeyMessage key;
        PayloadReader reader(payload, length);
        if (!readKey(reader, key))
        {
            return;
        }

        keyQueue_[(keyHead_ + keyCount_) % KeyQueueSize] = key;
        keyCount_++;
    }

    void LinkSession::handleFileStartFrame(const uint8_t *payload, size_t length)
    {
        PayloadReader reader(payload, length);
        if (readFileExchangeStart(reader, pendingFileStart_)) {
            std::strncpy(pendingFilename_, pendingFileStart_.filename, sizeof(pendingFilename_) - 1);
            pendingFilename_[sizeof(pendingFilename_) - 1] = '\0';
            pendingFileStart_.filename = pendingFilename_;
            hasPendingFileStart_ = true;
        }
    }

    void LinkSession::handleFileDataFrame(const uint8_t *payload, size_t length)
    {
        PayloadReader reader(payload, length);
        if (readFileExchangeData(reader, pendingFileData_)) {
            hasPendingFileData_ = true;
        }
    }

    bool LinkSession::nextKey(KeyMessage &key)
    {
        if (keyCount_ == 0)
        {
            return false;
        }
        key = keyQueue_[keyHead_];
        keyHead_ = (keyHead_ + 1) % KeyQueueSize;
        keyCount_--;
        return true;
    }

    bool LinkSession::hostSeen() const
    {
        return hostSeen_;
    }

    bool LinkSession::screenOverflowed() const
    {
        return screenOverflowed_;
    }

    bool LinkSession::sendHello(uint16_t width, uint16_t height)
    {
        HelloMessage hello;
        hello.width = width;
        hello.height = height;
        hello.protocolVersion = ProtocolVersion;

        PayloadBuilder builder(payloadBuffer_, sizeof(payloadBuffer_));
        if (!writeHello(builder, hello))
        {
            return false;
        }
        return emit(MessageType::Hello, payloadBuffer_, builder.size());
    }

    bool LinkSession::sendNotice(const NoticeMessage &notice)
    {
        PayloadBuilder builder(payloadBuffer_, sizeof(payloadBuffer_));
        if (!writeNotice(builder, notice))
        {
            return false;
        }
        return emit(MessageType::Notice, payloadBuffer_, builder.size());
    }

    bool LinkSession::sendExchangeAck(uint32_t chunkIndex, uint8_t status)
    {
        FileExchangeAckMessage ack{chunkIndex, status};
        PayloadBuilder builder(payloadBuffer_, sizeof(payloadBuffer_));
        if (!writeFileExchangeAck(builder, ack))
        {
            return false;
        }
        return emit(MessageType::FileExchangeAck, payloadBuffer_, builder.size());
    }

    bool LinkSession::nextFileStart(FileExchangeStartMessage &msg)
    {
        if (!hasPendingFileStart_) {
            return false;
        }
        msg = pendingFileStart_;
        hasPendingFileStart_ = false;
        return true;
    }

    bool LinkSession::nextFileData(FileExchangeDataMessage &msg)
    {
        if (!hasPendingFileData_) {
            return false;
        }

        msg = pendingFileData_;
        hasPendingFileData_ = false;
        return true;
    }

    bool LinkSession::emit(MessageType type, const uint8_t *payload, size_t length)
    {
        if (!writer_.begin(type, length))
        {
            return false;
        }

        auto *target = writer_.payload();
        for (size_t i = 0; i < length; i++)
        {
            target[i] = payload[i];
        }

        writer_.commit();
        stream_.write(writer_.frameData(), writer_.frameLength());
        return true;
    }

    void LinkSession::beginScreen(uint16_t width, uint16_t height, bool fullRedraw)
    {
        screenWidth_ = width;
        screenHeight_ = height;
        screenFullRedraw_ = fullRedraw;
        screenCount_ = 0;
        screenOpen_ = true;
        screenOverflowed_ = false;
    }

    bool LinkSession::addComponent(const ComponentMessage &component)
    {
        if (!screenOpen_ || screenCount_ >= MaxScreenComponents)
        {
            screenOverflowed_ = true;
            return false;
        }
        screenComponents_[screenCount_++] = component;
        return true;
    }

    bool LinkSession::finishScreen()
    {
        if (!screenOpen_)
        {
            return false;
        }
        screenOpen_ = false;

        ScreenMessageHeader header;
        header.width = screenWidth_;
        header.height = screenHeight_;
        header.fullRedraw = screenFullRedraw_;
        header.count = static_cast<uint8_t>(screenCount_);

        PayloadBuilder builder(payloadBuffer_, sizeof(payloadBuffer_));
        if (!writeScreenHeader(builder, header))
        {
            return false;
        }
        for (size_t i = 0; i < screenCount_; i++)
        {
            if (!writeComponent(builder, screenComponents_[i]))
            {
                return false;
            }
        }

        return emit(MessageType::Screen, payloadBuffer_, builder.size());
    }

}
