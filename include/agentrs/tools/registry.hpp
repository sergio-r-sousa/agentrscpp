#pragma once
// registry.hpp — Tool registry for named tool lookup and management.
// Equivalent to agentrs-tools/src/registry.rs

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/mcp/client.hpp"

namespace agentrs {

// --- ToolRegistry -----------------------------------------------------------

/// Registry of named tools available to an agent.
/// Stores unique ownership of ITool instances and provides lookup by name.
///
/// Equivalent to Rust's `ToolRegistry`.
class ToolRegistry {
public:
    /// Creates an empty registry.
    ToolRegistry() = default;

    // Movable but not copyable (tools are unique_ptr).
    ToolRegistry(const ToolRegistry&) = delete;
    ToolRegistry& operator=(const ToolRegistry&) = delete;
    ToolRegistry(ToolRegistry&&) noexcept = default;
    ToolRegistry& operator=(ToolRegistry&&) noexcept = default;

    ~ToolRegistry() = default;

    /// Registers a concrete tool by moving a unique_ptr into the registry.
    /// Returns *this for chaining.
    ToolRegistry& register_tool(std::unique_ptr<ITool> tool);

    /// Looks up a tool by name. Returns nullptr when not found.
    [[nodiscard]] ITool* get(std::string_view name) const;

    /// Returns whether a tool with the given name is registered.
    [[nodiscard]] bool contains(std::string_view name) const;

    /// Returns the number of registered tools.
    [[nodiscard]] std::size_t size() const noexcept;

    /// Returns true when no tools are registered.
    [[nodiscard]] bool empty() const noexcept;

    /// Returns the names of all registered tools.
    [[nodiscard]] std::vector<std::string> list() const;

    /// Converts all registered tools to LLM-facing ToolDefinition objects.
    [[nodiscard]] std::vector<ToolDefinition> definitions() const;

    /// Calls a tool by name. Throws ToolNotFoundError if the tool does not exist.
    ToolOutput call(std::string_view name, const nlohmann::json& input);

    /// Merges another registry into this one (moves all tools from other).
    ToolRegistry& merge(ToolRegistry other);

    // -- MCP registration methods -------------------------------------------
    // These connect to remote MCP servers, discover their tools, and
    // register each one as a local McpToolAdapter (ITool).

    /// Auto-detect: if the target starts with http(s)://, connect via HTTP;
    /// otherwise spawn a stdio subprocess.
    /// Equivalent to Rust's ToolRegistry::register_mcp().
    ToolRegistry& register_mcp(const std::string& command_or_endpoint);

    /// Registers all tools exposed by a stdio MCP server subprocess.
    ToolRegistry& register_mcp_stdio(const std::string& command);

    /// Registers all tools exposed by a web MCP endpoint (no options).
    ToolRegistry& register_mcp_http(const std::string& endpoint);

    /// Registers all tools exposed by a web MCP endpoint using an API key.
    ToolRegistry& register_mcp_http_with_api_key(
        const std::string& endpoint,
        const std::string& api_key);

    /// Registers all tools exposed by a web MCP endpoint with full options.
    ToolRegistry& register_mcp_http_with_options(
        const std::string& endpoint,
        const WebMcpOptions& options = {});

private:
    std::unordered_map<std::string, std::unique_ptr<ITool>> tools_;
};

} // namespace agentrs
