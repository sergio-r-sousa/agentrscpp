// builder.cpp — AgentBuilder implementation.
// Equivalent to agentrs-agents/src/builder.rs

#include "agentrs/agents/builder.hpp"
#include "agentrs/agents/runner.hpp"
#include "agentrs/memory/in_memory.hpp"

#include <stdexcept>
#include <utility>

namespace agentrs {

// --- Agent ------------------------------------------------------------------

AgentBuilder Agent::builder() {
    return AgentBuilder();
}

// --- AgentBuilder -----------------------------------------------------------

AgentBuilder::AgentBuilder()
    : memory_(std::make_unique<InMemoryMemory>())
    , config_{}
{
}

AgentBuilder& AgentBuilder::llm(std::shared_ptr<ILlmProvider> provider) {
    llm_ = std::move(provider);
    return *this;
}

AgentBuilder& AgentBuilder::system(std::string prompt) {
    system_prompt_ = std::move(prompt);
    return *this;
}

AgentBuilder& AgentBuilder::tool(std::unique_ptr<ITool> t) {
    tools_.register_tool(std::move(t));
    return *this;
}

AgentBuilder& AgentBuilder::tools(ToolRegistry registry) {
    tools_ = std::move(registry);
    return *this;
}

AgentBuilder& AgentBuilder::memory(std::unique_ptr<IMemory> mem) {
    memory_ = std::move(mem);
    return *this;
}

AgentBuilder& AgentBuilder::model(std::string model_name) {
    config_.model = std::move(model_name);
    return *this;
}

AgentBuilder& AgentBuilder::loop_strategy(LoopStrategy strategy) {
    config_.max_steps = strategy.max_steps_hint(config_.max_steps);
    config_.loop_strategy = std::move(strategy);
    return *this;
}

AgentBuilder& AgentBuilder::temperature(float t) {
    config_.temperature = t;
    return *this;
}

AgentBuilder& AgentBuilder::max_tokens(uint32_t tokens) {
    config_.max_tokens = tokens;
    return *this;
}

AgentBuilder& AgentBuilder::max_steps(std::size_t steps) {
    config_.max_steps = steps;
    return *this;
}

std::unique_ptr<AgentRunner> AgentBuilder::build() {
    if (!llm_) {
        throw NoLlmProviderError();
    }

    return std::make_unique<AgentRunner>(
        std::move(llm_),
        std::move(memory_),
        std::move(tools_),
        std::move(system_prompt_),
        std::move(config_)
    );
}

} // namespace agentrs
