#pragma once
// token_aware.hpp — Token-budget-aware memory backend.
// Equivalent to agentrs-memory/src/token_aware.rs

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

/// Counts approximate tokens for a string.
/// Equivalent to Rust's `Tokenizer` trait.
class ITokenizer {
public:
    virtual ~ITokenizer() = default;

    /// Returns the estimated token count.
    [[nodiscard]] virtual std::size_t count(std::string_view text) const = 0;
};

/// Lightweight tokenizer approximation that avoids external dependencies.
/// Heuristic: tokens ≈ text.length() / 4
/// Equivalent to Rust's `ApproximateTokenizer`.
class ApproximateTokenizer : public ITokenizer {
public:
    [[nodiscard]] std::size_t count(std::string_view text) const override;
};

/// Memory backend that trims history to fit a token budget.
///
/// When the total tokens exceed `max_tokens`, the oldest non-system messages
/// are removed until the budget is met.
/// Equivalent to Rust's `TokenAwareMemory`.
class TokenAwareMemory : public IMemory {
public:
    /// Creates a token-aware backend with the default approximate tokenizer.
    explicit TokenAwareMemory(std::size_t max_tokens);

    /// Creates a token-aware backend with a custom tokenizer.
    TokenAwareMemory(std::size_t max_tokens, std::shared_ptr<ITokenizer> tokenizer);

    ~TokenAwareMemory() override = default;

    // Movable
    TokenAwareMemory(TokenAwareMemory&&) noexcept = default;
    TokenAwareMemory& operator=(TokenAwareMemory&&) noexcept = default;

    // Not copyable (shared_ptr to interface)
    TokenAwareMemory(const TokenAwareMemory&) = default;
    TokenAwareMemory& operator=(const TokenAwareMemory&) = default;

    // --- IMemory ------------------------------------------------------------

    void store(std::string_view key, const Message& value) override;

    [[nodiscard]] std::vector<Message> retrieve(std::string_view query,
                                                 std::size_t limit) override;

    [[nodiscard]] std::vector<Message> history() const override;

    void clear() override;

    // --- Extra (mirrors Rust SearchableMemory) ------------------------------

    /// Returns the total token count across all stored messages.
    [[nodiscard]] std::size_t token_count() const;

private:
    std::size_t total_tokens() const;
    void trim_to_budget();

    std::vector<Message> messages_;
    std::size_t max_tokens_;
    std::shared_ptr<ITokenizer> tokenizer_;
};

} // namespace agentrs
