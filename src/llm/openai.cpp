// openai.cpp — OpenAI-compatible LLM provider implementation
// Equivalent to agentrs-llm/src/openai/mod.rs

#include <httplib.h>

#include <cstdlib>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/llm/openai.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// detail helpers
// ---------------------------------------------------------------------------

namespace detail {

std::string if_empty_then(const std::string& str, const std::string& fallback) {
    return str.empty() ? fallback : str;
}

nlohmann::json map_openai_message(const Message& message) {
    std::string role = role_to_string(message.role);

    nlohmann::json value = {
        {"role", role},
        {"content", message.text_content()}
    };

    if (message.tool_calls.has_value()) {
        nlohmann::json tc_array = nlohmann::json::array();
        for (const auto& tc : message.tool_calls.value()) {
            tc_array.push_back(map_openai_tool_call(tc));
        }
        value["tool_calls"] = tc_array;
    }
    if (message.tool_call_id.has_value()) {
        value["tool_call_id"] = message.tool_call_id.value();
    }
    return value;
}

nlohmann::json map_openai_tool(const ToolDefinition& tool) {
    return {
        {"type", "function"},
        {"function", {
            {"name", tool.name},
            {"description", tool.description},
            {"parameters", tool.schema}
        }}
    };
}

nlohmann::json map_openai_tool_call(const ToolCall& tool_call) {
    return {
        {"id", tool_call.id},
        {"type", "function"},
        {"function", {
            {"name", tool_call.name},
            {"arguments", tool_call.arguments.dump()}
        }}
    };
}

Usage parse_openai_usage(const nlohmann::json& value) {
    Usage usage;
    if (value.contains("prompt_tokens") && value["prompt_tokens"].is_number()) {
        usage.input_tokens = value["prompt_tokens"].get<uint32_t>();
    }
    if (value.contains("completion_tokens") && value["completion_tokens"].is_number()) {
        usage.output_tokens = value["completion_tokens"].get<uint32_t>();
    }
    if (value.contains("total_tokens") && value["total_tokens"].is_number()) {
        usage.total_tokens = value["total_tokens"].get<uint32_t>();
    }
    return usage;
}

CompletionResponse map_openai_response(const nlohmann::json& payload) {
    // Extract the first choice
    if (!payload.contains("choices") || !payload["choices"].is_array() ||
        payload["choices"].empty()) {
        throw InvalidResponseError("missing choices");
    }

    const auto& choice = payload["choices"][0];

    if (!choice.contains("message")) {
        throw InvalidResponseError("missing message");
    }

    const auto& message_value = choice["message"];

    // Stop reason
    StopReasonValue stop_reason(StopReason::Stop);
    if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
        stop_reason = map_stop_reason(choice["finish_reason"].get<std::string>());
    }

    // Tool calls
    std::optional<std::vector<ToolCall>> tool_calls;
    if (message_value.contains("tool_calls") && message_value["tool_calls"].is_array()) {
        std::vector<ToolCall> calls;
        for (const auto& call : message_value["tool_calls"]) {
            if (!call.contains("id") || !call["id"].is_string()) continue;
            if (!call.contains("function")) continue;

            const auto& function = call["function"];
            if (!function.contains("name") || !function["name"].is_string()) continue;

            ToolCall tc;
            tc.id = call["id"].get<std::string>();
            tc.name = function["name"].get<std::string>();

            if (function.contains("arguments") && function["arguments"].is_string()) {
                std::string args_str = function["arguments"].get<std::string>();
                try {
                    tc.arguments = nlohmann::json::parse(args_str);
                } catch (...) {
                    tc.arguments = nlohmann::json::object();
                }
            } else {
                tc.arguments = nlohmann::json::object();
            }
            calls.push_back(std::move(tc));
        }
        if (!calls.empty()) {
            tool_calls = std::move(calls);
        }
    }

    // Message
    std::string content_text;
    if (message_value.contains("content") && message_value["content"].is_string()) {
        content_text = message_value["content"].get<std::string>();
    }
    Message msg = Message::assistant(content_text);
    msg.tool_calls = std::move(tool_calls);

    // Usage
    Usage usage;
    if (payload.contains("usage") && payload["usage"].is_object()) {
        usage = parse_openai_usage(payload["usage"]);
    }

    // Model
    std::string model;
    if (payload.contains("model") && payload["model"].is_string()) {
        model = payload["model"].get<std::string>();
    }

    CompletionResponse resp;
    resp.message = std::move(msg);
    resp.stop_reason = stop_reason;
    resp.usage = usage;
    resp.model = model;
    resp.raw = payload;
    return resp;
}

} // namespace detail

// ---------------------------------------------------------------------------
// OpenAiBuilder
// ---------------------------------------------------------------------------

OpenAiBuilder::OpenAiBuilder()
    : base_url_("https://api.openai.com/v1")
    , model_("gpt-4o") {}

OpenAiBuilder& OpenAiBuilder::api_key(const std::string& key) {
    api_key_ = key;
    return *this;
}

OpenAiBuilder& OpenAiBuilder::base_url(const std::string& url) {
    base_url_ = url;
    return *this;
}

OpenAiBuilder& OpenAiBuilder::model(const std::string& model) {
    model_ = model;
    return *this;
}

std::shared_ptr<OpenAiProvider> OpenAiBuilder::build() const {
    if (!api_key_.has_value()) {
        throw MissingApiKeyError("OPENAI_API_KEY");
    }
    return std::shared_ptr<OpenAiProvider>(
        new OpenAiProvider(api_key_.value(), base_url_, model_)
    );
}

// ---------------------------------------------------------------------------
// OpenAiProvider
// ---------------------------------------------------------------------------

OpenAiProvider::OpenAiProvider(const std::string& api_key,
                               const std::string& base_url,
                               const std::string& model)
    : api_key_(api_key)
    , base_url_(base_url)
    , model_(model) {}

OpenAiBuilder OpenAiProvider::from_env() {
    OpenAiBuilder builder;
    const char* key = std::getenv("OPENAI_API_KEY");
    if (key) builder.api_key_ = std::string(key);

    const char* url = std::getenv("OPENAI_BASE_URL");
    if (url) builder.base_url_ = std::string(url);

    const char* model = std::getenv("OPENAI_MODEL");
    if (model) builder.model_ = std::string(model);

    return builder;
}

OpenAiBuilder OpenAiProvider::builder() {
    return OpenAiBuilder();
}

nlohmann::json OpenAiProvider::request_body(const CompletionRequest& req, bool stream_flag) const {
    // Build messages array
    nlohmann::json messages = nlohmann::json::array();

    // Insert system message at front if present
    if (req.system.has_value()) {
        messages.push_back({
            {"role", "system"},
            {"content", req.system.value()}
        });
    }

    for (const auto& msg : req.messages) {
        messages.push_back(detail::map_openai_message(msg));
    }

    std::string effective_model = detail::if_empty_then(req.model, model_);

    nlohmann::json body = {
        {"model", effective_model},
        {"messages", messages},
        {"stream", stream_flag}
    };

    if (req.temperature.has_value()) {
        body["temperature"] = req.temperature.value();
    }
    if (req.max_tokens.has_value()) {
        body["max_tokens"] = req.max_tokens.value();
    }

    // Tools
    if (req.tools.has_value()) {
        nlohmann::json tools_arr = nlohmann::json::array();
        for (const auto& tool : req.tools.value()) {
            tools_arr.push_back(detail::map_openai_tool(tool));
        }
        body["tools"] = tools_arr;
    }

    // Extra fields
    for (const auto& [key, value] : req.extra) {
        body[key] = value;
    }

    return body;
}

/// Helper: parse URL into host and path. Supports https:// and http://.
static void parse_url(const std::string& url, std::string& scheme_host, std::string& path) {
    // Find scheme
    std::string trimmed = url;
    // Trim trailing slashes
    while (!trimmed.empty() && trimmed.back() == '/') {
        trimmed.pop_back();
    }

    std::size_t scheme_end = trimmed.find("://");
    if (scheme_end == std::string::npos) {
        scheme_host = "http://" + trimmed;
        path = "";
        return;
    }

    // Find the first '/' after the scheme
    std::size_t path_start = trimmed.find('/', scheme_end + 3);
    if (path_start == std::string::npos) {
        scheme_host = trimmed;
        path = "";
    } else {
        scheme_host = trimmed.substr(0, path_start);
        path = trimmed.substr(path_start);
    }
}

CompletionResponse OpenAiProvider::complete(const CompletionRequest& req) {
    nlohmann::json body = request_body(req, false);

    std::string scheme_host, base_path;
    parse_url(base_url_, scheme_host, base_path);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);
    
    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"charset", "utf-8"},
        {"Authorization", "Bearer " + api_key_}
    };

    std::string endpoint = base_path + "/chat/completions";
    std::string body_str = body.dump();

    auto result = client.Post(endpoint, headers, body_str, "application/json");

    if (!result) {
        throw HttpError("Request failed: " + httplib::to_string(result.error()));
    }

    if (result->status < 200 || result->status >= 300) {
        throw ApiError(static_cast<uint16_t>(result->status), result->body);
    }

    nlohmann::json payload;
    try {
        payload = nlohmann::json::parse(result->body);
    } catch (const nlohmann::json::exception& e) {
        throw ParseError(std::string("Failed to parse response JSON: ") + e.what());
    }

    return detail::map_openai_response(payload);
}

void OpenAiProvider::stream(const CompletionRequest& req, ChunkCallback callback) {
    nlohmann::json body = request_body(req, true);

    std::string scheme_host, base_path;
    parse_url(base_url_, scheme_host, base_path);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    std::string endpoint = base_path + "/chat/completions";
    std::string body_str = body.dump();

    // Use low-level send() with content_receiver for true streaming
    std::string buffer;
    bool cancelled = false;

    httplib::Request http_req;
    http_req.method = "POST";
    http_req.path = endpoint;
    http_req.headers = {
        {"Content-Type", "application/json"},
        {"charset", "utf-8"},
        {"Authorization", "Bearer " + api_key_}
    };
    http_req.body = body_str;
    http_req.content_receiver =
        [&](const char* data, size_t data_length, uint64_t /*offset*/, uint64_t /*total_length*/) -> bool {
            if (cancelled) return false;

            buffer.append(data, data_length);

            // Process complete SSE lines
            std::size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

                // Trim \r if present
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.empty()) continue;

                auto chunk_opt = parse_sse_chunk(line);
                if (chunk_opt.has_value()) {
                    if (!callback(chunk_opt.value())) {
                        cancelled = true;
                        return false;
                    }
                }
            }
            return true;
        };

    auto result = client.send(http_req);

    if (!result && !cancelled) {
        throw HttpError("Stream request failed: " + httplib::to_string(result.error()));
    }

    if (result && (result->status < 200 || result->status >= 300) && !cancelled) {
        throw ApiError(static_cast<uint16_t>(result->status), "Stream error");
    }
}

std::string OpenAiProvider::name() const {
    return "openai";
}

std::shared_ptr<ILlmProvider> OpenAiProvider::clone_provider() const {
    return std::shared_ptr<OpenAiProvider>(
        new OpenAiProvider(api_key_, base_url_, model_)
    );
}

} // namespace agentrs
