#pragma once
// openai.hpp — OpenAI-compatible LLM provider
// Equivalent to agentrs-llm/src/openai/mod.rs

#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"

namespace agentrs {

class OpenAiProvider;

/// Builder for OpenAiProvider.
class OpenAiBuilder {
public:
    OpenAiBuilder();

    /// Overrides the API key.
    OpenAiBuilder& api_key(const std::string& key);

    /// Overrides the base URL.
    OpenAiBuilder& base_url(const std::string& url);

    /// Sets the default model.
    OpenAiBuilder& model(const std::string& model);

    /// Builds the provider. Throws MissingApiKeyError if API key is not set.
    [[nodiscard]] std::shared_ptr<OpenAiProvider> build() const;

private:
    std::optional<std::string> api_key_;
    std::string base_url_;
    std::string model_;

    friend class OpenAiProvider;
};

/// OpenAI-compatible LLM provider.
/// Sends requests to the OpenAI chat/completions API.
class OpenAiProvider : public ILlmProvider {
public:
    /// Creates a builder seeded from environment variables.
    /// Reads: OPENAI_API_KEY, OPENAI_BASE_URL, OPENAI_MODEL
    static OpenAiBuilder from_env();

    /// Creates an empty builder with defaults.
    static OpenAiBuilder builder();

    // --- ILlmProvider interface ---
    CompletionResponse complete(const CompletionRequest& req) override;
    void stream(const CompletionRequest& req, ChunkCallback callback) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::shared_ptr<ILlmProvider> clone_provider() const override;

private:
    OpenAiProvider(const std::string& api_key,
                   const std::string& base_url,
                   const std::string& model);

    /// Builds the JSON request body for the API.
    [[nodiscard]] nlohmann::json request_body(const CompletionRequest& req, bool stream_flag) const;

    std::string api_key_;
    std::string base_url_;
    std::string model_;

    friend class OpenAiBuilder;
    friend class OllamaProvider;
};

// --- Helper functions (internal, shared with Azure) ---

namespace detail {

nlohmann::json map_openai_message(const Message& message);
nlohmann::json map_openai_tool(const ToolDefinition& tool);
nlohmann::json map_openai_tool_call(const ToolCall& tool_call);
CompletionResponse map_openai_response(const nlohmann::json& payload);
Usage parse_openai_usage(const nlohmann::json& value);

/// Returns fallback if str is empty.
std::string if_empty_then(const std::string& str, const std::string& fallback);

} // namespace detail

} // namespace agentrs
