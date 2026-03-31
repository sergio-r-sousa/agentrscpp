#pragma once
// sliding_window.hpp — Sliding-window memory backend.
// Equivalent to agentrs-memory/src/sliding_window.rs

#include <cstddef>
#include <deque>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

/// Memory backend that keeps a fixed number of recent non-system messages.
///
/// System messages are stored separately and always returned at the front of
/// the history. Non-system messages are evicted FIFO when the window is full.
/// Equivalent to Rust's `SlidingWindowMemory`.
class SlidingWindowMemory : public IMemory {
public:
    /// Creates a new sliding-window memory.
    explicit SlidingWindowMemory(std::size_t window_size);

    ~SlidingWindowMemory() override = default;

    // Movable
    SlidingWindowMemory(SlidingWindowMemory&&) noexcept = default;
    SlidingWindowMemory& operator=(SlidingWindowMemory&&) noexcept = default;

    // Copyable
    SlidingWindowMemory(const SlidingWindowMemory&) = default;
    SlidingWindowMemory& operator=(const SlidingWindowMemory&) = default;

    // --- IMemory ------------------------------------------------------------

    void store(std::string_view key, const Message& value) override;

    [[nodiscard]] std::vector<Message> retrieve(std::string_view query,
                                                 std::size_t limit) override;

    [[nodiscard]] std::vector<Message> history() const override;

    void clear() override;

    // --- Extra (mirrors Rust SearchableMemory) ------------------------------

    /// Returns the approximate token count for the current history.
    [[nodiscard]] std::size_t token_count() const;

private:
    std::optional<Message> system_message_;
    std::deque<Message> messages_;
    std::size_t window_size_;
};

} // namespace agentrs
