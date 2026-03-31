// in_memory.cpp — InMemoryMemory implementation.
// Equivalent to agentrs-memory/src/in_memory.rs

#include "agentrs/memory/in_memory.hpp"

#include <algorithm>
#include <string>

namespace agentrs {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

InMemoryMemory::InMemoryMemory()
    : max_messages_(std::nullopt) {}

InMemoryMemory::InMemoryMemory(std::size_t max_messages)
    : max_messages_(max_messages) {}

// ---------------------------------------------------------------------------
// IMemory
// ---------------------------------------------------------------------------

void InMemoryMemory::store(std::string_view /*key*/, const Message& value) {
    messages_.push_back(value);
    trim();
}

std::vector<Message> InMemoryMemory::retrieve(std::string_view query,
                                               std::size_t limit) {
    // Case-insensitive substring search.
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

std::vector<Message> InMemoryMemory::history() const {
    return messages_;
}

void InMemoryMemory::clear() {
    messages_.clear();
}

// ---------------------------------------------------------------------------
// SearchableMemory equivalent
// ---------------------------------------------------------------------------

std::size_t InMemoryMemory::token_count() const {
    std::size_t total = 0;
    for (const auto& msg : messages_) {
        total += msg.text_content().length() / 4;
    }
    return total;
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void InMemoryMemory::trim() {
    if (!max_messages_.has_value()) return;

    const auto max = max_messages_.value();
    if (messages_.size() <= max) return;

    const auto overflow = messages_.size() - max;
    messages_.erase(messages_.begin(),
                    messages_.begin() + static_cast<std::ptrdiff_t>(overflow));
}

} // namespace agentrs
