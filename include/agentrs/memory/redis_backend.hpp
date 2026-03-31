#pragma once
// redis_backend.hpp — Redis-backed memory stub.
// Equivalent to agentrs-memory/src/redis_backend.rs
//
// This is a stub implementation that throws MemoryBackendError on all
// operations. A real implementation would depend on a Redis client library.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

/// Redis-backed persistent conversation memory (stub).
///
/// All operations throw `MemoryBackendError("Redis not configured")`.
/// Equivalent to Rust's `RedisMemory`.
class RedisMemory : public IMemory {
public:
    /// Constructs a RedisMemory stub with a session id.
    explicit RedisMemory(std::string session_id);

    ~RedisMemory() override = default;

    // Movable
    RedisMemory(RedisMemory&&) noexcept = default;
    RedisMemory& operator=(RedisMemory&&) noexcept = default;

    // Copyable
    RedisMemory(const RedisMemory&) = default;
    RedisMemory& operator=(const RedisMemory&) = default;

    // --- IMemory ------------------------------------------------------------

    void store(std::string_view key, const Message& value) override;

    [[nodiscard]] std::vector<Message> retrieve(std::string_view query,
                                                 std::size_t limit) override;

    [[nodiscard]] std::vector<Message> history() const override;

    void clear() override;

private:
    std::string session_id_;
};

} // namespace agentrs
