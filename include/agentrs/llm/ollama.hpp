#pragma once
// ollama.hpp — Ollama LLM provider (OpenAI-compatible wrapper)
// Equivalent to agentrs-llm/src/ollama/mod.rs

#include <memory>
#include <string>

#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/error.hpp"

namespace agentrs {

class OllamaProvider;

/// Builder for OllamaProvider.
class OllamaBuilder {
public:
    OllamaBuilder();

    /// Sets the model name.
    OllamaBuilder& model(const std::string& model);

    /// Sets the base URL.
    OllamaBuilder& base_url(const std::string& url);

    /// Builds the provider.
    [[nodiscard]] std::shared_ptr<OllamaProvider> build() const;

private:
    std::string model_;
    std::string base_url_;

    friend class OllamaProvider;
};

/// Ollama LLM provider backed by its OpenAI-compatible endpoint.
/// Delegates to an internal OpenAiProvider with api_key="ollama".
class OllamaProvider : public ILlmProvider {
public:
    /// Creates a builder seeded from environment variables.
    /// Reads: OLLAMA_BASE_URL, OLLAMA_MODEL
    static OllamaBuilder from_env();

    /// Creates an empty builder with defaults.
    static OllamaBuilder builder();

    /// Convenience: creates a provider with the given model and default base URL.
    static std::shared_ptr<OllamaProvider> create(const std::string& model);

    // --- ILlmProvider interface ---
    CompletionResponse complete(const CompletionRequest& req) override;
    void stream(const CompletionRequest& req, ChunkCallback callback) override;
    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::shared_ptr<ILlmProvider> clone_provider() const override;

private:
    OllamaProvider(std::shared_ptr<ILlmProvider> inner);

    std::shared_ptr<ILlmProvider> inner_;

    friend class OllamaBuilder;
};

} // namespace agentrs
