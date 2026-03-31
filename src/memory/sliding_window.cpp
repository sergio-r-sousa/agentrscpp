// sliding_window.cpp — SlidingWindowMemory implementation.
// Equivalent to agentrs-memory/src/sliding_window.rs

#include "agentrs/memory/sliding_window.hpp"

#include <algorithm>
#include <string>

namespace agentrs {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

SlidingWindowMemory::SlidingWindowMemory(std::size_t window_size)
    : system_message_(std::nullopt), window_size_(window_size) {}

// ---------------------------------------------------------------------------
// IMemory
// ---------------------------------------------------------------------------

void SlidingWindowMemory::store(std::string_view /*key*/, const Message& value) {
    // System messages are stored separately and never counted toward the window.
    if (value.role == Role::System) {
        system_message_ = value;
        return;
    }

    messages_.push_back(value);
    while (messages_.size() > window_size_) {
        messages_.pop_front();
    }
}

std::vector<Message> SlidingWindowMemory::retrieve(std::string_view query,
                                                    std::size_t limit) {
    std::string q(query);
    std::transform(q.begin(), q.end(), q.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    std::vector<Message> result;
    for (const auto& msg : messages_) {
        if (result.size() >= limit) break;

        std::string text = msg.text_content();
        std::transform(text.begin(), text.end(), text.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (text.find(q) != std::string::npos) {
            result.push_back(msg);
        }
    }
    return result;
}

std::vector<Message> SlidingWindowMemory::history() const {
    std::vector<Message> result;
    result.reserve(messages_.size() + 1);

    if (system_message_.has_value()) {
        result.push_back(system_message_.value());
    }

    for (const auto& msg : messages_) {
        result.push_back(msg);
    }
    return result;
}

void SlidingWindowMemory::clear() {
    messages_.clear();
    // Note: system message is NOT cleared — mirrors Rust behavior
    // where only the deque is cleared.
}

// ---------------------------------------------------------------------------
// SearchableMemory equivalent
// ---------------------------------------------------------------------------

std::size_t SlidingWindowMemory::token_count() const {
    std::size_t total = 0;
    for (const auto& msg : messages_) {
        total += msg.text_content().length() / 4;
    }
    return total;
}

} // namespace agentrs
