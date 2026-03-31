// adapter.cpp -- McpToolAdapter implementation.
// Equivalent to agentrs-mcp/src/adapter.rs

#include "agentrs/mcp/adapter.hpp"

#include <mutex>
#include <utility>

namespace agentrs {

McpToolAdapter::McpToolAdapter(
    std::shared_ptr<std::mutex> client_mutex,
    std::shared_ptr<McpClient> client,
    McpTool definition)
    : client_mutex_(std::move(client_mutex)),
      client_(std::move(client)),
      definition_(std::move(definition)) {}

std::string McpToolAdapter::name() const {
    return definition_.name;
}

std::string McpToolAdapter::description() const {
    return definition_.description;
}

nlohmann::json McpToolAdapter::schema() const {
    return definition_.input_schema;
}

ToolOutput McpToolAdapter::call(const nlohmann::json& input) {
    std::lock_guard<std::mutex> lock(*client_mutex_);
    return client_->call_tool(definition_.name, input);
}

} // namespace agentrs
