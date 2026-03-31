// shared_memory.cpp — Thread-safe shared state implementation.

#include "agentrs/multi/shared_memory.hpp"

namespace agentrs {

// --- SharedMemory -----------------------------------------------------------

void SharedMemory::set(const std::string& key, const nlohmann::json& value) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    data_[key] = value;
}

std::optional<nlohmann::json> SharedMemory::get(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    auto it = data_.find(key);
    if (it != data_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool SharedMemory::remove(const std::string& key) {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    return data_.erase(key) > 0;
}

std::vector<std::string> SharedMemory::keys() const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    std::vector<std::string> result;
    result.reserve(data_.size());
    for (const auto& [k, _] : data_) {
        result.push_back(k);
    }
    return result;
}

bool SharedMemory::contains(const std::string& key) const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return data_.find(key) != data_.end();
}

void SharedMemory::clear() {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    data_.clear();
}

std::size_t SharedMemory::size() const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return data_.size();
}

// --- SharedConversation -----------------------------------------------------

void SharedConversation::add(const std::string& agent_name, Message message) {
    message.metadata["agent"] = nlohmann::json(agent_name);
    std::unique_lock<std::shared_mutex> lock(mtx_);
    messages_.push_back(std::move(message));
}

std::vector<Message> SharedConversation::get_all() const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return messages_;
}

void SharedConversation::clear() {
    std::unique_lock<std::shared_mutex> lock(mtx_);
    messages_.clear();
}

std::size_t SharedConversation::size() const {
    std::shared_lock<std::shared_mutex> lock(mtx_);
    return messages_.size();
}

} // namespace agentrs
