#pragma once
// in_memory.hpp — Default in-process memory backend.
// Equivalent to agentrs-memory/src/in_memory.rs

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

/// Default in-process memory backend.
///
/// Stores messages in a vector. Optionally caps to the most recent N messages.
/// Equivalent to Rust's `InMemoryMemory`.
class InMemoryMemory : public IMemory {
public:
    /// Creates an empty memory backend with no message limit.
    InMemoryMemory();

    /// Creates an in-memory backend capped to the most recent messages.
    explicit InMemoryMemory(std::size_t max_messages);

    ~InMemoryMemory() override = default;

    // Movable
    InMemoryMemory(InMemoryMemory&&) noexcept = default;
    InMemoryMemory& operator=(InMemoryMemory&&) noexcept = default;

    // Copyable
    InMemoryMemory(const InMemoryMemory&) = default;
    InMemoryMemory& operator=(const InMemoryMemory&) = default;

    // --- IMemory ------------------------------------------------------------

    void store(std::string_view key, const Message& value) override;

    [[nodiscard]] std::vector<Message> retrieve(std::string_view query,
                                                 std::size_t limit) override;

    [[nodiscard]] std::vector<Message> history() const override;

    void clear() override;

    // --- Extra (mirrors Rust SearchableMemory) ------------------------------

    /// Returns the approximate token count for the current history.
    /// Heuristic: tokens ≈ text.length() / 4
    [[nodiscard]] std::size_t token_count() const;

private:
    void trim();

    std::vector<Message> messages_;
    std::optional<std::size_t> max_messages_;
};

} // namespace agentrs
