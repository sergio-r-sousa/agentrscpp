// anthropic.cpp — Anthropic LLM provider implementation
// Equivalent to agentrs-llm/src/anthropic/mod.rs

#include <httplib.h>

#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/llm/anthropic.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// AnthropicBuilder
// ---------------------------------------------------------------------------

AnthropicBuilder::AnthropicBuilder()
    : base_url_("https://api.anthropic.com/v1")
    , model_("claude-3-5-sonnet-latest") {}

AnthropicBuilder& AnthropicBuilder::api_key(const std::string& key) {
    api_key_ = key;
    return *this;
}

AnthropicBuilder& AnthropicBuilder::base_url(const std::string& url) {
    base_url_ = url;
    return *this;
}

AnthropicBuilder& AnthropicBuilder::model(const std::string& model) {
    model_ = model;
    return *this;
}

std::shared_ptr<AnthropicProvider> AnthropicBuilder::build() const {
    if (!api_key_.has_value()) {
        throw MissingApiKeyError("ANTHROPIC_API_KEY");
    }
    return std::shared_ptr<AnthropicProvider>(
        new AnthropicProvider(api_key_.value(), base_url_, model_)
    );
}

// ---------------------------------------------------------------------------
// AnthropicProvider
// ---------------------------------------------------------------------------

AnthropicProvider::AnthropicProvider(const std::string& api_key,
                                     const std::string& base_url,
                                     const std::string& model)
    : api_key_(api_key)
    , base_url_(base_url)
    , model_(model) {}

AnthropicBuilder AnthropicProvider::from_env() {
    AnthropicBuilder builder;
    const char* key = std::getenv("ANTHROPIC_API_KEY");
    if (key) builder.api_key_ = std::string(key);

    const char* url = std::getenv("ANTHROPIC_BASE_URL");
    if (url) builder.base_url_ = std::string(url);

    const char* model = std::getenv("ANTHROPIC_MODEL");
    if (model) builder.model_ = std::string(model);

    return builder;
}

AnthropicBuilder AnthropicProvider::builder() {
    return AnthropicBuilder();
}

/// Helper: parse URL into host and path.
static void parse_url(const std::string& url, std::string& scheme_host, std::string& path) {
    std::string trimmed = url;
    while (!trimmed.empty() && trimmed.back() == '/') {
        trimmed.pop_back();
    }

    std::size_t scheme_end = trimmed.find("://");
    if (scheme_end == std::string::npos) {
        scheme_host = "http://" + trimmed;
        path = "";
        return;
    }

    std::size_t path_start = trimmed.find('/', scheme_end + 3);
    if (path_start == std::string::npos) {
        scheme_host = trimmed;
        path = "";
    } else {
        scheme_host = trimmed.substr(0, path_start);
        path = trimmed.substr(path_start);
    }
}

CompletionResponse AnthropicProvider::complete(const CompletionRequest& req) {
    // Build Anthropic messages format
    nlohmann::json messages = nlohmann::json::array();
    std::string system_text = req.system.value_or("");

    for (const auto& msg : req.messages) {
        if (msg.role == Role::System) {
            if (!system_text.empty()) {
                system_text += "\n";
            }
            system_text += msg.text_content();
            continue;
        }

        std::string role;
        switch (msg.role) {
            case Role::User:      role = "user"; break;
            case Role::Assistant: role = "assistant"; break;
            case Role::Tool:      role = "user"; break;
            default:              role = "user"; break;
        }

        messages.push_back({
            {"role", role},
            {"content", nlohmann::json::array({{
                {"type", "text"},
                {"text", msg.text_content()}
            }})}
        });
    }

    // Build tools array
    nlohmann::json tools_json = nlohmann::json();
    if (req.tools.has_value()) {
        tools_json = nlohmann::json::array();
        for (const auto& tool : req.tools.value()) {
            tools_json.push_back({
                {"name", tool.name},
                {"description", tool.description},
                {"input_schema", tool.schema}
            });
        }
    }

    std::string effective_model = req.model.empty() ? model_ : req.model;
    uint32_t max_tokens = req.max_tokens.value_or(2048);

    nlohmann::json body = {
        {"model", effective_model},
        {"messages", messages},
        {"system", system_text},
        {"max_tokens", max_tokens}
    };

    if (req.temperature.has_value()) {
        body["temperature"] = req.temperature.value();
    }

    if (!tools_json.is_null()) {
        body["tools"] = tools_json;
    }

    // HTTP request
    std::string scheme_host, base_path;
    parse_url(base_url_, scheme_host, base_path);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    httplib::Headers headers = {
        {"Content-Type", "application/json"},
        {"x-api-key", api_key_},
        {"anthropic-version", "2023-06-01"}
    };

    std::string endpoint = base_path + "/messages";
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

    // Parse Anthropic response
    std::string text;
    std::vector<ToolCall> tool_calls;

    if (payload.contains("content") && payload["content"].is_array()) {
        for (const auto& item : payload["content"]) {
            std::string item_type;
            if (item.contains("type") && item["type"].is_string()) {
                item_type = item["type"].get<std::string>();
            }

            if (item_type == "text") {
                if (item.contains("text") && item["text"].is_string()) {
                    text += item["text"].get<std::string>();
                }
            } else if (item_type == "tool_use") {
                ToolCall tc;
                if (item.contains("id") && item["id"].is_string()) {
                    tc.id = item["id"].get<std::string>();
                }
                if (item.contains("name") && item["name"].is_string()) {
                    tc.name = item["name"].get<std::string>();
                }
                if (item.contains("input")) {
                    tc.arguments = item["input"];
                } else {
                    tc.arguments = nlohmann::json::object();
                }
                tool_calls.push_back(std::move(tc));
            }
        }
    }

    Message msg = Message::assistant(text);
    if (!tool_calls.empty()) {
        msg.tool_calls = std::move(tool_calls);
    }

    // Stop reason
    StopReasonValue stop_reason(StopReason::Stop);
    if (payload.contains("stop_reason") && payload["stop_reason"].is_string()) {
        stop_reason = map_stop_reason(payload["stop_reason"].get<std::string>());
    }

    // Usage
    Usage usage;
    if (payload.contains("usage") && payload["usage"].is_object()) {
        const auto& u = payload["usage"];
        if (u.contains("input_tokens") && u["input_tokens"].is_number()) {
            usage.input_tokens = u["input_tokens"].get<uint32_t>();
        }
        if (u.contains("output_tokens") && u["output_tokens"].is_number()) {
            usage.output_tokens = u["output_tokens"].get<uint32_t>();
        }
        usage.total_tokens = usage.input_tokens + usage.output_tokens;
    }

    // Model
    std::string response_model;
    if (payload.contains("model") && payload["model"].is_string()) {
        response_model = payload["model"].get<std::string>();
    }

    CompletionResponse resp;
    resp.message = std::move(msg);
    resp.stop_reason = stop_reason;
    resp.usage = usage;
    resp.model = response_model;
    resp.raw = payload;
    return resp;
}

void AnthropicProvider::stream(const CompletionRequest& /*req*/, ChunkCallback /*callback*/) {
    throw UnsupportedError("Anthropic streaming is not yet implemented in this SDK scaffold");
}

std::string AnthropicProvider::name() const {
    return "anthropic";
}

std::shared_ptr<ILlmProvider> AnthropicProvider::clone_provider() const {
    return std::shared_ptr<AnthropicProvider>(
        new AnthropicProvider(api_key_, base_url_, model_)
    );
}

} // namespace agentrs
