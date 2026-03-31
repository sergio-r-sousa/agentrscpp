// runner.cpp — AgentRunner implementation (core reasoning loops).
// Equivalent to agentrs-agents/src/runner.rs

#include "agentrs/agents/runner.hpp"
#include "agentrs/agents/builder.hpp"

#include <algorithm>
#include <map>
#include <sstream>
#include <string>
#include <utility>

namespace agentrs {

// --- Construction -----------------------------------------------------------

AgentRunner::AgentRunner(
    std::shared_ptr<ILlmProvider> llm,
    std::unique_ptr<IMemory> memory,
    ToolRegistry tools,
    std::optional<std::string> system_prompt,
    AgentConfig config
)
    : llm_(std::move(llm))
    , memory_(std::move(memory))
    , tools_(std::move(tools))
    , system_prompt_(std::move(system_prompt))
    , config_(std::move(config))
{
}

// --- Static builder shortcut ------------------------------------------------

AgentBuilder AgentRunner::builder() {
    return Agent::builder();
}

// --- IAgent: run ------------------------------------------------------------

AgentOutput AgentRunner::run(std::string_view input) {
    const auto& strategy = config_.loop_strategy;

    switch (strategy.kind) {
        case LoopStrategyKind::ReAct:
            return run_react(input);

        case LoopStrategyKind::CoT:
            return run_cot(input);

        case LoopStrategyKind::PlanAndExecute:
            return run_plan_execute(input, strategy.max_steps);

        case LoopStrategyKind::Custom: {
            std::string augmented_input =
                strategy.custom_instruction + "\n\nUser task: " + std::string(input);
            return run_cot(augmented_input);
        }
    }

    // Unreachable but silences compiler warnings.
    return run_react(input);
}

// --- IAgent: stream_run -----------------------------------------------------

void AgentRunner::stream_run(std::string_view input, EventCallback callback) {
    // Emit thinking event.
    if (!callback(AgentEvent::thinking("completed"))) {
        return;
    }

    // Execute the full run.
    AgentOutput output = run(input);

    // Emit tokens by splitting on whitespace (mirrors Rust implementation).
    std::istringstream stream(output.text);
    std::string word;
    while (stream >> word) {
        if (!callback(AgentEvent::token(word + " "))) {
            return;
        }
    }

    // Emit done event with the full output.
    callback(AgentEvent::done(std::move(output)));
}

// --- ReAct loop -------------------------------------------------------------

AgentOutput AgentRunner::run_react(std::string_view input) {
    memory_->store("user", Message::user(std::string(input)));

    std::size_t max_steps = config_.max_steps;
    if (config_.loop_strategy.kind == LoopStrategyKind::ReAct) {
        max_steps = config_.loop_strategy.max_steps;
    }

    for (std::size_t step = 1; step <= max_steps; ++step) {
        auto history = memory_->history();
        auto request = build_request(history, !tools_.empty());
        auto response = llm_->complete(request);

        Message assistant_message = response.message;
        memory_->store("assistant", assistant_message);

        // Check for tool calls.
        if (assistant_message.tool_calls.has_value() &&
            !assistant_message.tool_calls->empty()) {

            auto tool_results = execute_tool_calls(*assistant_message.tool_calls);
            for (auto& msg : tool_results) {
                memory_->store("tool", msg);
            }
            continue;
        }

        // No tool calls — produce final output.
        return finish_output(response, step);
    }

    throw MaxStepsReached(max_steps);
}

// --- Chain-of-Thought -------------------------------------------------------

AgentOutput AgentRunner::run_cot(std::string_view input) {
    memory_->store("user", Message::user(std::string(input)));

    auto history = memory_->history();
    auto response = llm_->complete(build_request(history, false));

    memory_->store("assistant", response.message);

    return finish_output(response, 1);
}

// --- Plan and Execute -------------------------------------------------------

AgentOutput AgentRunner::run_plan_execute(std::string_view input, std::size_t max_steps) {
    // Step 1: Generate a plan.
    std::string planner_prompt =
        "Create a concise numbered plan to solve the user task. Task: " + std::string(input);

    CompletionRequest plan_request;
    plan_request.messages = {Message::user(planner_prompt)};
    plan_request.tools = std::nullopt;
    plan_request.model = config_.model;
    plan_request.temperature = 0.1f;
    plan_request.max_tokens = config_.max_tokens;
    plan_request.stream = false;
    plan_request.system = system_prompt_;

    auto plan_response = llm_->complete(plan_request);

    memory_->store("plan", Message::assistant(plan_response.message.text_content()));

    // Step 2: Execute the plan via ReAct.
    std::string execution_prompt =
        "Use this plan to solve the task.\nPlan:\n" +
        plan_response.message.text_content() +
        "\n\nTask: " + std::string(input);

    memory_->store("user", Message::user(execution_prompt));

    auto output = run_react(input);

    // Adjust steps (mirrors Rust: output.steps = output.steps.max(max_steps.min(output.steps.max(1))))
    std::size_t min_steps = std::min(max_steps, std::max(output.steps, std::size_t{1}));
    output.steps = std::max(output.steps, min_steps);

    return output;
}

// --- build_request ----------------------------------------------------------

CompletionRequest AgentRunner::build_request(
    const std::vector<Message>& history,
    bool include_tools
) const {
    CompletionRequest request;
    request.messages = history;
    request.model = config_.model;
    request.temperature = config_.temperature;
    request.max_tokens = config_.max_tokens;
    request.stream = false;
    request.system = system_prompt_;

    if (include_tools) {
        request.tools = tools_.definitions();
    } else {
        request.tools = std::nullopt;
    }

    return request;
}

// --- execute_tool_calls -----------------------------------------------------

std::vector<Message> AgentRunner::execute_tool_calls(
    const std::vector<ToolCall>& tool_calls
) {
    std::vector<Message> results;
    results.reserve(tool_calls.size());

    for (const auto& tc : tool_calls) {
        ToolOutput output;
        try {
            output = tools_.call(tc.name, tc.arguments);
        } catch (const std::exception& e) {
            output = ToolOutput::error(e.what());
        }
        results.push_back(Message::tool_result(tc.id, tc.name, output));
    }

    return results;
}

// --- finish_output ----------------------------------------------------------

AgentOutput AgentRunner::finish_output(
    const CompletionResponse& response,
    std::size_t steps
) const {
    auto history = memory_->history();

    AgentOutput output;
    output.text = response.message.text_content();
    output.steps = steps;
    output.usage = response.usage;
    output.messages = std::move(history);
    return output;
}

} // namespace agentrs
