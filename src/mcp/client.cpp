// client.cpp -- MCP client implementations (stdio and HTTP transports).
// Equivalent to agentrs-mcp/src/client.rs

#include "agentrs/mcp/client.hpp"
#include "agentrs/mcp/adapter.hpp"

#include <algorithm>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#include <httplib.h>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else
#  include <csignal>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

namespace agentrs {

// ===========================================================================
// WebMcpOptions
// ===========================================================================

WebMcpOptions& WebMcpOptions::api_key(const std::string& key) {
    api_key_ = key;
    return *this;
}

WebMcpOptions& WebMcpOptions::api_key_header(const std::string& header_name) {
    api_key_header_ = header_name;
    return *this;
}

WebMcpOptions& WebMcpOptions::api_key_prefix(const std::string& prefix) {
    api_key_prefix_ = prefix;
    return *this;
}

WebMcpOptions& WebMcpOptions::bearer_auth(const std::string& token) {
    api_key_ = token;
    api_key_header_ = "Authorization";
    api_key_prefix_ = "Bearer ";
    return *this;
}

WebMcpOptions& WebMcpOptions::header(const std::string& name, const std::string& value) {
    headers_[name] = value;
    return *this;
}

WebMcpOptions& WebMcpOptions::headers(const std::map<std::string, std::string>& hdrs) {
    for (const auto& [k, v] : hdrs) {
        headers_[k] = v;
    }
    return *this;
}

std::map<std::string, std::string> WebMcpOptions::into_headers() const {
    auto result = headers_;

    if (api_key_.has_value()) {
        const auto& key = api_key_.value();
        std::string header_name = api_key_header_.value_or("Authorization");
        std::string header_value;

        if (api_key_prefix_.has_value()) {
            header_value = api_key_prefix_.value() + key;
        } else {
            // Check if header is Authorization (case-insensitive).
            std::string lower_name = header_name;
            std::transform(lower_name.begin(), lower_name.end(),
                           lower_name.begin(), ::tolower);
            if (lower_name == "authorization") {
                header_value = "Bearer " + key;
            } else {
                header_value = key;
            }
        }

        // Only insert if not already present.
        if (result.find(header_name) == result.end()) {
            result[header_name] = header_value;
        }
    }

    return result;
}

// ===========================================================================
// StdioTransport
// ===========================================================================

#ifdef _WIN32

// --- Windows implementation using CreateProcess + pipes ---

StdioTransport::~StdioTransport() {
    kill();
}

std::unique_ptr<StdioTransport> StdioTransport::spawn(const std::string& cmd) {
    auto transport = std::make_unique<StdioTransport>();
    transport->command = cmd;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
    sa.bInheritHandle = TRUE;
    sa.lpSecurityDescriptor = nullptr;

    HANDLE child_stdin_read  = nullptr;
    HANDLE child_stdin_write = nullptr;
    HANDLE child_stdout_read = nullptr;
    HANDLE child_stdout_write = nullptr;

    if (!CreatePipe(&child_stdin_read, &child_stdin_write, &sa, 0)) {
        throw McpSpawnFailed("failed to create stdin pipe");
    }
    SetHandleInformation(child_stdin_write, HANDLE_FLAG_INHERIT, 0);

    if (!CreatePipe(&child_stdout_read, &child_stdout_write, &sa, 0)) {
        CloseHandle(child_stdin_read);
        CloseHandle(child_stdin_write);
        throw McpSpawnFailed("failed to create stdout pipe");
    }
    SetHandleInformation(child_stdout_read, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = child_stdin_read;
    si.hStdOutput = child_stdout_write;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);

    ZeroMemory(&pi, sizeof(pi));

    // CreateProcessA needs a mutable char buffer.
    std::string cmd_copy = cmd;
    if (!CreateProcessA(
            nullptr,
            &cmd_copy[0],
            nullptr,
            nullptr,
            TRUE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        CloseHandle(child_stdin_read);
        CloseHandle(child_stdin_write);
        CloseHandle(child_stdout_read);
        CloseHandle(child_stdout_write);
        throw McpSpawnFailed("CreateProcess failed for: " + cmd);
    }

    // Close the child-side handles (parent doesn't need them).
    CloseHandle(child_stdin_read);
    CloseHandle(child_stdout_write);
    CloseHandle(pi.hThread);

    transport->process_handle = pi.hProcess;
    transport->stdin_write    = child_stdin_write;
    transport->stdout_read    = child_stdout_read;

    return transport;
}

void StdioTransport::write_line(const std::string& data) {
    if (!stdin_write) throw McpProtocolError("stdin pipe is closed");

    std::string payload = data + "\n";
    DWORD written = 0;
    if (!WriteFile(static_cast<HANDLE>(stdin_write),
                   payload.data(),
                   static_cast<DWORD>(payload.size()),
                   &written,
                   nullptr)) {
        throw McpProtocolError("failed to write to child stdin");
    }
    FlushFileBuffers(static_cast<HANDLE>(stdin_write));
}

std::string StdioTransport::read_line() {
    if (!stdout_read) throw McpProtocolError("stdout pipe is closed");

    std::string line;
    char ch = 0;
    DWORD bytes_read = 0;

    while (true) {
        if (!ReadFile(static_cast<HANDLE>(stdout_read),
                      &ch, 1, &bytes_read, nullptr) || bytes_read == 0) {
            if (line.empty()) {
                throw McpProtocolError("child process closed stdout");
            }
            break;
        }
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }

    return line;
}

void StdioTransport::kill() {
    if (stdin_write) {
        CloseHandle(static_cast<HANDLE>(stdin_write));
        stdin_write = nullptr;
    }
    if (stdout_read) {
        CloseHandle(static_cast<HANDLE>(stdout_read));
        stdout_read = nullptr;
    }
    if (process_handle) {
        TerminateProcess(static_cast<HANDLE>(process_handle), 1);
        CloseHandle(static_cast<HANDLE>(process_handle));
        process_handle = nullptr;
    }
}

#else // POSIX

// --- POSIX implementation using fork + pipe ---

StdioTransport::~StdioTransport() {
    kill();
}

std::unique_ptr<StdioTransport> StdioTransport::spawn(const std::string& cmd) {
    auto transport = std::make_unique<StdioTransport>();
    transport->command = cmd;

    int stdin_pipe[2];   // parent writes -> child reads
    int stdout_pipe[2];  // child writes -> parent reads

    if (::pipe(stdin_pipe) != 0) {
        throw McpSpawnFailed("failed to create stdin pipe");
    }
    if (::pipe(stdout_pipe) != 0) {
        ::close(stdin_pipe[0]);
        ::close(stdin_pipe[1]);
        throw McpSpawnFailed("failed to create stdout pipe");
    }

    pid_t pid = ::fork();
    if (pid < 0) {
        ::close(stdin_pipe[0]);
        ::close(stdin_pipe[1]);
        ::close(stdout_pipe[0]);
        ::close(stdout_pipe[1]);
        throw McpSpawnFailed("fork failed");
    }

    if (pid == 0) {
        // Child process.
        ::close(stdin_pipe[1]);   // close write end of stdin pipe
        ::close(stdout_pipe[0]);  // close read end of stdout pipe

        ::dup2(stdin_pipe[0], STDIN_FILENO);
        ::dup2(stdout_pipe[1], STDOUT_FILENO);

        // Redirect stderr to /dev/null.
        int devnull = ::open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            ::dup2(devnull, STDERR_FILENO);
            ::close(devnull);
        }

        ::close(stdin_pipe[0]);
        ::close(stdout_pipe[1]);

        ::execl("/bin/sh", "sh", "-c", cmd.c_str(), nullptr);
        ::_exit(127);
    }

    // Parent process.
    ::close(stdin_pipe[0]);
    ::close(stdout_pipe[1]);

    transport->child_pid   = pid;
    transport->stdin_write  = stdin_pipe[1];
    transport->stdout_read  = stdout_pipe[0];

    return transport;
}

void StdioTransport::write_line(const std::string& data) {
    if (stdin_write < 0) throw McpProtocolError("stdin pipe is closed");

    std::string payload = data + "\n";
    ssize_t total = 0;
    while (total < static_cast<ssize_t>(payload.size())) {
        ssize_t n = ::write(stdin_write,
                            payload.data() + total,
                            payload.size() - static_cast<size_t>(total));
        if (n <= 0) throw McpProtocolError("failed to write to child stdin");
        total += n;
    }
}

std::string StdioTransport::read_line() {
    if (stdout_read < 0) throw McpProtocolError("stdout pipe is closed");

    std::string line;
    char ch = 0;
    while (true) {
        ssize_t n = ::read(stdout_read, &ch, 1);
        if (n <= 0) {
            if (line.empty()) {
                throw McpProtocolError("child process closed stdout");
            }
            break;
        }
        if (ch == '\n') break;
        if (ch != '\r') line += ch;
    }
    return line;
}

void StdioTransport::kill() {
    if (stdin_write >= 0) {
        ::close(stdin_write);
        stdin_write = -1;
    }
    if (stdout_read >= 0) {
        ::close(stdout_read);
        stdout_read = -1;
    }
    if (child_pid > 0) {
        ::kill(child_pid, SIGTERM);
        ::waitpid(child_pid, nullptr, WNOHANG);
        child_pid = -1;
    }
}

#endif // _WIN32 / POSIX

// ===========================================================================
// HttpTransport
// ===========================================================================

/// Splits a URL into scheme+host and path components for httplib::Client.
static void parse_endpoint(const std::string& endpoint,
                           std::string& base_url,
                           std::string& path) {
    // Find the scheme separator.
    auto scheme_end = endpoint.find("://");
    if (scheme_end == std::string::npos) {
        // No scheme -- assume http.
        auto slash = endpoint.find('/');
        if (slash == std::string::npos) {
            base_url = "http://" + endpoint;
            path = "/";
        } else {
            base_url = "http://" + endpoint.substr(0, slash);
            path = endpoint.substr(slash);
        }
        return;
    }

    // After scheme://
    auto after_scheme = scheme_end + 3;
    auto slash = endpoint.find('/', after_scheme);
    if (slash == std::string::npos) {
        base_url = endpoint;
        path = "/";
    } else {
        base_url = endpoint.substr(0, slash);
        path = endpoint.substr(slash);
    }
}

McpMessage HttpTransport::send_request(const McpMessage& message) {
    std::string base_url;
    std::string path;
    parse_endpoint(endpoint, base_url, path);

    httplib::Client cli(base_url);
    cli.set_follow_location(true);
    cli.set_connection_timeout(30, 0);
    cli.set_read_timeout(120, 0);

    // Build headers.
    httplib::Headers hdrs;
    hdrs.emplace("Content-Type", "application/json");
    hdrs.emplace("charset", "utf-8");
    hdrs.emplace("Accept", "application/json, text/event-stream");
    hdrs.emplace("MCP-Protocol-Version", MCP_PROTOCOL_VERSION);

    if (session_id.has_value()) {
        hdrs.emplace("Mcp-Session-Id", session_id.value());
    }

    for (const auto& [name, value] : headers) {
        hdrs.emplace(name, value);
    }

    nlohmann::json body_json = message;
    std::string body_str = body_json.dump();

    auto res = cli.Post(path, hdrs, body_str, "application/json");
    if (!res) {
        throw McpProtocolError("HTTP request failed: " +
                               httplib::to_string(res.error()));
    }

    // Capture Mcp-Session-Id from response.
    auto it = res->headers.find("Mcp-Session-Id");
    if (it != res->headers.end()) {
        session_id = it->second;
    }

    if (res->status < 200 || res->status >= 300) {
        throw McpResponseError("HTTP " + std::to_string(res->status) +
                               ": " + res->body);
    }

    // Check content type for SSE.
    std::string content_type;
    auto ct_it = res->headers.find("Content-Type");
    if (ct_it != res->headers.end()) {
        content_type = ct_it->second;
    }

    if (content_type.find("text/event-stream") != std::string::npos) {
        return parse_sse_response(res->body, message.id);
    }

    // Parse as plain JSON-RPC response.
    McpMessage response;
    nlohmann::json response_json = nlohmann::json::parse(res->body);
    response = response_json.get<McpMessage>();
    return response;
}

// ===========================================================================
// McpClient
// ===========================================================================

McpClient::~McpClient() {
    if (transport_type_ == TransportType::Stdio && stdio_) {
        stdio_->kill();
    }
}

McpClient::McpClient(McpClient&& other) noexcept
    : transport_type_(other.transport_type_),
      stdio_(std::move(other.stdio_)),
      http_(std::move(other.http_)),
      next_id_(other.next_id_.load()) {}

McpClient& McpClient::operator=(McpClient&& other) noexcept {
    if (this != &other) {
        transport_type_ = other.transport_type_;
        stdio_ = std::move(other.stdio_);
        http_ = std::move(other.http_);
        next_id_.store(other.next_id_.load());
    }
    return *this;
}

McpClient McpClient::spawn(const std::string& command) {
    if (command.empty()) {
        throw McpInvalidCommand();
    }

    McpClient client;
    client.transport_type_ = TransportType::Stdio;
    client.stdio_ = StdioTransport::spawn(command);
    client.initialize();
    return client;
}

McpClient McpClient::connect(const std::string& endpoint) {
    return connect_with_options(endpoint, WebMcpOptions());
}

McpClient McpClient::connect_with_headers(
    const std::string& endpoint,
    const std::map<std::string, std::string>& headers) {
    WebMcpOptions opts;
    opts.headers(headers);
    return connect_with_options(endpoint, opts);
}

McpClient McpClient::connect_with_api_key(
    const std::string& endpoint,
    const std::string& api_key) {
    WebMcpOptions opts;
    opts.api_key(api_key);
    return connect_with_options(endpoint, opts);
}

McpClient McpClient::connect_with_options(
    const std::string& endpoint,
    const WebMcpOptions& options) {

    McpClient client;
    client.transport_type_ = TransportType::Http;
    client.http_ = std::make_unique<HttpTransport>();
    client.http_->endpoint = endpoint;
    client.http_->headers = options.into_headers();
    client.initialize();
    return client;
}

std::vector<McpTool> McpClient::list_tools() {
    nlohmann::json response = call_method("tools/list", nlohmann::json::object());

    if (!response.contains("tools")) {
        throw McpProtocolError("missing tools field");
    }

    return response["tools"].get<std::vector<McpTool>>();
}

ToolOutput McpClient::call_tool(
    const std::string& name,
    const nlohmann::json& arguments) {

    nlohmann::json params = {
        {"name", name},
        {"arguments", arguments}
    };

    nlohmann::json response = call_method("tools/call", params);

    McpCallToolResult result = response.get<McpCallToolResult>();
    return result.into_tool_output();
}

void McpClient::initialize() {
    nlohmann::json params = {
        {"protocolVersion", MCP_PROTOCOL_VERSION},
        {"capabilities", {{"tools", nlohmann::json::object()}}},
        {"clientInfo", {
            {"name", "agentrs"},
            {"version", AGENTRS_VERSION}
        }}
    };

    (void)call_method("initialize", params);
}

nlohmann::json McpClient::call_method(
    const std::string& method,
    const nlohmann::json& params) {

    uint64_t id = next_id_.fetch_add(1, std::memory_order_relaxed);

    McpMessage message;
    message.jsonrpc = "2.0";
    message.id = id;
    message.method = method;
    message.params = params;

    McpMessage response;

    if (transport_type_ == TransportType::Stdio && stdio_) {
        // Serialize and send via stdio.
        nlohmann::json msg_json = message;
        stdio_->write_line(msg_json.dump());

        std::string line = stdio_->read_line();
        if (line.empty()) {
            throw McpProtocolError("empty MCP response");
        }

        nlohmann::json resp_json = nlohmann::json::parse(line);
        response = resp_json.get<McpMessage>();

    } else if (transport_type_ == TransportType::Http && http_) {
        response = http_->send_request(message);

    } else {
        throw McpProtocolError("no transport configured");
    }

    if (response.error.has_value()) {
        throw McpResponseError(response.error.value().dump());
    }

    if (!response.result.has_value()) {
        throw McpProtocolError("missing MCP result");
    }

    return response.result.value();
}

// ===========================================================================
// Free functions -- convenience wrappers
// ===========================================================================

/// Internal helper: converts an McpClient + tools into ITool instances.
static std::vector<std::shared_ptr<ITool>> wrap_mcp_tools(
    McpClient&& raw_client) {

    auto client = std::make_shared<McpClient>(std::move(raw_client));
    auto mutex = std::make_shared<std::mutex>();

    std::vector<McpTool> tools;
    {
        std::lock_guard<std::mutex> lock(*mutex);
        tools = client->list_tools();
    }

    std::vector<std::shared_ptr<ITool>> result;
    result.reserve(tools.size());
    for (auto& tool : tools) {
        result.push_back(
            std::make_shared<McpToolAdapter>(mutex, client, std::move(tool)));
    }
    return result;
}

std::vector<std::shared_ptr<ITool>> spawn_mcp_tools(const std::string& command) {
    return wrap_mcp_tools(McpClient::spawn(command));
}

std::vector<std::shared_ptr<ITool>> connect_mcp_tools(const std::string& endpoint) {
    return wrap_mcp_tools(McpClient::connect(endpoint));
}

std::vector<std::shared_ptr<ITool>> connect_mcp_tools_with_headers(
    const std::string& endpoint,
    const std::map<std::string, std::string>& headers) {
    return wrap_mcp_tools(McpClient::connect_with_headers(endpoint, headers));
}

std::vector<std::shared_ptr<ITool>> connect_mcp_tools_with_api_key(
    const std::string& endpoint,
    const std::string& api_key) {
    return wrap_mcp_tools(McpClient::connect_with_api_key(endpoint, api_key));
}

std::vector<std::shared_ptr<ITool>> connect_mcp_tools_with_options(
    const std::string& endpoint,
    const WebMcpOptions& options) {
    return wrap_mcp_tools(McpClient::connect_with_options(endpoint, options));
}

// ===========================================================================
// SSE helpers
// ===========================================================================

bool matches_expected_id(
    const McpMessage& message,
    std::optional<uint64_t> expected_id) {

    if (!expected_id.has_value()) return true;
    return message.id.has_value() && message.id.value() == expected_id.value();
}

std::optional<McpMessage> extract_response_message(
    const std::string& payload,
    std::optional<uint64_t> expected_id) {

    std::string trimmed = payload;
    // Trim leading/trailing whitespace.
    auto start = trimmed.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return std::nullopt;
    trimmed = trimmed.substr(start);
    auto end = trimmed.find_last_not_of(" \t\n\r");
    if (end != std::string::npos) trimmed = trimmed.substr(0, end + 1);

    if (trimmed.empty() || trimmed == "[DONE]") {
        return std::nullopt;
    }

    nlohmann::json value = nlohmann::json::parse(trimmed);

    if (value.is_array()) {
        for (const auto& item : value) {
            McpMessage message = item.get<McpMessage>();
            if (matches_expected_id(message, expected_id)) {
                return message;
            }
        }
        return std::nullopt;
    }

    McpMessage message = value.get<McpMessage>();
    if (matches_expected_id(message, expected_id)) {
        return message;
    }
    return std::nullopt;
}

McpMessage parse_sse_response(
    const std::string& payload,
    std::optional<uint64_t> expected_id) {

    std::string event_data;
    std::istringstream stream(payload);
    std::string line;

    while (std::getline(stream, line)) {
        // Remove trailing \r if present.
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Check for "data:" prefix.
        if (line.size() >= 5 && line.substr(0, 5) == "data:") {
            std::string data = line.substr(5);
            // Trim leading whitespace.
            auto pos = data.find_first_not_of(' ');
            if (pos != std::string::npos) {
                data = data.substr(pos);
            }
            event_data += data;
            event_data += '\n';
            continue;
        }

        // Empty line means end of event.
        std::string trimmed = line;
        auto pos = trimmed.find_first_not_of(" \t\r\n");
        if (pos == std::string::npos && !event_data.empty()) {
            // Check if event_data is non-whitespace.
            auto data_pos = event_data.find_first_not_of(" \t\r\n");
            if (data_pos != std::string::npos) {
                auto result = extract_response_message(event_data, expected_id);
                if (result.has_value()) {
                    return result.value();
                }
            }
            event_data.clear();
        }
    }

    // Try remaining data.
    if (!event_data.empty()) {
        auto result = extract_response_message(event_data, expected_id);
        if (result.has_value()) {
            return result.value();
        }
    }

    throw McpProtocolError("missing JSON-RPC response in SSE stream");
}

} // namespace agentrs
