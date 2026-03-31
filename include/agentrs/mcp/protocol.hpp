#pragma once
// protocol.hpp -- MCP JSON-RPC 2.0 protocol types.
// Equivalent to agentrs-mcp/src/protocol.rs

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agentrs/core/types.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// McpMessage -- JSON-RPC 2.0 envelope used by the MCP protocol.
// ---------------------------------------------------------------------------

/// MCP JSON-RPC message envelope.
struct McpMessage {
    /// Protocol version -- always "2.0".
    std::string jsonrpc = "2.0";

    /// Optional request id.
    std::optional<uint64_t> id;

    /// Optional RPC method.
    std::optional<std::string> method;

    /// Optional params.
    std::optional<nlohmann::json> params;

    /// Optional result.
    std::optional<nlohmann::json> result;

    /// Optional error.
    std::optional<nlohmann::json> error;
};

void to_json(nlohmann::json& j, const McpMessage& m);
void from_json(const nlohmann::json& j, McpMessage& m);

// ---------------------------------------------------------------------------
// McpTool -- Tool metadata returned by MCP servers.
// ---------------------------------------------------------------------------

/// Tool metadata returned by MCP servers.
struct McpTool {
    /// Public tool name.
    std::string name;

    /// Tool description.
    std::string description;

    /// Input schema (JSON Schema).
    nlohmann::json input_schema;
};

void to_json(nlohmann::json& j, const McpTool& t);
void from_json(const nlohmann::json& j, McpTool& t);

// ---------------------------------------------------------------------------
// McpCallToolResult -- Result payload from tools/call.
// ---------------------------------------------------------------------------

/// Tool call result payload.
struct McpCallToolResult {
    /// Structured output content.
    std::vector<nlohmann::json> content;

    /// Whether the result is an error.
    bool is_error = false;

    /// Converts the MCP result to the SDK ToolOutput.
    [[nodiscard]] ToolOutput into_tool_output() const;
};

void to_json(nlohmann::json& j, const McpCallToolResult& r);
void from_json(const nlohmann::json& j, McpCallToolResult& r);

} // namespace agentrs
