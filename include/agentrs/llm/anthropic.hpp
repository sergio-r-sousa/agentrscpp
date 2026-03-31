#pragma once
// anthropic.hpp — Anthropic LLM provider
// Equivalent to agentrs-llm/src/anthropic/mod.rs

#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"

namespace agentrs {

class AnthropicProvider;

/// Builder for AnthropicProvider.
class AnthropicBuilder {
public:
    AnthropicBuilder();

    /// Sets the API key.
    AnthropicBuilder& api_key(const std::string& key);

    /// Sets the base URL.
    AnthropicBuilder& base_url(const std::string& url);

    /// Sets the default model.
    AnthropicBuilder& model(const std::string& model);

    /// Builds the provider. Throws MissingApiKeyError if API key is not set.
    [[nodiscard]] std::shared_ptr<AnthropicProvider> build() const;

private:
    std::optional<std::string> api_key_;
    std::string base_url_;
    std::string model_;

    friend class AnthropicProvider;
};

/// Anthropic LLM provider.
/// Sends requests to the Anthropic Messages API.
class AnthropicProvider : public ILlmProvider {
public:
    /// Creates a builder seeded from environment variables.
    /// Reads: ANTHROPIC_API_KEY, ANTHROPIC_BASE_URL, ANTHROPIC_MODEL
    static AnthropicBuilder from_env();

    /// Creates an empty builder with defaults.
    static AnthropicBuilder builder();

    // --- ILlmProvider interface ---
    CompletionResponse complete(const CompletionRequest& req) override;
    void stream(const CompletionRequest& req, ChunkCallback callback) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::shared_ptr<ILlmProvider> clone_provider() const override;

private:
    AnthropicProvider(const std::string& api_key,
                      const std::string& base_url,
                      const std::string& model);

    std::string api_key_;
    std::string base_url_;
    std::string model_;

    friend class AnthropicBuilder;
};

} // namespace agentrs
