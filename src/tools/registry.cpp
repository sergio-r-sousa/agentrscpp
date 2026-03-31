// registry.cpp — ToolRegistry implementation.
// Equivalent to agentrs-tools/src/registry.rs

#include "agentrs/tools/registry.hpp"
#include "agentrs/mcp/adapter.hpp"

#include <memory>
#include <mutex>
#include <utility>

namespace agentrs {

ToolRegistry& ToolRegistry::register_tool(std::unique_ptr<ITool> tool) {
    if (!tool) {
        return *this;
    }
    std::string tool_name = tool->name();
    tools_[std::move(tool_name)] = std::move(tool);
    return *this;
}

ITool* ToolRegistry::get(std::string_view name) const {
    auto it = tools_.find(std::string(name));
    if (it != tools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool ToolRegistry::contains(std::string_view name) const {
    return tools_.count(std::string(name)) > 0;
}

std::size_t ToolRegistry::size() const noexcept {
    return tools_.size();
}

bool ToolRegistry::empty() const noexcept {
    return tools_.empty();
}

std::vector<std::string> ToolRegistry::list() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());
    for (const auto& [name, _] : tools_) {
        names.push_back(name);
    }
    return names;
}

std::vector<ToolDefinition> ToolRegistry::definitions() const {
    std::vector<ToolDefinition> defs;
    defs.reserve(tools_.size());
    for (const auto& [_, tool] : tools_) {
        ToolDefinition def;
        def.name = tool->name();
        def.description = tool->description();
        def.schema = tool->schema();
        defs.push_back(std::move(def));
    }
    return defs;
}

ToolOutput ToolRegistry::call(std::string_view name, const nlohmann::json& input) {
    ITool* tool = get(name);
    if (!tool) {
        throw ToolNotFoundError(std::string(name));
    }
    return tool->call(input);
}

ToolRegistry& ToolRegistry::merge(ToolRegistry other) {
    for (auto& [name, tool] : other.tools_) {
        tools_[name] = std::move(tool);
    }
    return *this;
}

// ---------------------------------------------------------------------------
// MCP registration — connect to MCP servers and register discovered tools.
// ---------------------------------------------------------------------------

/// Internal helper: creates a shared McpClient + mutex, discovers tools,
/// and registers each as a McpToolAdapter (unique_ptr<ITool>).
static void register_mcp_client_tools(
    ToolRegistry& registry,
    McpClient&& raw_client) {

    auto client = std::make_shared<McpClient>(std::move(raw_client));
    auto mtx = std::make_shared<std::mutex>();

    std::vector<McpTool> mcp_tools;
    {
        std::lock_guard<std::mutex> lock(*mtx);
        mcp_tools = client->list_tools();
    }

    for (auto& tool : mcp_tools) {
        registry.register_tool(
            std::make_unique<McpToolAdapter>(mtx, client, std::move(tool)));
    }
}

ToolRegistry& ToolRegistry::register_mcp(const std::string& command_or_endpoint) {
    // Auto-detect: HTTP endpoint vs stdio command.
    if (command_or_endpoint.rfind("http://", 0) == 0 ||
        command_or_endpoint.rfind("https://", 0) == 0) {
        return register_mcp_http(command_or_endpoint);
    }
    return register_mcp_stdio(command_or_endpoint);
}

ToolRegistry& ToolRegistry::register_mcp_stdio(const std::string& command) {
    register_mcp_client_tools(*this, McpClient::spawn(command));
    return *this;
}

ToolRegistry& ToolRegistry::register_mcp_http(const std::string& endpoint) {
    return register_mcp_http_with_options(endpoint, WebMcpOptions{});
}

ToolRegistry& ToolRegistry::register_mcp_http_with_api_key(
    const std::string& endpoint,
    const std::string& api_key) {
    WebMcpOptions opts;
    opts.api_key(api_key);
    return register_mcp_http_with_options(endpoint, opts);
}

ToolRegistry& ToolRegistry::register_mcp_http_with_options(
    const std::string& endpoint,
    const WebMcpOptions& options) {
    register_mcp_client_tools(
        *this, McpClient::connect_with_options(endpoint, options));
    return *this;
}

} // namespace agentrs
