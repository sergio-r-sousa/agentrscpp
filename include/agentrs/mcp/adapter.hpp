#pragma once
// adapter.hpp -- McpToolAdapter: wraps an MCP tool as an ITool.
// Equivalent to agentrs-mcp/src/adapter.rs

#include <memory>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/mcp/client.hpp"
#include "agentrs/mcp/protocol.hpp"

namespace agentrs {

/// Adapts an MCP tool definition to the local ITool interface.
///
/// The adapter holds a shared, mutex-protected McpClient and the tool
/// metadata obtained during `list_tools()`. Each `call()` invocation
/// locks the client and forwards the request to the MCP server.
class McpToolAdapter : public ITool {
public:
    /// Creates a new adapter.
    ///
    /// @param client  Shared, mutex-protected MCP client.
    /// @param definition  Tool metadata from the MCP server.
    McpToolAdapter(
        std::shared_ptr<std::mutex> client_mutex,
        std::shared_ptr<McpClient> client,
        McpTool definition);

    ~McpToolAdapter() override = default;

    // -- ITool interface ------------------------------------------------------

    /// Returns the public tool name.
    [[nodiscard]] std::string name() const override;

    /// Returns a short tool description.
    [[nodiscard]] std::string description() const override;

    /// Returns a JSON schema describing the tool input.
    [[nodiscard]] nlohmann::json schema() const override;

    /// Executes the tool by forwarding the call to the MCP server.
    ToolOutput call(const nlohmann::json& input) override;

private:
    std::shared_ptr<std::mutex> client_mutex_;
    std::shared_ptr<McpClient> client_;
    McpTool definition_;
};

} // namespace agentrs
