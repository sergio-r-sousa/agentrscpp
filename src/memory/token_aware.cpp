// token_aware.cpp — TokenAwareMemory implementation.
// Equivalent to agentrs-memory/src/token_aware.rs

#include "agentrs/memory/token_aware.hpp"

#include <algorithm>
#include <cmath>
#include <string>

namespace agentrs {

// ---------------------------------------------------------------------------
// ApproximateTokenizer
// ---------------------------------------------------------------------------

std::size_t ApproximateTokenizer::count(std::string_view text) const {
    // Heuristic: tokens ≈ text.length() / 4
    return text.length() / 4;
}

// ---------------------------------------------------------------------------
// TokenAwareMemory — construction
// ---------------------------------------------------------------------------

TokenAwareMemory::TokenAwareMemory(std::size_t max_tokens)
    : max_tokens_(max_tokens),
      tokenizer_(std::make_shared<ApproximateTokenizer>()) {}

TokenAwareMemory::TokenAwareMemory(std::size_t max_tokens,
                                     std::shared_ptr<ITokenizer> tokenizer)
    : max_tokens_(max_tokens),
      tokenizer_(std::move(tokenizer)) {}

// ---------------------------------------------------------------------------
// IMemory
// ---------------------------------------------------------------------------

void TokenAwareMemory::store(std::string_view /*key*/, const Message& value) {
    messages_.push_back(value);
    trim_to_budget();
}

std::vector<Message> TokenAwareMemory::retrieve(std::string_view query,
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

std::vector<Message> TokenAwareMemory::history() const {
    return messages_;
}

void TokenAwareMemory::clear() {
    messages_.clear();
}

// ---------------------------------------------------------------------------
// SearchableMemory equivalent
// ---------------------------------------------------------------------------

std::size_t TokenAwareMemory::token_count() const {
    return total_tokens();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::size_t TokenAwareMemory::total_tokens() const {
    std::size_t total = 0;
    for (const auto& msg : messages_) {
        total += tokenizer_->count(msg.text_content());
    }
    return total;
}

void TokenAwareMemory::trim_to_budget() {
    while (total_tokens() > max_tokens_ && messages_.size() > 1) {
        // Find the first non-system message and remove it.
        auto it = std::find_if(messages_.begin(), messages_.end(),
                               [](const Message& msg) {
                                   return msg.role != Role::System;
                               });
        if (it != messages_.end()) {
            messages_.erase(it);
        } else {
            // Only system messages remain — stop trimming.
            break;
        }
    }
}

} // namespace agentrs
