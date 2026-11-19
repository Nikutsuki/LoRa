#include "ChatHistory.hpp"

#include <cstdio>
#include <cstring>

namespace crypto_chat {

size_t ChatHistory::addIncoming(const char *sender, const char *text, float RSSI, float SNR)
{
    push(ChatEntry::Direction::Incoming, sender, text, ChatEntry::Status::None, RSSI, SNR);
    return count_ - 1;
}

size_t ChatHistory::addOutgoing(const char *text)
{
    push(ChatEntry::Direction::Outgoing, nullptr, text, ChatEntry::Status::None, 0.0f, 0.0f);
    return count_ - 1;
}

size_t ChatHistory::addSystem(const char *text)
{
    push(ChatEntry::Direction::System, nullptr, text, ChatEntry::Status::None, 0.0f, 0.0f);
    return count_ - 1;
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

ChatEntry &ChatHistory::at(size_t index)
{
    // Oldest entry sits at head_ once the ring has wrapped.
    const size_t start = count_ == Capacity ? head_ : 0;
    return entries_[(start + index) % Capacity];
}

const ChatEntry &ChatHistory::at(size_t index) const
{
    // Oldest entry sits at head_ once the ring has wrapped.
    const size_t start = count_ == Capacity ? head_ : 0;
    return entries_[(start + index) % Capacity];
}

void ChatHistory::push(ChatEntry::Direction direction, const char *prefix, const char *text, ChatEntry::Status status, float RSSI, float SNR)
{
    ChatEntry &entry = entries_[head_];
    entry.direction = direction;
    entry.status = status;
    entry.RSSI = RSSI;
    entry.SNR = SNR;
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
