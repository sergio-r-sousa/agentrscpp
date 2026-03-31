#pragma once
// shared_memory.hpp — Thread-safe shared state for multi-agent orchestration.
// Equivalent to agentrs-multi/src/shared_memory.rs

#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agentrs/core/types.hpp"

namespace agentrs {

// --- SharedMemory -----------------------------------------------------------

/// Thread-safe key-value store for multi-agent shared state.
/// Keys are strings, values are JSON objects. All operations are thread-safe.
class SharedMemory {
public:
    SharedMemory() = default;

    /// Sets a key-value pair.
    void set(const std::string& key, const nlohmann::json& value);

    /// Gets a value by key, or std::nullopt if not present.
    [[nodiscard]] std::optional<nlohmann::json> get(const std::string& key) const;

    /// Removes a key-value pair. Returns true if the key was present.
    bool remove(const std::string& key);

    /// Returns all keys.
    [[nodiscard]] std::vector<std::string> keys() const;

    /// Returns true if the key exists.
    [[nodiscard]] bool contains(const std::string& key) const;

    /// Clears all entries.
    void clear();

    /// Returns the number of entries.
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::shared_mutex mtx_;
    std::map<std::string, nlohmann::json> data_;
};

// --- SharedConversation -----------------------------------------------------

/// Shared conversation state used by multiple agents.
/// Equivalent to Rust's SharedConversation.
class SharedConversation {
public:
    SharedConversation() = default;

    /// Appends a message tagged with its source agent.
    void add(const std::string& agent_name, Message message);

    /// Returns the full shared conversation.
    [[nodiscard]] std::vector<Message> get_all() const;

    /// Clears all messages.
    void clear();

    /// Returns the number of messages.
    [[nodiscard]] std::size_t size() const;

private:
    mutable std::shared_mutex mtx_;
    std::vector<Message> messages_;
};

} // namespace agentrs
