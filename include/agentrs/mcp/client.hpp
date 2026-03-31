#pragma once
// client.hpp -- MCP client implementations (stdio and HTTP transports).
// Equivalent to agentrs-mcp/src/client.rs

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/mcp/protocol.hpp"

namespace agentrs {

/// MCP protocol version supported by this client.
constexpr const char* MCP_PROTOCOL_VERSION = "2025-03-26";

/// Library version string.
constexpr const char* AGENTRS_VERSION = "0.1.0";

// ---------------------------------------------------------------------------
// WebMcpOptions -- Configuration for connecting to a web MCP endpoint.
// ---------------------------------------------------------------------------

/// Configuration for connecting to a web MCP endpoint.
class WebMcpOptions {
public:
    WebMcpOptions() = default;

    /// Adds an API key using the default `Authorization: Bearer <token>` header.
    WebMcpOptions& api_key(const std::string& key);

    /// Overrides the header used for the API key.
    WebMcpOptions& api_key_header(const std::string& header_name);

    /// Overrides the prefix applied to the API key value.
    WebMcpOptions& api_key_prefix(const std::string& prefix);

    /// Sets bearer authorization explicitly.
    WebMcpOptions& bearer_auth(const std::string& token);

    /// Adds a custom HTTP header.
    WebMcpOptions& header(const std::string& name, const std::string& value);

    /// Adds multiple custom HTTP headers.
    WebMcpOptions& headers(const std::map<std::string, std::string>& hdrs);

    /// Resolves API key settings and returns final header map.
    [[nodiscard]] std::map<std::string, std::string> into_headers() const;

private:
    std::map<std::string, std::string> headers_;
    std::optional<std::string> api_key_;
    std::optional<std::string> api_key_header_;
    std::optional<std::string> api_key_prefix_;
};

// ---------------------------------------------------------------------------
// StdioTransport -- Subprocess pipe transport.
// ---------------------------------------------------------------------------

/// Internal: Manages a child process via pipe-based stdio.
struct StdioTransport {
    FILE* pipe = nullptr;     ///< Bidirectional pipe handle (popen).
    std::string command;      ///< Original command string.

    // For the real implementation we need separate read/write handles.
    // On POSIX we use pipe+fork; on Windows we use CreateProcess.
    // This simplified version stores child process handles.
#ifdef _WIN32
    void* process_handle = nullptr;
    void* stdin_write    = nullptr;
    void* stdout_read    = nullptr;
#else
    int child_pid   = -1;
    int stdin_write  = -1;
    int stdout_read  = -1;
#endif

    ~StdioTransport();

    /// Spawn a child process with piped stdin/stdout.
    static std::unique_ptr<StdioTransport> spawn(const std::string& command);

    /// Write a line to the child's stdin.
    void write_line(const std::string& data);

    /// Read a line from the child's stdout.
    [[nodiscard]] std::string read_line();

    /// Kill the child process.
    void kill();
};

// ---------------------------------------------------------------------------
// HttpTransport -- Streamable HTTP transport via cpp-httplib.
// ---------------------------------------------------------------------------

/// Internal: Manages an HTTP-based MCP connection.
struct HttpTransport {
    std::string endpoint;
    std::map<std::string, std::string> headers;
    std::optional<std::string> session_id;

    /// Sends a JSON-RPC request and returns the response.
    McpMessage send_request(const McpMessage& message);
};

// ---------------------------------------------------------------------------
// McpClient -- Unified MCP client (stdio or HTTP).
// ---------------------------------------------------------------------------

/// MCP client over stdio or Streamable HTTP transport.
class McpClient {
public:
    ~McpClient();

    // Non-copyable, movable.
    McpClient(const McpClient&) = delete;
    McpClient& operator=(const McpClient&) = delete;
    McpClient(McpClient&&) noexcept;
    McpClient& operator=(McpClient&&) noexcept;

    /// Spawns an MCP server process and performs the initialize handshake.
    static McpClient spawn(const std::string& command);

    /// Connects to a web MCP endpoint using Streamable HTTP.
    static McpClient connect(const std::string& endpoint);

    /// Connects with custom headers.
    static McpClient connect_with_headers(
        const std::string& endpoint,
        const std::map<std::string, std::string>& headers);

    /// Connects using an API key.
    static McpClient connect_with_api_key(
        const std::string& endpoint,
        const std::string& api_key);

    /// Connects with full web options.
    static McpClient connect_with_options(
        const std::string& endpoint,
        const WebMcpOptions& options);

    /// Lists all tools exported by the server.
    [[nodiscard]] std::vector<McpTool> list_tools();

    /// Calls a server-side tool.
    [[nodiscard]] ToolOutput call_tool(
        const std::string& name,
        const nlohmann::json& arguments);

private:
    McpClient() = default;

    /// Performs the MCP initialize handshake.
    void initialize();

    /// Sends a JSON-RPC method call and returns the result value.
    [[nodiscard]] nlohmann::json call_method(
        const std::string& method,
        const nlohmann::json& params);

    enum class TransportType { Stdio, Http };
    TransportType transport_type_ = TransportType::Stdio;
    std::unique_ptr<StdioTransport> stdio_;
    std::unique_ptr<HttpTransport> http_;
    std::atomic<uint64_t> next_id_{1};
};

// ---------------------------------------------------------------------------
// Free functions -- convenience wrappers matching the Rust API.
// ---------------------------------------------------------------------------

/// Spawns an MCP server and converts all exposed tools to ITool instances.
std::vector<std::shared_ptr<ITool>> spawn_mcp_tools(const std::string& command);

/// Connects to a web MCP endpoint and converts all exposed tools.
std::vector<std::shared_ptr<ITool>> connect_mcp_tools(const std::string& endpoint);

/// Connects with custom headers and converts all exposed tools.
std::vector<std::shared_ptr<ITool>> connect_mcp_tools_with_headers(
    const std::string& endpoint,
    const std::map<std::string, std::string>& headers);

/// Connects with an API key and converts all exposed tools.
std::vector<std::shared_ptr<ITool>> connect_mcp_tools_with_api_key(
    const std::string& endpoint,
    const std::string& api_key);

/// Connects with full web options and converts all exposed tools.
std::vector<std::shared_ptr<ITool>> connect_mcp_tools_with_options(
    const std::string& endpoint,
    const WebMcpOptions& options);

// ---------------------------------------------------------------------------
// SSE helpers (internal, declared here for testability).
// ---------------------------------------------------------------------------

/// Parses an SSE response body and extracts the matching JSON-RPC message.
McpMessage parse_sse_response(
    const std::string& payload,
    std::optional<uint64_t> expected_id);

/// Extracts a response message from a data block, optionally filtering by id.
std::optional<McpMessage> extract_response_message(
    const std::string& payload,
    std::optional<uint64_t> expected_id);

/// Returns true when the message id matches the expected id.
bool matches_expected_id(
    const McpMessage& message,
    std::optional<uint64_t> expected_id);

} // namespace agentrs
