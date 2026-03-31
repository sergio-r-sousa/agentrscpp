// communication.cpp — Inter-agent messaging and event bus implementation.

#include "agentrs/multi/communication.hpp"

#include <algorithm>

namespace agentrs {

// --- InMemoryBus ------------------------------------------------------------

InMemoryBus::InMemoryBus(std::size_t capacity) : capacity_(capacity) {}

void InMemoryBus::publish(const OrchestratorEvent& event) {
    std::lock_guard<std::mutex> lock(mtx_);
    for (auto& listener : listeners_) {
        listener.callback(event);
    }
}

std::size_t InMemoryBus::subscribe(
    std::function<void(const OrchestratorEvent&)> listener) {
    std::lock_guard<std::mutex> lock(mtx_);
    auto id = next_id_++;
    listeners_.push_back({id, std::move(listener)});
    return id;
}

void InMemoryBus::unsubscribe(std::size_t id) {
    std::lock_guard<std::mutex> lock(mtx_);
    listeners_.erase(
        std::remove_if(listeners_.begin(), listeners_.end(),
                       [id](const Listener& l) { return l.id == id; }),
        listeners_.end());
}

// --- Mailbox ----------------------------------------------------------------

void Mailbox::send(const std::string& from, const std::string& to,
                   const std::string& message) {
    std::lock_guard<std::mutex> lock(mtx_);
    boxes_[to].emplace_back(from, to, message);
}

std::vector<AgentMessage> Mailbox::receive(const std::string& agent_name) {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<AgentMessage> result;
    auto it = boxes_.find(agent_name);
    if (it != boxes_.end()) {
        result.assign(it->second.begin(), it->second.end());
        it->second.clear();
    }
    return result;
}

std::vector<AgentMessage> Mailbox::peek(const std::string& agent_name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    std::vector<AgentMessage> result;
    auto it = boxes_.find(agent_name);
    if (it != boxes_.end()) {
        result.assign(it->second.begin(), it->second.end());
    }
    return result;
}

std::size_t Mailbox::pending_count(const std::string& agent_name) const {
    std::lock_guard<std::mutex> lock(mtx_);
    auto it = boxes_.find(agent_name);
    if (it != boxes_.end()) {
        return it->second.size();
    }
    return 0;
}

} // namespace agentrs
