#pragma once

#include <cstddef>
#include <cstdint>

namespace crypto_chat {

/*!
 * One line of chat history.
 */
struct ChatEntry {
    enum class Direction : uint8_t {
        Incoming,
        Outgoing,
        System,
    };

    Direction direction = Direction::System;
    char text[96]{};
    float RSSI = 0.0f;
    float SNR = 0.0f;
    enum class Status : uint8_t {
        None,
        Sending,
        Delivered,
        Failed,
    };
    Status status = Status::None;
};

/*!
 * Bounded message history.
 *
 * Both directions are recorded so the chat page reads like a conversation rather
 * than a receive-only log. Storage is a fixed ring buffer, so memory use is
 * constant and the oldest entry is overwritten once full.
 */
class ChatHistory {
public:
    static constexpr size_t Capacity = 12;
    static constexpr size_t TextSize = sizeof(ChatEntry::text);

    size_t addIncoming(const char *sender, const char *text, float RSSI, float SNR);

    /*! Appends a message this device sent. */
    size_t addOutgoing(const char *text);

    /*! Appends a system note such as a notice or error. */
    size_t addSystem(const char *text);

    void clear();

    size_t count() const;

    ChatEntry &at(size_t index);
    const ChatEntry &at(size_t index) const;

private:
    void push(ChatEntry::Direction direction, const char *prefix, const char *text, ChatEntry::Status status, float RSSI, float SNR);
    ChatEntry entries_[Capacity]{};
    size_t head_ = 0;
    size_t count_ = 0;
};

}
