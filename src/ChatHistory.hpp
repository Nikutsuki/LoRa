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

    /*! Appends an incoming message, attributed to \p sender. */
    void addIncoming(const char *sender, const char *text);

    /*! Appends a message this device sent. */
    void addOutgoing(const char *text);

    /*! Appends a system note such as a notice or error. */
    void addSystem(const char *text);

    void clear();

    size_t count() const;

    /*!
     * Returns entry \p index where 0 is the oldest retained entry.
     */
    const ChatEntry &at(size_t index) const;

private:
    void push(ChatEntry::Direction direction, const char *prefix, const char *text);

    ChatEntry entries_[Capacity]{};
    size_t head_ = 0;
    size_t count_ = 0;
};

}
