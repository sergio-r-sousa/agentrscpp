#pragma once
// communication.hpp — Inter-agent messaging and event bus.
// Equivalent to agentrs-multi/src/communication.rs

#include <chrono>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "agentrs/core/types.hpp"

namespace agentrs {

// --- OrchestratorEvent ------------------------------------------------------

/// Events emitted during orchestration.
struct OrchestratorEvent {
    enum class Type {
        AgentCompleted
    };

    Type type = Type::AgentCompleted;

    /// Name of the completed agent.
    std::string agent;

    /// Output produced by the agent.
    AgentOutput output;

    /// Creates an AgentCompleted event.
    static OrchestratorEvent agent_completed(const std::string& agent_name,
                                             const AgentOutput& out) {
        OrchestratorEvent ev;
        ev.type = Type::AgentCompleted;
        ev.agent = agent_name;
        ev.output = out;
        return ev;
    }
};

// --- IEventBus --------------------------------------------------------------

/// Pluggable event bus for orchestration observability.
class IEventBus {
public:
    virtual ~IEventBus() = default;

    /// Publishes an event.
    virtual void publish(const OrchestratorEvent& event) = 0;

    /// Subscribes a listener. Returns an ID for future unsubscribe.
    virtual std::size_t subscribe(std::function<void(const OrchestratorEvent&)> listener) = 0;

    /// Unsubscribes a listener by ID.
    virtual void unsubscribe(std::size_t id) = 0;
};

// --- InMemoryBus ------------------------------------------------------------

/// In-memory broadcast event bus (thread-safe).
class InMemoryBus : public IEventBus {
public:
    explicit InMemoryBus(std::size_t capacity = 256);

    void publish(const OrchestratorEvent& event) override;
    std::size_t subscribe(std::function<void(const OrchestratorEvent&)> listener) override;
    void unsubscribe(std::size_t id) override;

private:
    struct Listener {
        std::size_t id;
        std::function<void(const OrchestratorEvent&)> callback;
    };

    std::size_t capacity_;
    std::size_t next_id_ = 0;
    mutable std::mutex mtx_;
    std::vector<Listener> listeners_;
};

// --- AgentMessage -----------------------------------------------------------

/// Message exchanged between agents via a mailbox.
struct AgentMessage {
    std::string from;
    std::string to;
    std::string content;
    std::chrono::system_clock::time_point timestamp;

    AgentMessage() : timestamp(std::chrono::system_clock::now()) {}
    AgentMessage(const std::string& from, const std::string& to, const std::string& content)
        : from(from), to(to), content(content),
          timestamp(std::chrono::system_clock::now()) {}
};

// --- Mailbox ----------------------------------------------------------------

/// Thread-safe mailbox for inter-agent messaging.
class Mailbox {
public:
    Mailbox() = default;

    /// Sends a message from one agent to another.
    void send(const std::string& from, const std::string& to, const std::string& message);

    /// Receives all pending messages for a given agent, draining its queue.
    std::vector<AgentMessage> receive(const std::string& agent_name);

    /// Peeks at pending messages for an agent without draining.
    [[nodiscard]] std::vector<AgentMessage> peek(const std::string& agent_name) const;

    /// Returns the number of pending messages for an agent.
    [[nodiscard]] std::size_t pending_count(const std::string& agent_name) const;

private:
    mutable std::mutex mtx_;
    std::map<std::string, std::deque<AgentMessage>> boxes_;
};

} // namespace agentrs
