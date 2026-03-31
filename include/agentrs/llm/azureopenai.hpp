#pragma once
// azureopenai.hpp — Azure OpenAI LLM provider
// Equivalent to agentrs-llm/src/azureopenai/mod.rs

#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"

namespace agentrs {

class AzureOpenAiProvider;

/// Builder for AzureOpenAiProvider.
class AzureOpenAiBuilder {
public:
    AzureOpenAiBuilder();

    /// Overrides the API key.
    AzureOpenAiBuilder& api_key(const std::string& key);

    /// Overrides the base URL (Azure endpoint).
    AzureOpenAiBuilder& base_url(const std::string& url);

    /// Sets the default model (deployment name).
    AzureOpenAiBuilder& model(const std::string& model);

    /// Builds the provider. Throws MissingApiKeyError if API key is not set.
    [[nodiscard]] std::shared_ptr<AzureOpenAiProvider> build() const;

private:
    std::optional<std::string> api_key_;
    std::optional<std::string> base_url_;
    std::optional<std::string> model_;

    friend class AzureOpenAiProvider;
};

/// Azure OpenAI LLM provider.
/// Sends requests to the Azure OpenAI chat/completions API.
class AzureOpenAiProvider : public ILlmProvider {
public:
    /// Creates a builder seeded from environment variables.
    /// Reads: AZURE_OPENAI_ENDPOINT, AZURE_OPENAI_KEY, AZURE_OPENAI_MODEL
    static AzureOpenAiBuilder from_env();

    /// Creates an empty builder with defaults.
    static AzureOpenAiBuilder builder();

    // --- ILlmProvider interface ---
    CompletionResponse complete(const CompletionRequest& req) override;
    void stream(const CompletionRequest& req, ChunkCallback callback) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::shared_ptr<ILlmProvider> clone_provider() const override;

private:
    AzureOpenAiProvider(const std::string& api_key,
                        const std::string& base_url,
                        const std::string& model);

    /// Builds the JSON request body for the API.
    [[nodiscard]] nlohmann::json request_body(const CompletionRequest& req, bool stream_flag) const;

    std::string api_key_;
    std::string base_url_;
    std::string model_;

    friend class AzureOpenAiBuilder;
};

} // namespace agentrs
