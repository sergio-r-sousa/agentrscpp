// ollama.cpp — Ollama LLM provider implementation
// Equivalent to agentrs-llm/src/ollama/mod.rs

#include <cstdlib>
#include <string>

#include "agentrs/llm/ollama.hpp"
#include "agentrs/llm/openai.hpp"
#include "agentrs/core/error.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// OllamaBuilder
// ---------------------------------------------------------------------------

OllamaBuilder::OllamaBuilder()
    : model_("llama3.2")
    , base_url_("http://localhost:11434/v1") {}

OllamaBuilder& OllamaBuilder::model(const std::string& model) {
    model_ = model;
    return *this;
}

OllamaBuilder& OllamaBuilder::base_url(const std::string& url) {
    base_url_ = url;
    return *this;
}

std::shared_ptr<OllamaProvider> OllamaBuilder::build() const {
    // Build an internal OpenAiProvider with api_key="ollama"
    auto inner = OpenAiProvider::builder()
        .api_key("ollama")
        .base_url(base_url_)
        .model(model_)
        .build();

    return std::shared_ptr<OllamaProvider>(
        new OllamaProvider(std::move(inner))
    );
}

// ---------------------------------------------------------------------------
// OllamaProvider
// ---------------------------------------------------------------------------

OllamaProvider::OllamaProvider(std::shared_ptr<ILlmProvider> inner)
    : inner_(std::move(inner)) {}

OllamaBuilder OllamaProvider::from_env() {
    OllamaBuilder builder;
    const char* url = std::getenv("OLLAMA_BASE_URL");
    if (url) builder.base_url_ = std::string(url);

    const char* model = std::getenv("OLLAMA_MODEL");
    if (model) builder.model_ = std::string(model);

    return builder;
}

OllamaBuilder OllamaProvider::builder() {
    return OllamaBuilder();
}

std::shared_ptr<OllamaProvider> OllamaProvider::create(const std::string& model) {
    return OllamaProvider::builder().model(model).build();
}

CompletionResponse OllamaProvider::complete(const CompletionRequest& req) {
    return inner_->complete(req);
}

void OllamaProvider::stream(const CompletionRequest& req, ChunkCallback callback) {
    inner_->stream(req, std::move(callback));
}

std::string OllamaProvider::name() const {
    return "ollama";
}

std::shared_ptr<ILlmProvider> OllamaProvider::clone_provider() const {
    return std::shared_ptr<OllamaProvider>(
        new OllamaProvider(inner_->clone_provider())
    );
}

} // namespace agentrs
