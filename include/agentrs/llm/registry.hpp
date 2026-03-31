#pragma once
// registry.hpp — Runtime LLM provider registry
// Equivalent to agentrs-llm/src/registry.rs

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include "agentrs/core/interfaces.hpp"

namespace agentrs {

/// Runtime registry for LLM providers.
/// Allows registering and retrieving providers by name.
class LlmProviderRegistry {
public:
    LlmProviderRegistry() = default;

    /// Registers a provider. Returns *this for chaining.
    LlmProviderRegistry& register_provider(std::shared_ptr<ILlmProvider> provider);

    /// Returns a provider by name, or nullptr if not found.
    [[nodiscard]] std::shared_ptr<ILlmProvider> get(const std::string& name) const;

    /// Returns true if a provider with the given name is registered.
    [[nodiscard]] bool has(const std::string& name) const;

    /// Returns the number of registered providers.
    [[nodiscard]] std::size_t size() const;

    /// Factory: creates a provider by name from the registry.
    /// Returns nullptr if the provider is not registered.
    [[nodiscard]] std::shared_ptr<ILlmProvider> create(const std::string& provider_name) const;

private:
    std::unordered_map<std::string, std::shared_ptr<ILlmProvider>> providers_;
};

} // namespace agentrs
