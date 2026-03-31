#pragma once
// builder.hpp — AgentBuilder for fluent agent construction.
// Equivalent to agentrs-agents/src/builder.rs
//
// The Rust crate uses a typestate pattern (NoLlm → WithLlm) enforced at
// compile time. In C++ we simplify: the builder stores all fields as
// optionals and build() throws NoLlmProviderError if no LLM was set.

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/tools/registry.hpp"

namespace agentrs {

// Forward declarations
class AgentRunner;
struct AgentConfig;
class AgentBuilder;

/// LoopStrategy — built-in reasoning loop strategies.
/// Equivalent to Rust's `LoopStrategy` enum.
enum class LoopStrategyKind {
    ReAct,
    CoT,
    PlanAndExecute,
    Custom
};

/// Loop strategy with associated parameters.
struct LoopStrategy {
    LoopStrategyKind kind = LoopStrategyKind::ReAct;
    std::size_t max_steps = 8;
    std::string custom_instruction;

    /// Standard reason-and-act loop.
    static LoopStrategy react(std::size_t max_steps = 8) {
        return {LoopStrategyKind::ReAct, max_steps, {}};
    }

    /// Single-pass chain-of-thought (no tool loop).
    static LoopStrategy cot() {
        return {LoopStrategyKind::CoT, 0, {}};
    }

    /// Planner + executor loop using the same LLM.
    static LoopStrategy plan_and_execute(std::size_t max_steps = 8) {
        return {LoopStrategyKind::PlanAndExecute, max_steps, {}};
    }

    /// Custom instruction prepended before execution.
    static LoopStrategy custom(std::string instruction) {
        return {LoopStrategyKind::Custom, 0, std::move(instruction)};
    }

    /// Returns the max_steps hint for this strategy, or fallback.
    [[nodiscard]] std::size_t max_steps_hint(std::size_t fallback) const {
        switch (kind) {
            case LoopStrategyKind::ReAct:
            case LoopStrategyKind::PlanAndExecute:
                return max_steps;
            case LoopStrategyKind::CoT:
            case LoopStrategyKind::Custom:
                return fallback;
        }
        return fallback;
    }
};

/// Runtime agent configuration.
struct AgentConfig {
    /// Default model name.
    std::string model;

    /// Sampling temperature.
    std::optional<float> temperature = 0.2f;

    /// Maximum output tokens.
    std::optional<uint32_t> max_tokens = 4096;

    /// Loop strategy.
    LoopStrategy loop_strategy = LoopStrategy::react(8);

    /// Maximum tool/action steps.
    std::size_t max_steps = 8;
};

/// User-facing entrypoint for agent construction.
///
/// Usage:
///   auto agent = Agent::builder()
///       .llm(my_provider)
///       .system("You are a helpful assistant.")
///       .tool(std::make_unique<MyTool>())
///       .build();
struct Agent {
    /// Starts building an agent.
    static AgentBuilder builder();
};

/// Fluent builder for AgentRunner.
///
/// Equivalent to Rust's `AgentBuilder<State, M>`.
/// All fields are optional; build() validates that an LLM provider was set.
class AgentBuilder {
public:
    AgentBuilder();
    ~AgentBuilder() = default;

    // Movable
    AgentBuilder(AgentBuilder&&) noexcept = default;
    AgentBuilder& operator=(AgentBuilder&&) noexcept = default;

    // Not copyable (owns ToolRegistry with unique_ptrs)
    AgentBuilder(const AgentBuilder&) = delete;
    AgentBuilder& operator=(const AgentBuilder&) = delete;

    /// Sets the LLM provider (shared ownership).
    AgentBuilder& llm(std::shared_ptr<ILlmProvider> provider);

    /// Sets the system prompt.
    AgentBuilder& system(std::string prompt);

    /// Registers a single tool.
    AgentBuilder& tool(std::unique_ptr<ITool> t);

    /// Replaces the entire tool registry.
    AgentBuilder& tools(ToolRegistry registry);

    /// Replaces the memory backend.
    AgentBuilder& memory(std::unique_ptr<IMemory> mem);

    /// Sets the default model name.
    AgentBuilder& model(std::string model_name);

    /// Sets the loop strategy.
    AgentBuilder& loop_strategy(LoopStrategy strategy);

    /// Sets the sampling temperature.
    AgentBuilder& temperature(float t);

    /// Sets the maximum output tokens.
    AgentBuilder& max_tokens(uint32_t tokens);

    /// Sets the maximum tool/action steps.
    AgentBuilder& max_steps(std::size_t steps);

    /// Builds the configured agent.
    /// Throws NoLlmProviderError if no LLM was set.
    [[nodiscard]] std::unique_ptr<AgentRunner> build();

private:
    std::shared_ptr<ILlmProvider> llm_;
    std::unique_ptr<IMemory> memory_;
    ToolRegistry tools_;
    std::optional<std::string> system_prompt_;
    AgentConfig config_;
};

} // namespace agentrs
