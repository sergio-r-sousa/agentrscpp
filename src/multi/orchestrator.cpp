// orchestrator.cpp — Multi-agent orchestrator implementation.

#include "agentrs/multi/orchestrator.hpp"

#include <future>
#include <sstream>

#include <nlohmann/json.hpp>

namespace agentrs {

// --- MultiAgentOrchestrator -------------------------------------------------

MultiAgentOrchestratorBuilder MultiAgentOrchestrator::builder() {
    return MultiAgentOrchestratorBuilder{};
}

AgentOutput MultiAgentOrchestrator::run(std::string_view input) {
    // Visit the routing strategy variant and dispatch.
    struct Visitor {
        MultiAgentOrchestrator& self;
        std::string_view input;

        AgentOutput operator()(const SequentialStrategy& s) {
            return self.run_sequential(input, s.agents);
        }
        AgentOutput operator()(const ParallelStrategy& s) {
            return self.run_parallel(input, s.agents);
        }
        AgentOutput operator()(const SupervisorStrategy& s) {
            return self.run_supervisor(input, s.llm, s.workers, s.max_turns);
        }
        AgentOutput operator()(const GraphStrategy& s) {
            return self.run_graph(input, s.graph);
        }
    };

    return std::visit(Visitor{*this, input}, routing_);
}

void MultiAgentOrchestrator::stream_run(std::string_view input,
                                         EventCallback callback) {
    // Simplified streaming: run to completion and emit a Done event.
    auto output = run(input);
    if (callback) {
        callback(AgentEvent::done(std::move(output)));
    }
}

void MultiAgentOrchestrator::add_agent(const std::string& name,
                                        std::unique_ptr<IAgent> agent) {
    agents_[name] = std::move(agent);
}

std::vector<std::string> MultiAgentOrchestrator::agent_names() const {
    std::vector<std::string> names;
    names.reserve(agents_.size());
    for (const auto& [name, _] : agents_) {
        names.push_back(name);
    }
    return names;
}

AgentOutput MultiAgentOrchestrator::run_sequential(
    std::string_view input, const std::vector<std::string>& names) {
    if (names.empty()) {
        throw InvalidConfigurationError("no agents configured");
    }

    std::string current_input(input);
    AgentOutput final_output{};

    for (const auto& name : names) {
        final_output = run_named_agent(name, current_input);
        current_input = final_output.text;
    }

    return final_output;
}

AgentOutput MultiAgentOrchestrator::run_parallel(
    std::string_view input, const std::vector<std::string>& names) {
    if (names.empty()) {
        throw InvalidConfigurationError("no agents configured");
    }

    // Launch each agent in its own async task.
    struct AgentResult {
        std::string name;
        AgentOutput output;
    };

    std::vector<std::future<AgentResult>> futures;
    futures.reserve(names.size());

    // Note: std::async with unique_ptr requires careful handling.
    // We run agents via pointer, not moving ownership.
    for (const auto& name : names) {
        auto it = agents_.find(name);
        if (it == agents_.end()) {
            throw AgentNotFoundError(name);
        }
        IAgent* agent_ptr = it->second.get();
        std::string agent_name = name;
        std::string inp(input);

        futures.push_back(std::async(std::launch::async,
            [agent_ptr, agent_name, inp]() -> AgentResult {
                auto output = agent_ptr->run(inp);
                return AgentResult{agent_name, std::move(output)};
            }));
    }

    // Collect results.
    std::ostringstream combined_text;
    std::vector<Message> combined_messages;
    bool first = true;

    for (auto& fut : futures) {
        auto result = fut.get(); // may throw if agent threw

        // Emit event.
        emit_event(result.name, result.output);

        // Store shared memory.
        if (shared_memory_ && !result.output.messages.empty()) {
            shared_memory_->add(result.name, result.output.messages.back());
        }

        if (!first) {
            combined_text << "\n\n";
        }
        combined_text << "[" << result.name << "]\n" << result.output.text;
        combined_messages.insert(combined_messages.end(),
                                 result.output.messages.begin(),
                                 result.output.messages.end());
        first = false;
    }

    AgentOutput output;
    output.text = combined_text.str();
    output.steps = 1;
    output.usage = Usage{};
    output.messages = std::move(combined_messages);
    return output;
}

AgentOutput MultiAgentOrchestrator::run_supervisor(
    std::string_view input,
    const std::shared_ptr<ILlmProvider>& llm,
    const std::vector<std::string>& workers,
    std::size_t max_turns) {

    // Build the agent list for the supervisor prompt.
    std::ostringstream agent_lines;
    for (const auto& name : workers) {
        agent_lines << "- " << name << "\n";
    }

    std::string context =
        "You are a supervisor. Available agents:\n" + agent_lines.str() +
        "\nReturn JSON {\"agent\": \"name\"} for the best agent to handle the "
        "task. Task: " + std::string(input);

    for (std::size_t turn = 0; turn < max_turns; ++turn) {
        CompletionRequest req;
        req.messages = {Message::user(context)};
        req.tools = std::nullopt;
        req.model = "";
        req.temperature = 0.0f;
        req.max_tokens = 256;
        req.stream = false;
        req.system = std::nullopt;

        auto response = llm->complete(req);
        auto response_text = response.message.text_content();

        // Try to parse the response as JSON.
        try {
            auto choice = nlohmann::json::parse(response_text);
            auto agent_it = choice.find("agent");
            if (agent_it != choice.end() && agent_it->is_string()) {
                std::string agent_name = agent_it->get<std::string>();
                return run_named_agent(agent_name, input);
            }
        } catch (const nlohmann::json::exception&) {
            // Not valid JSON; append feedback and retry.
        }

        context += "\nPrevious response was invalid JSON: " + response_text;
    }

    throw MaxStepsReached(max_turns);
}

AgentOutput MultiAgentOrchestrator::run_graph(std::string_view input,
                                               const AgentGraph& graph) {
    std::string current = graph.entry();
    std::string current_input(input);

    for (;;) {
        auto output = run_named_agent(current, current_input);
        auto next = graph.next(current, output);
        if (next.has_value()) {
            current = next.value();
            current_input = output.text;
            continue;
        }
        return output;
    }
}

AgentOutput MultiAgentOrchestrator::run_named_agent(const std::string& name,
                                                     std::string_view input) {
    auto it = agents_.find(name);
    if (it == agents_.end()) {
        throw AgentNotFoundError(name);
    }

    auto output = it->second->run(input);

    // Store to shared memory.
    if (shared_memory_ && !output.messages.empty()) {
        shared_memory_->add(name, output.messages.back());
    }

    // Emit event.
    emit_event(name, output);

    return output;
}

void MultiAgentOrchestrator::emit_event(const std::string& agent_name,
                                         const AgentOutput& output) {
    if (event_bus_) {
        event_bus_->publish(
            OrchestratorEvent::agent_completed(agent_name, output));
    }
}

// --- MultiAgentOrchestratorBuilder ------------------------------------------

MultiAgentOrchestratorBuilder& MultiAgentOrchestratorBuilder::add_agent(
    const std::string& name, std::unique_ptr<IAgent> agent) {
    order_.push_back(name);
    agents_[name] = std::move(agent);
    return *this;
}

MultiAgentOrchestratorBuilder& MultiAgentOrchestratorBuilder::routing(
    RoutingStrategy strategy) {
    routing_ = std::move(strategy);
    return *this;
}

MultiAgentOrchestratorBuilder& MultiAgentOrchestratorBuilder::shared_memory(
    std::shared_ptr<SharedConversation> memory) {
    shared_memory_ = std::move(memory);
    return *this;
}

MultiAgentOrchestratorBuilder& MultiAgentOrchestratorBuilder::event_bus(
    std::shared_ptr<IEventBus> bus) {
    event_bus_ = std::move(bus);
    return *this;
}

MultiAgentOrchestrator MultiAgentOrchestratorBuilder::build() {
    MultiAgentOrchestrator orch;
    orch.agents_ = std::move(agents_);
    orch.shared_memory_ = std::move(shared_memory_);
    orch.event_bus_ = std::move(event_bus_);

    if (routing_.has_value()) {
        orch.routing_ = std::move(routing_.value());
    } else {
        // Default: sequential in insertion order.
        orch.routing_ = SequentialStrategy{order_};
    }

    return orch;
}

} // namespace agentrs
