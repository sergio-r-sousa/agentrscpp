// gemini.cpp — Google Gemini LLM provider implementation
// Equivalent to agentrs-llm/src/gemini/mod.rs

#include <httplib.h>

#include <cstdlib>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/llm/gemini.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// GeminiBuilder
// ---------------------------------------------------------------------------

GeminiBuilder::GeminiBuilder()
    : base_url_("https://generativelanguage.googleapis.com/v1beta/models")
    , model_("gemini-2.0-flash") {}

GeminiBuilder& GeminiBuilder::api_key(const std::string& key) {
    api_key_ = key;
    return *this;
}

GeminiBuilder& GeminiBuilder::model(const std::string& model) {
    model_ = model;
    return *this;
}

GeminiBuilder& GeminiBuilder::base_url(const std::string& url) {
    base_url_ = url;
    return *this;
}

std::shared_ptr<GeminiProvider> GeminiBuilder::build() const {
    if (!api_key_.has_value()) {
        throw MissingApiKeyError("GEMINI_API_KEY");
    }
    return std::shared_ptr<GeminiProvider>(
        new GeminiProvider(api_key_.value(), base_url_, model_)
    );
}

// ---------------------------------------------------------------------------
// GeminiProvider
// ---------------------------------------------------------------------------

GeminiProvider::GeminiProvider(const std::string& api_key,
                               const std::string& base_url,
                               const std::string& model)
    : api_key_(api_key)
    , base_url_(base_url)
    , model_(model) {}

GeminiBuilder GeminiProvider::from_env() {
    GeminiBuilder builder;
    const char* key = std::getenv("GEMINI_API_KEY");
    if (key) builder.api_key_ = std::string(key);

    const char* model = std::getenv("GEMINI_MODEL");
    if (model) builder.model_ = std::string(model);

    return builder;
}

GeminiBuilder GeminiProvider::builder() {
    return GeminiBuilder();
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

CompletionResponse GeminiProvider::complete(const CompletionRequest& req) {
    // Build Gemini contents format
    nlohmann::json contents = nlohmann::json::array();

    if (req.system.has_value()) {
        contents.push_back({
            {"role", "user"},
            {"parts", nlohmann::json::array({{
                {"text", "System instruction: " + req.system.value()}
            }})}
        });
    }

    for (const auto& msg : req.messages) {
        std::string role;
        switch (msg.role) {
            case Role::Assistant: role = "model"; break;
            default:              role = "user"; break;
        }

        contents.push_back({
            {"role", role},
            {"parts", nlohmann::json::array({{
                {"text", msg.text_content()}
            }})}
        });
    }

    // Generation config
    nlohmann::json generation_config = nlohmann::json::object();
    if (req.temperature.has_value()) {
        generation_config["temperature"] = req.temperature.value();
    }
    if (req.max_tokens.has_value()) {
        generation_config["maxOutputTokens"] = req.max_tokens.value();
    }

    nlohmann::json body = {
        {"contents", contents},
        {"generationConfig", generation_config}
    };

    // Determine effective model
    std::string effective_model = req.model.empty() ? model_ : req.model;

    // Build URL: {base_url}/{model}:generateContent?key={api_key}
    std::string scheme_host, base_path;
    parse_url(base_url_, scheme_host, base_path);

    httplib::Client client(scheme_host);
    client.set_connection_timeout(30, 0);
    client.set_read_timeout(120, 0);

    std::string endpoint = base_path + "/" + effective_model
                         + ":generateContent?key=" + api_key_;

    httplib::Headers headers = {
        {"Content-Type", "application/json"}
    };

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

    // Parse Gemini response
    std::string text;
    if (payload.contains("candidates") && payload["candidates"].is_array() &&
        !payload["candidates"].empty()) {
        const auto& candidate = payload["candidates"][0];
        if (candidate.contains("content") && candidate["content"].contains("parts") &&
            candidate["content"]["parts"].is_array()) {
            for (const auto& part : candidate["content"]["parts"]) {
                if (part.contains("text") && part["text"].is_string()) {
                    text += part["text"].get<std::string>();
                }
            }
        }
    }

    CompletionResponse resp;
    resp.message = Message::assistant(text);
    resp.stop_reason = StopReasonValue(StopReason::Stop);
    resp.usage = Usage();
    resp.model = req.model;
    resp.raw = payload;
    return resp;
}

void GeminiProvider::stream(const CompletionRequest& /*req*/, ChunkCallback /*callback*/) {
    throw UnsupportedError("Gemini streaming is not yet implemented in this SDK scaffold");
}

std::string GeminiProvider::name() const {
    return "gemini";
}

std::shared_ptr<ILlmProvider> GeminiProvider::clone_provider() const {
    return std::shared_ptr<GeminiProvider>(
        new GeminiProvider(api_key_, base_url_, model_)
    );
}

} // namespace agentrs
