// redis_backend.cpp — RedisMemory stub implementation.
// Equivalent to agentrs-memory/src/redis_backend.rs
//
// All operations throw MemoryBackendError. A real implementation would
// depend on a Redis client library (e.g. hiredis, redis-plus-plus).

#include "agentrs/memory/redis_backend.hpp"

#include "agentrs/core/error.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

RedisMemory::RedisMemory(std::string session_id)
    : session_id_(std::move(session_id)) {}

// ---------------------------------------------------------------------------
// IMemory — all methods throw
// ---------------------------------------------------------------------------

void RedisMemory::store(std::string_view /*key*/, const Message& /*value*/) {
    throw MemoryBackendError("Redis not configured");
}

std::vector<Message> RedisMemory::retrieve(std::string_view /*query*/,
                                            std::size_t /*limit*/) {
    throw MemoryBackendError("Redis not configured");
}

std::vector<Message> RedisMemory::history() const {
    throw MemoryBackendError("Redis not configured");
}

void RedisMemory::clear() {
    throw MemoryBackendError("Redis not configured");
}

} // namespace agentrs
