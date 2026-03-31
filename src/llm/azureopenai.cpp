// azureopenai.cpp — Azure OpenAI LLM provider implementation
// Equivalent to agentrs-llm/src/azureopenai/mod.rs

#include <httplib.h>

#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/llm/azureopenai.hpp"
#include "agentrs/llm/openai.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// AzureOpenAiBuilder
// ---------------------------------------------------------------------------

AzureOpenAiBuilder::AzureOpenAiBuilder() = default;

AzureOpenAiBuilder& AzureOpenAiBuilder::api_key(const std::string& key) {
    api_key_ = key;
    return *this;
}

AzureOpenAiBuilder& AzureOpenAiBuilder::base_url(const std::string& url) {
    base_url_ = url;
    return *this;
}

AzureOpenAiBuilder& AzureOpenAiBuilder::model(const std::string& model) {
    model_ = model;
    return *this;
}

std::shared_ptr<AzureOpenAiProvider> AzureOpenAiBuilder::build() const {
    if (!api_key_.has_value()) {
        throw MissingApiKeyError("AZURE_OPENAI_KEY");
    }
    std::string effective_url = base_url_.value_or("https://api.azureopenai.com/v1");
    std::string effective_model = model_.value_or("gpt-4o-mini");
    return std::shared_ptr<AzureOpenAiProvider>(
        new AzureOpenAiProvider(api_key_.value(), effective_url, effective_model)
    );
}

// ---------------------------------------------------------------------------
// AzureOpenAiProvider
// ---------------------------------------------------------------------------

AzureOpenAiProvider::AzureOpenAiProvider(const std::string& api_key,
                                         const std::string& base_url,
                                         const std::string& model)
    : api_key_(api_key)
    , base_url_(base_url)
    , model_(model) {}

AzureOpenAiBuilder AzureOpenAiProvider::from_env() {
    AzureOpenAiBuilder builder;
    const char* key = std::getenv("AZURE_OPENAI_KEY");
    if (key) builder.api_key_ = std::string(key);

    const char* endpoint = std::getenv("AZURE_OPENAI_ENDPOINT");
    if (endpoint) builder.base_url_ = std::string(endpoint);

    const char* model = std::getenv("AZURE_OPENAI_MODEL");
    if (model) builder.model_ = std::string(model);

    return builder;
}

AzureOpenAiBuilder AzureOpenAiProvider::builder() {
    return AzureOpenAiBuilder();
}

nlohmann::json AzureOpenAiProvider::request_body(const CompletionRequest& req, bool stream_flag) const {
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
        {"temperature", 1.0},
        {"stream", stream_flag}
    };

    if (req.max_tokens.has_value()) {
        body["max_completion_tokens"] = req.max_tokens.value();
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

CompletionResponse AzureOpenAiProvider::complete(const CompletionRequest& req) {
    nlohmann::json body = request_body(req, false);

    std::string scheme_host, base_path;
    parse_url(base_url_, scheme_host, base_path);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    httplib::Headers headers = {
        {"Content-Type", "application/json"},
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

void AzureOpenAiProvider::stream(const CompletionRequest& req, ChunkCallback callback) {
    nlohmann::json body = request_body(req, true);

    std::string scheme_host, base_path;
    parse_url(base_url_, scheme_host, base_path);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    std::string endpoint = base_path + "/chat/completions";
    std::string body_str = body.dump();

    std::string buffer;
    bool cancelled = false;

    // Use low-level send() with content_receiver for true streaming
    httplib::Request http_req;
    http_req.method = "POST";
    http_req.path = endpoint;
    http_req.headers = {
        {"Content-Type", "application/json"},
        {"Authorization", "Bearer " + api_key_}
    };
    http_req.body = body_str;
    http_req.content_receiver =
        [&](const char* data, size_t data_length, uint64_t /*offset*/, uint64_t /*total_length*/) -> bool {
            if (cancelled) return false;

            buffer.append(data, data_length);

            std::size_t pos;
            while ((pos = buffer.find('\n')) != std::string::npos) {
                std::string line = buffer.substr(0, pos);
                buffer.erase(0, pos + 1);

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

std::string AzureOpenAiProvider::name() const {
    return "azureopenai";
}

std::shared_ptr<ILlmProvider> AzureOpenAiProvider::clone_provider() const {
    return std::shared_ptr<AzureOpenAiProvider>(
        new AzureOpenAiProvider(api_key_, base_url_, model_)
    );
}

} // namespace agentrs
