#pragma once
// runner.hpp — AgentRunner: the core agent execution engine.
// Equivalent to agentrs-agents/src/runner.rs
//
// Implements the IAgent interface with ReAct, CoT, PlanAndExecute, and
// Custom loop strategies. Owns an LLM provider, tool registry, memory
// backend, and agent configuration.

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/tools/registry.hpp"
#include "agentrs/agents/builder.hpp"

namespace agentrs {

/// Runnable agent implementation.
///
/// Equivalent to Rust's `AgentRunner<M>`. In C++ the memory backend is
/// type-erased behind `std::unique_ptr<IMemory>` so there is no template.
class AgentRunner : public IAgent {
public:
    /// Constructs a new agent runner with all components.
    AgentRunner(
        std::shared_ptr<ILlmProvider> llm,
        std::unique_ptr<IMemory> memory,
        ToolRegistry tools,
        std::optional<std::string> system_prompt,
        AgentConfig config
    );

    ~AgentRunner() override = default;

    // Movable
    AgentRunner(AgentRunner&&) noexcept = default;
    AgentRunner& operator=(AgentRunner&&) noexcept = default;

    // Not copyable
    AgentRunner(const AgentRunner&) = delete;
    AgentRunner& operator=(const AgentRunner&) = delete;

    // --- IAgent interface ---------------------------------------------------

    /// Runs the agent to completion for a user input.
    AgentOutput run(std::string_view input) override;

    /// Runs the agent as an event stream.
    /// Calls the callback for each event. Return false from the callback to cancel.
    void stream_run(std::string_view input, EventCallback callback) override;

    // --- Static builder shortcut --------------------------------------------

    /// Returns a new AgentBuilder.
    static AgentBuilder builder();

    // --- Accessors ----------------------------------------------------------

    /// Returns a reference to the agent's tool registry.
    [[nodiscard]] const ToolRegistry& tool_registry() const noexcept { return tools_; }

    /// Returns the agent's configuration.
    [[nodiscard]] const AgentConfig& config() const noexcept { return config_; }

    /// Returns the system prompt, if set.
    [[nodiscard]] const std::optional<std::string>& system_prompt() const noexcept {
        return system_prompt_;
    }

private:
    // --- Loop implementations -----------------------------------------------

    /// ReAct loop: reason → act → observe → repeat.
    AgentOutput run_react(std::string_view input);

    /// Chain-of-thought: single LLM call with no tool loop.
    AgentOutput run_cot(std::string_view input);

    /// Plan-and-execute: planner call then ReAct execution.
    AgentOutput run_plan_execute(std::string_view input, std::size_t max_steps);

    // --- Helpers ------------------------------------------------------------

    /// Builds a CompletionRequest from the current history.
    [[nodiscard]] CompletionRequest build_request(
        const std::vector<Message>& history,
        bool include_tools
    ) const;

    /// Executes a vector of tool calls and returns the result messages.
    std::vector<Message> execute_tool_calls(const std::vector<ToolCall>& tool_calls);

    /// Creates the final AgentOutput from a completion response.
    [[nodiscard]] AgentOutput finish_output(
        const CompletionResponse& response,
        std::size_t steps
    ) const;

    // --- Members ------------------------------------------------------------

    std::shared_ptr<ILlmProvider> llm_;
    std::unique_ptr<IMemory> memory_;
    ToolRegistry tools_;
    std::optional<std::string> system_prompt_;
    AgentConfig config_;
};

} // namespace agentrs
