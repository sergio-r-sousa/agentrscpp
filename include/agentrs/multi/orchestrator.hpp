#pragma once
// orchestrator.hpp — Multi-agent orchestrator with configurable routing strategies.
// Equivalent to agentrs-multi/src/orchestrator.rs

#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"
#include "agentrs/multi/communication.hpp"
#include "agentrs/multi/graph.hpp"
#include "agentrs/multi/shared_memory.hpp"

namespace agentrs {

// Forward declarations
class MultiAgentOrchestratorBuilder;

// --- RoutingStrategy --------------------------------------------------------

/// Routing strategy variants for multi-agent runs.
///
/// Sequential: runs agents in the given order, passing output of one as input to next.
/// Parallel:   runs agents concurrently (std::async), returns combined output.
/// Supervisor: a supervisor LLM decides which worker agent to call next.
/// Graph:      uses an AgentGraph to determine execution order.

struct SequentialStrategy {
    std::vector<std::string> agents;
};

struct ParallelStrategy {
    std::vector<std::string> agents;
};

struct SupervisorStrategy {
    /// LLM provider used for routing decisions.
    std::shared_ptr<ILlmProvider> llm;
    /// Eligible worker agent names.
    std::vector<std::string> workers;
    /// Maximum supervisor turns before giving up.
    std::size_t max_turns = 3;
};

struct GraphStrategy {
    AgentGraph graph;
};

using RoutingStrategy = std::variant<
    SequentialStrategy,
    ParallelStrategy,
    SupervisorStrategy,
    GraphStrategy
>;

// --- MultiAgentOrchestrator -------------------------------------------------

/// Multi-agent orchestrator: stores named agents and executes them according
/// to a routing strategy.
class MultiAgentOrchestrator {
public:
    /// Starts building an orchestrator.
    static MultiAgentOrchestratorBuilder builder();

    /// Runs the configured workflow to completion.
    AgentOutput run(std::string_view input);

    /// Runs the configured workflow as an event stream.
    void stream_run(std::string_view input, EventCallback callback);

    /// Registers or replaces a named agent after construction.
    void add_agent(const std::string& name, std::unique_ptr<IAgent> agent);

    /// Returns agent names.
    [[nodiscard]] std::vector<std::string> agent_names() const;

private:
    friend class MultiAgentOrchestratorBuilder;

    MultiAgentOrchestrator() = default;

    AgentOutput run_sequential(std::string_view input,
                               const std::vector<std::string>& names);
    AgentOutput run_parallel(std::string_view input,
                             const std::vector<std::string>& names);
    AgentOutput run_supervisor(std::string_view input,
                               const std::shared_ptr<ILlmProvider>& llm,
                               const std::vector<std::string>& workers,
                               std::size_t max_turns);
    AgentOutput run_graph(std::string_view input, const AgentGraph& graph);

    AgentOutput run_named_agent(const std::string& name, std::string_view input);
    void emit_event(const std::string& agent_name, const AgentOutput& output);

    std::map<std::string, std::unique_ptr<IAgent>> agents_;
    RoutingStrategy routing_;
    std::shared_ptr<SharedConversation> shared_memory_;
    std::shared_ptr<IEventBus> event_bus_;
};

// --- MultiAgentOrchestratorBuilder ------------------------------------------

/// Builder for MultiAgentOrchestrator.
class MultiAgentOrchestratorBuilder {
public:
    MultiAgentOrchestratorBuilder() = default;

    /// Registers an agent by name (move semantics).
    MultiAgentOrchestratorBuilder& add_agent(const std::string& name,
                                              std::unique_ptr<IAgent> agent);

    /// Sets the routing strategy.
    MultiAgentOrchestratorBuilder& routing(RoutingStrategy strategy);

    /// Enables shared conversation memory.
    MultiAgentOrchestratorBuilder& shared_memory(std::shared_ptr<SharedConversation> memory);

    /// Enables orchestration events.
    MultiAgentOrchestratorBuilder& event_bus(std::shared_ptr<IEventBus> bus);

    /// Builds the orchestrator. Throws InvalidConfigurationError on bad config.
    [[nodiscard]] MultiAgentOrchestrator build();

private:
    std::map<std::string, std::unique_ptr<IAgent>> agents_;
    std::vector<std::string> order_;
    std::optional<RoutingStrategy> routing_;
    std::shared_ptr<SharedConversation> shared_memory_;
    std::shared_ptr<IEventBus> event_bus_;
};

} // namespace agentrs
