#pragma once
// gemini.hpp — Google Gemini LLM provider
// Equivalent to agentrs-llm/src/gemini/mod.rs

#include <memory>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/streaming.hpp"

namespace agentrs {

class GeminiProvider;

/// Builder for GeminiProvider.
class GeminiBuilder {
public:
    GeminiBuilder();

    /// Sets the API key.
    GeminiBuilder& api_key(const std::string& key);

    /// Sets the default model.
    GeminiBuilder& model(const std::string& model);

    /// Sets the base URL.
    GeminiBuilder& base_url(const std::string& url);

    /// Builds the provider. Throws MissingApiKeyError if API key is not set.
    [[nodiscard]] std::shared_ptr<GeminiProvider> build() const;

private:
    std::optional<std::string> api_key_;
    std::string base_url_;
    std::string model_;

    friend class GeminiProvider;
};

/// Google Gemini LLM provider.
/// Sends requests to the Gemini generateContent API.
class GeminiProvider : public ILlmProvider {
public:
    /// Creates a builder seeded from environment variables.
    /// Reads: GEMINI_API_KEY, GEMINI_MODEL
    static GeminiBuilder from_env();

    /// Creates an empty builder with defaults.
    static GeminiBuilder builder();

    // --- ILlmProvider interface ---
    CompletionResponse complete(const CompletionRequest& req) override;
    void stream(const CompletionRequest& req, ChunkCallback callback) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::shared_ptr<ILlmProvider> clone_provider() const override;

private:
    GeminiProvider(const std::string& api_key,
                   const std::string& base_url,
                   const std::string& model);

    std::string api_key_;
    std::string base_url_;
    std::string model_;

    friend class GeminiBuilder;
};

} // namespace agentrs
