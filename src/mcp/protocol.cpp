// protocol.cpp -- MCP JSON-RPC 2.0 protocol type implementations.
// Equivalent to agentrs-mcp/src/protocol.rs

#include "agentrs/mcp/protocol.hpp"

#include <string>

namespace agentrs {

// ---------------------------------------------------------------------------
// McpMessage serialization
// ---------------------------------------------------------------------------

void to_json(nlohmann::json& j, const McpMessage& m) {
    j = nlohmann::json{{"jsonrpc", m.jsonrpc}};

    if (m.id.has_value()) {
        j["id"] = m.id.value();
    }
    if (m.method.has_value()) {
        j["method"] = m.method.value();
    }
    if (m.params.has_value()) {
        j["params"] = m.params.value();
    }
    if (m.result.has_value()) {
        j["result"] = m.result.value();
    }
    if (m.error.has_value()) {
        j["error"] = m.error.value();
    }
}

void from_json(const nlohmann::json& j, McpMessage& m) {
    j.at("jsonrpc").get_to(m.jsonrpc);

    if (j.contains("id") && !j["id"].is_null()) {
        m.id = j["id"].get<uint64_t>();
    } else {
        m.id = std::nullopt;
    }

    if (j.contains("method") && !j["method"].is_null()) {
        m.method = j["method"].get<std::string>();
    } else {
        m.method = std::nullopt;
    }

    if (j.contains("params") && !j["params"].is_null()) {
        m.params = j["params"];
    } else {
        m.params = std::nullopt;
    }

    if (j.contains("result") && !j["result"].is_null()) {
        m.result = j["result"];
    } else {
        m.result = std::nullopt;
    }

    if (j.contains("error") && !j["error"].is_null()) {
        m.error = j["error"];
    } else {
        m.error = std::nullopt;
    }
}

// ---------------------------------------------------------------------------
// McpTool serialization
// ---------------------------------------------------------------------------

void to_json(nlohmann::json& j, const McpTool& t) {
    j = nlohmann::json{
        {"name", t.name},
        {"description", t.description},
        {"inputSchema", t.input_schema}
    };
}

void from_json(const nlohmann::json& j, McpTool& t) {
    j.at("name").get_to(t.name);

    if (j.contains("description")) {
        j.at("description").get_to(t.description);
    } else {
        t.description.clear();
    }

    // Support both "input_schema" and "inputSchema" (alias).
    if (j.contains("inputSchema")) {
        t.input_schema = j["inputSchema"];
    } else if (j.contains("input_schema")) {
        t.input_schema = j["input_schema"];
    } else {
        t.input_schema = nlohmann::json::object();
    }
}

// ---------------------------------------------------------------------------
// McpCallToolResult serialization
// ---------------------------------------------------------------------------

void to_json(nlohmann::json& j, const McpCallToolResult& r) {
    j = nlohmann::json{
        {"content", r.content},
        {"is_error", r.is_error}
    };
}

void from_json(const nlohmann::json& j, McpCallToolResult& r) {
    j.at("content").get_to(r.content);

    if (j.contains("is_error")) {
        j.at("is_error").get_to(r.is_error);
    } else if (j.contains("isError")) {
        j.at("isError").get_to(r.is_error);
    } else {
        r.is_error = false;
    }
}

// ---------------------------------------------------------------------------
// McpCallToolResult -> ToolOutput conversion
// ---------------------------------------------------------------------------

ToolOutput McpCallToolResult::into_tool_output() const {
    // Collect all "text" fields from content items, separated by newlines.
    std::string text;
    for (const auto& item : content) {
        if (item.contains("text") && item["text"].is_string()) {
            if (!text.empty()) {
                text += '\n';
            }
            text += item["text"].get<std::string>();
        }
    }

    ToolOutput output = ToolOutput::text(text);
    output.is_error = is_error;
    return output;
}

} // namespace agentrs
