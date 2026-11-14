#include "ChatHistory.hpp"

#include <cstdio>
#include <cstring>

namespace crypto_chat {

void ChatHistory::addIncoming(const char *sender, const char *text)
{
    push(ChatEntry::Direction::Incoming, sender, text);
}

void ChatHistory::addOutgoing(const char *text)
{
    push(ChatEntry::Direction::Outgoing, nullptr, text);
}

void ChatHistory::addSystem(const char *text)
{
    push(ChatEntry::Direction::System, nullptr, text);
}

void ChatHistory::clear()
{
    head_ = 0;
    count_ = 0;
}

size_t ChatHistory::count() const
{
    return count_;
}

const ChatEntry &ChatHistory::at(size_t index) const
{
    // Oldest entry sits at head_ once the ring has wrapped.
    const size_t start = count_ == Capacity ? head_ : 0;
    return entries_[(start + index) % Capacity];
}

void ChatHistory::push(ChatEntry::Direction direction, const char *prefix, const char *text)
{
    ChatEntry &entry = entries_[head_];
    entry.direction = direction;
    std::memset(entry.text, 0, sizeof(entry.text));

    if (prefix != nullptr && prefix[0] != '\0') {
        std::snprintf(entry.text, sizeof(entry.text), "%s: %s", prefix, text != nullptr ? text : "");
    } else if (direction == ChatEntry::Direction::Outgoing) {
        std::snprintf(entry.text, sizeof(entry.text), "> %s", text != nullptr ? text : "");
    } else {
        std::snprintf(entry.text, sizeof(entry.text), "%s", text != nullptr ? text : "");
    }

    head_ = (head_ + 1) % Capacity;
    if (count_ < Capacity) {
        count_++;
    }
}

}
