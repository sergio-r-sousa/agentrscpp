// registry.cpp — LlmProviderRegistry implementation
// Equivalent to agentrs-llm/src/registry.rs

#include "agentrs/llm/registry.hpp"

namespace agentrs {

LlmProviderRegistry& LlmProviderRegistry::register_provider(std::shared_ptr<ILlmProvider> provider) {
    if (provider) {
        providers_[provider->name()] = std::move(provider);
    }
    return *this;
}

std::shared_ptr<ILlmProvider> LlmProviderRegistry::get(const std::string& name) const {
    auto it = providers_.find(name);
    if (it != providers_.end()) {
        return it->second;
    }
    return nullptr;
}

bool LlmProviderRegistry::has(const std::string& name) const {
    return providers_.find(name) != providers_.end();
}

std::size_t LlmProviderRegistry::size() const {
    return providers_.size();
}

std::shared_ptr<ILlmProvider> LlmProviderRegistry::create(const std::string& provider_name) const {
    auto provider = get(provider_name);
    if (provider) {
        return provider->clone_provider();
    }
    return nullptr;
}

} // namespace agentrs
