#pragma once
// config.hpp — YAML-driven agent and orchestrator configuration.
// Equivalent to agentrs/src/config.rs
//
// Uses yaml-cpp to parse YAML files and build agents,
// multi-agent orchestrators, or runtime configurations.

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/agents/builder.hpp"
#include "agentrs/multi/orchestrator.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// YAML config structs — mirror the Rust serde types.
// ---------------------------------------------------------------------------

/// YAML config for LLM provider creation.
/// Rust: `LlmYamlConfig` (serde tagged enum by "provider").
struct LlmYamlConfig {
    /// Provider name: "open_ai", "azure_open_ai", "anthropic", "gemini", "ollama"
    std::string provider;

    /// Optional API key override.
    std::optional<std::string> api_key;

    /// Optional base URL override.
    std::optional<std::string> base_url;

    /// Optional model override.
    std::optional<std::string> model;
};

/// YAML config for tool creation.
/// Rust: `ToolYamlConfig` (serde tagged enum by "type").
struct ToolYamlConfig {
    /// Tool type: "calculator", "web_fetch", "web_search", "file_read",
    ///            "file_write", "mcp"
    std::string type;

    // -- FileRead / FileWrite options --
    std::optional<std::string> root;

    // -- MCP options --
    std::optional<std::string> target;
    std::optional<std::string> api_key;
    std::optional<std::string> api_key_header;
    std::optional<std::string> api_key_prefix;
    std::optional<std::map<std::string, std::string>> headers;
};

/// YAML config for memory backends.
/// Rust: `MemoryYamlConfig` (serde tagged enum by "type").
struct MemoryYamlConfig {
    /// Memory type: "in_memory", "sliding_window", "token_aware", "vector"
    std::string type = "in_memory";

    // -- SlidingWindow options --
    std::optional<std::size_t> window_size;

    // -- TokenAware options --
    std::optional<std::size_t> max_tokens;

    /// Returns true when this is the default configuration.
    [[nodiscard]] bool is_default() const { return type == "in_memory"; }
};

/// YAML config for loop strategies.
/// Rust: `LoopStrategyYamlConfig` (serde tagged enum by "type").
struct LoopStrategyYamlConfig {
    /// Strategy type: "re_act", "chain_of_thought", "plan_and_execute", "custom"
    std::string type = "re_act";

    /// Optional max steps override (re_act, plan_and_execute).
    std::optional<std::size_t> max_steps;

    /// Custom instruction (custom strategy only).
    std::optional<std::string> instruction;

    /// Returns true when this is the default configuration.
    [[nodiscard]] bool is_default() const {
        return type == "re_act" && !max_steps.has_value();
    }

    /// Converts to the runtime LoopStrategy type.
    [[nodiscard]] LoopStrategy into_loop_strategy() const;
};

/// YAML config for routing strategies.
/// Rust: `RoutingYamlConfig` (serde tagged enum by "type").
struct RoutingYamlConfig {
    /// Strategy type: "sequential", "parallel"
    std::string type = "sequential";

    /// Optional explicit order (sequential).
    std::optional<std::vector<std::string>> order;

    /// Agents for parallel execution.
    std::optional<std::vector<std::string>> agents;

    /// Returns true when this is the default configuration.
    [[nodiscard]] bool is_default() const {
        return type == "sequential" && !order.has_value();
    }

    /// Converts to the runtime RoutingStrategy variant.
    /// @param default_order  Agent names in declaration order (used when order is absent).
    [[nodiscard]] RoutingStrategy into_routing_strategy(
        const std::vector<std::string>& default_order) const;
};

/// YAML config for a single agent.
/// Rust: `AgentYamlConfig`.
struct AgentYamlConfig {
    /// Optional friendly agent name.
    std::optional<std::string> name;

    /// LLM provider configuration (required).
    LlmYamlConfig llm;

    /// Optional system prompt.
    std::optional<std::string> system;

    /// Memory backend (defaults to in_memory).
    MemoryYamlConfig memory;

    /// Optional tools.
    std::vector<ToolYamlConfig> tools;

    /// Loop strategy (defaults to re_act).
    LoopStrategyYamlConfig loop_strategy;

    /// Optional model override at agent level.
    std::optional<std::string> model;

    /// Sampling temperature.
    std::optional<float> temperature;

    /// Max output tokens.
    std::optional<uint32_t> max_tokens;

    /// Max steps.
    std::optional<std::size_t> max_steps;

    /// Builds a boxed agent from this configuration.
    [[nodiscard]] std::unique_ptr<IAgent> build() const;
};

/// A named agent entry inside a multi-agent YAML config.
/// Rust: `NamedAgentYamlConfig`.
struct NamedAgentYamlConfig {
    /// Public agent name.
    std::string name;

    /// Agent configuration (all AgentYamlConfig fields flattened).
    AgentYamlConfig agent;
};

/// YAML config for multi-agent orchestration.
/// Rust: `MultiAgentYamlConfig`.
struct MultiAgentYamlConfig {
    /// Named agents available to the orchestrator.
    std::vector<NamedAgentYamlConfig> agents;

    /// Routing strategy (defaults to sequential).
    RoutingYamlConfig routing;

    /// Builds a MultiAgentOrchestrator from this configuration.
    [[nodiscard]] MultiAgentOrchestrator build() const;
};

/// Top-level YAML runtime config.
/// Rust: `RuntimeConfig` (serde tagged enum by "kind").
struct RuntimeConfig {
    /// "agent" or "multi_agent"
    std::string kind;

    /// Agent config (when kind == "agent").
    std::optional<AgentYamlConfig> agent;

    /// Multi-agent config (when kind == "multi_agent").
    std::optional<MultiAgentYamlConfig> multi_agent;
};

// ---------------------------------------------------------------------------
// Runtime loaded from YAML.
// ---------------------------------------------------------------------------

/// Runtime loaded from YAML — either a single agent or a multi-agent orchestrator.
/// Rust: `ConfiguredRuntime`.
class ConfiguredRuntime {
public:
    enum class Kind { Agent, Multi };

    /// Creates a single-agent runtime.
    explicit ConfiguredRuntime(std::unique_ptr<IAgent> agent);

    /// Creates a multi-agent runtime.
    explicit ConfiguredRuntime(MultiAgentOrchestrator orchestrator);

    /// Runs the configured runtime with a user input.
    AgentOutput run(const std::string& input);

    /// Returns the runtime kind.
    [[nodiscard]] Kind kind() const { return kind_; }

private:
    Kind kind_;
    std::unique_ptr<IAgent> agent_;
    std::optional<MultiAgentOrchestrator> orchestrator_;
};

// ---------------------------------------------------------------------------
// Free functions — top-level API matching the Rust public interface.
// ---------------------------------------------------------------------------

/// Loads a single agent from a YAML file on disk.
[[nodiscard]] std::unique_ptr<IAgent> load_agent_from_yaml(const std::string& path);

/// Loads a single agent from a YAML string.
[[nodiscard]] std::unique_ptr<IAgent> load_agent_from_yaml_str(const std::string& yaml);

/// Loads a multi-agent orchestrator from a YAML file on disk.
[[nodiscard]] MultiAgentOrchestrator load_multi_agent_from_yaml(const std::string& path);

/// Loads a multi-agent orchestrator from a YAML string.
[[nodiscard]] MultiAgentOrchestrator load_multi_agent_from_yaml_str(const std::string& yaml);

/// Loads a YAML runtime (agent or multi-agent) from disk.
[[nodiscard]] ConfiguredRuntime load_runtime_from_yaml(const std::string& path);

/// Loads a YAML runtime (agent or multi-agent) from a string.
[[nodiscard]] ConfiguredRuntime load_runtime_from_yaml_str(const std::string& yaml);

// ---------------------------------------------------------------------------
// Internal helpers (exposed for testing).
// ---------------------------------------------------------------------------

namespace detail {

/// Builds an LLM provider from YAML config.
[[nodiscard]] std::shared_ptr<ILlmProvider> build_llm(const LlmYamlConfig& config);

/// Builds a tool registry from YAML tool configs.
[[nodiscard]] ToolRegistry build_tools(const std::vector<ToolYamlConfig>& configs);

} // namespace detail

} // namespace agentrs
