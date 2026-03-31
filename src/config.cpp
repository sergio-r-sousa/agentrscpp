// config.cpp — YAML-driven agent and orchestrator configuration.
// Implementation of agentrs/config.hpp
//
// Uses yaml-cpp to parse YAML files and build agents,
// multi-agent orchestrators, or runtime configurations.

#include "agentrs/config.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

#include <yaml-cpp/yaml.h>

#include "agentrs/agents/builder.hpp"
#include "agentrs/agents/runner.hpp"
#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/llm/anthropic.hpp"
#include "agentrs/llm/azureopenai.hpp"
#include "agentrs/llm/gemini.hpp"
#include "agentrs/llm/ollama.hpp"
#include "agentrs/llm/openai.hpp"
#include "agentrs/mcp/client.hpp"
#include "agentrs/memory/in_memory.hpp"
#include "agentrs/memory/sliding_window.hpp"
#include "agentrs/memory/token_aware.hpp"
#include "agentrs/memory/vector.hpp"
#include "agentrs/tools/builtin.hpp"
#include "agentrs/tools/registry.hpp"

namespace agentrs {

// ---------------------------------------------------------------------------
// YAML → config struct helpers (yaml-cpp)
// ---------------------------------------------------------------------------

namespace {

/// Reads an optional string from a YAML node.
std::optional<std::string> opt_string(const YAML::Node& node, const std::string& key) {
    if (node[key] && !node[key].IsNull()) {
        return node[key].as<std::string>();
    }
    return std::nullopt;
}

/// Reads an optional size_t from a YAML node.
std::optional<std::size_t> opt_size(const YAML::Node& node, const std::string& key) {
    if (node[key] && !node[key].IsNull()) {
        return node[key].as<std::size_t>();
    }
    return std::nullopt;
}

/// Reads an optional float from a YAML node.
std::optional<float> opt_float(const YAML::Node& node, const std::string& key) {
    if (node[key] && !node[key].IsNull()) {
        return node[key].as<float>();
    }
    return std::nullopt;
}

/// Reads an optional uint32_t from a YAML node.
std::optional<uint32_t> opt_uint32(const YAML::Node& node, const std::string& key) {
    if (node[key] && !node[key].IsNull()) {
        return node[key].as<uint32_t>();
    }
    return std::nullopt;
}

/// Reads a string-to-string map from a YAML node.
std::map<std::string, std::string> read_string_map(const YAML::Node& node) {
    std::map<std::string, std::string> result;
    if (node && node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            result[it->first.as<std::string>()] = it->second.as<std::string>();
        }
    }
    return result;
}

/// Reads a vector of strings from a YAML node.
std::vector<std::string> read_string_vec(const YAML::Node& node) {
    std::vector<std::string> result;
    if (node && node.IsSequence()) {
        for (const auto& item : node) {
            result.push_back(item.as<std::string>());
        }
    }
    return result;
}

/// Parses LlmYamlConfig from a YAML node.
LlmYamlConfig parse_llm_config(const YAML::Node& node) {
    LlmYamlConfig cfg;
    if (!node["provider"]) {
        throw InvalidConfigurationError("llm.provider is required");
    }
    cfg.provider = node["provider"].as<std::string>();
    cfg.api_key = opt_string(node, "api_key");
    cfg.base_url = opt_string(node, "base_url");
    cfg.model = opt_string(node, "model");
    return cfg;
}

/// Parses ToolYamlConfig from a YAML node.
ToolYamlConfig parse_tool_config(const YAML::Node& node) {
    ToolYamlConfig cfg;
    if (!node["type"]) {
        throw InvalidConfigurationError("tool.type is required");
    }
    cfg.type = node["type"].as<std::string>();
    cfg.root = opt_string(node, "root");
    cfg.target = opt_string(node, "target");
    cfg.api_key = opt_string(node, "api_key");
    cfg.api_key_header = opt_string(node, "api_key_header");
    cfg.api_key_prefix = opt_string(node, "api_key_prefix");

    if (node["headers"] && node["headers"].IsMap()) {
        cfg.headers = read_string_map(node["headers"]);
    }

    return cfg;
}

/// Parses MemoryYamlConfig from a YAML node.
MemoryYamlConfig parse_memory_config(const YAML::Node& node) {
    MemoryYamlConfig cfg;
    if (!node || node.IsNull()) {
        return cfg; // defaults to in_memory
    }
    if (node["type"]) {
        cfg.type = node["type"].as<std::string>();
    }
    cfg.window_size = opt_size(node, "window_size");
    cfg.max_tokens = opt_size(node, "max_tokens");
    return cfg;
}

/// Parses LoopStrategyYamlConfig from a YAML node.
LoopStrategyYamlConfig parse_loop_strategy_config(const YAML::Node& node) {
    LoopStrategyYamlConfig cfg;
    if (!node || node.IsNull()) {
        return cfg; // defaults to re_act
    }
    if (node["type"]) {
        cfg.type = node["type"].as<std::string>();
    }
    cfg.max_steps = opt_size(node, "max_steps");
    cfg.instruction = opt_string(node, "instruction");
    return cfg;
}

/// Parses RoutingYamlConfig from a YAML node.
RoutingYamlConfig parse_routing_config(const YAML::Node& node) {
    RoutingYamlConfig cfg;
    if (!node || node.IsNull()) {
        return cfg; // defaults to sequential
    }
    if (node["type"]) {
        cfg.type = node["type"].as<std::string>();
    }
    if (node["order"] && node["order"].IsSequence()) {
        cfg.order = read_string_vec(node["order"]);
    }
    if (node["agents"] && node["agents"].IsSequence()) {
        cfg.agents = read_string_vec(node["agents"]);
    }
    return cfg;
}

/// Parses AgentYamlConfig from a YAML node.
AgentYamlConfig parse_agent_config(const YAML::Node& node) {
    AgentYamlConfig cfg;
    cfg.name = opt_string(node, "name");

    if (!node["llm"]) {
        throw InvalidConfigurationError("agent.llm is required");
    }
    cfg.llm = parse_llm_config(node["llm"]);
    cfg.system = opt_string(node, "system");
    cfg.memory = parse_memory_config(node["memory"]);

    if (node["tools"] && node["tools"].IsSequence()) {
        for (const auto& tool_node : node["tools"]) {
            cfg.tools.push_back(parse_tool_config(tool_node));
        }
    }

    cfg.loop_strategy = parse_loop_strategy_config(node["loop_strategy"]);
    cfg.model = opt_string(node, "model");
    cfg.temperature = opt_float(node, "temperature");
    cfg.max_tokens = opt_uint32(node, "max_tokens");
    cfg.max_steps = opt_size(node, "max_steps");

    return cfg;
}

/// Parses MultiAgentYamlConfig from a YAML node.
MultiAgentYamlConfig parse_multi_agent_config(const YAML::Node& node) {
    MultiAgentYamlConfig cfg;

    if (!node["agents"] || !node["agents"].IsSequence()) {
        throw InvalidConfigurationError("multi_agent.agents is required and must be a list");
    }

    for (const auto& agent_node : node["agents"]) {
        NamedAgentYamlConfig named;
        if (!agent_node["name"]) {
            throw InvalidConfigurationError("each agent in agents list requires a 'name' field");
        }
        named.name = agent_node["name"].as<std::string>();
        named.agent = parse_agent_config(agent_node);
        cfg.agents.push_back(std::move(named));
    }

    cfg.routing = parse_routing_config(node["routing"]);
    return cfg;
}

/// Parses RuntimeConfig from a YAML node.
RuntimeConfig parse_runtime_config(const YAML::Node& root) {
    RuntimeConfig cfg;

    if (!root["kind"]) {
        throw InvalidConfigurationError("top-level 'kind' field is required (agent or multi_agent)");
    }

    cfg.kind = root["kind"].as<std::string>();

    if (cfg.kind == "agent") {
        cfg.agent = parse_agent_config(root);
    } else if (cfg.kind == "multi_agent") {
        cfg.multi_agent = parse_multi_agent_config(root);
    } else {
        throw InvalidConfigurationError("unknown kind: " + cfg.kind +
                                         " (expected 'agent' or 'multi_agent')");
    }

    return cfg;
}

/// Reads an entire file into a string.
std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw IoError("Could not open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

} // anonymous namespace

// ---------------------------------------------------------------------------
// LoopStrategyYamlConfig::into_loop_strategy
// ---------------------------------------------------------------------------

LoopStrategy LoopStrategyYamlConfig::into_loop_strategy() const {
    if (type == "re_act") {
        return LoopStrategy::react(max_steps.value_or(8));
    }
    if (type == "chain_of_thought") {
        return LoopStrategy::cot();
    }
    if (type == "plan_and_execute") {
        return LoopStrategy::plan_and_execute(max_steps.value_or(8));
    }
    if (type == "custom") {
        return LoopStrategy::custom(instruction.value_or(""));
    }
    throw InvalidConfigurationError("unknown loop_strategy type: " + type);
}

// ---------------------------------------------------------------------------
// RoutingYamlConfig::into_routing_strategy
// ---------------------------------------------------------------------------

RoutingStrategy RoutingYamlConfig::into_routing_strategy(
    const std::vector<std::string>& default_order) const
{
    if (type == "sequential") {
        SequentialStrategy seq;
        seq.agents = order.value_or(default_order);
        return seq;
    }
    if (type == "parallel") {
        ParallelStrategy par;
        par.agents = agents.value_or(default_order);
        return par;
    }
    throw InvalidConfigurationError("unknown routing type: " + type);
}

// ---------------------------------------------------------------------------
// detail::build_llm
// ---------------------------------------------------------------------------

namespace detail {

std::shared_ptr<ILlmProvider> build_llm(const LlmYamlConfig& config) {
    const auto& provider = config.provider;

    if (provider == "open_ai") {
        auto builder = OpenAiProvider::from_env();
        if (config.api_key) builder.api_key(*config.api_key);
        if (config.base_url) builder.base_url(*config.base_url);
        if (config.model) builder.model(*config.model);
        return builder.build();
    }

    if (provider == "azure_open_ai") {
        auto builder = AzureOpenAiProvider::from_env();
        if (config.api_key) builder.api_key(*config.api_key);
        if (config.base_url) builder.base_url(*config.base_url);
        if (config.model) builder.model(*config.model);
        return builder.build();
    }

    if (provider == "anthropic") {
        auto builder = AnthropicProvider::from_env();
        if (config.api_key) builder.api_key(*config.api_key);
        if (config.base_url) builder.base_url(*config.base_url);
        if (config.model) builder.model(*config.model);
        return builder.build();
    }

    if (provider == "gemini") {
        auto builder = GeminiProvider::from_env();
        if (config.api_key) builder.api_key(*config.api_key);
        if (config.base_url) builder.base_url(*config.base_url);
        if (config.model) builder.model(*config.model);
        return builder.build();
    }

    if (provider == "ollama") {
        auto builder = OllamaProvider::builder();
        if (config.base_url) builder.base_url(*config.base_url);
        if (config.model) builder.model(*config.model);
        return builder.build();
    }

    throw InvalidConfigurationError(
        "unknown LLM provider: " + provider +
        " (expected open_ai, azure_open_ai, anthropic, gemini, or ollama)");
}

// ---------------------------------------------------------------------------
// detail::build_tools
// ---------------------------------------------------------------------------

ToolRegistry build_tools(const std::vector<ToolYamlConfig>& configs) {
    ToolRegistry registry;

    for (const auto& cfg : configs) {
        if (cfg.type == "calculator") {
            registry.register_tool(std::make_unique<CalculatorTool>());
        } else if (cfg.type == "web_fetch") {
            registry.register_tool(std::make_unique<WebFetchTool>());
        } else if (cfg.type == "web_search") {
            registry.register_tool(std::make_unique<WebSearchTool>());
        } else if (cfg.type == "file_read") {
            registry.register_tool(std::make_unique<FileReadTool>());
        } else if (cfg.type == "file_write") {
            registry.register_tool(std::make_unique<FileWriteTool>());
        } else if (cfg.type == "mcp") {
            if (!cfg.target) {
                throw InvalidConfigurationError("mcp tool requires a 'target' field");
            }
            const auto& target = *cfg.target;

            // Check if target is an HTTP endpoint.
            if (target.rfind("http://", 0) == 0 || target.rfind("https://", 0) == 0) {
                // Build WebMcpOptions for the registry's HTTP registration.
                agentrs::WebMcpOptions opts;
                if (cfg.api_key) {
                    opts.api_key(*cfg.api_key);
                }
                if (cfg.api_key_header) {
                    opts.api_key_header(*cfg.api_key_header);
                }
                if (cfg.api_key_prefix) {
                    opts.api_key_prefix(*cfg.api_key_prefix);
                }
                registry.register_mcp_http_with_options(target, opts);
            } else {
                // Treat as stdio command.
                registry.register_mcp_stdio(target);
            }
        } else {
            throw InvalidConfigurationError("unknown tool type: " + cfg.type);
        }
    }

    return registry;
}

} // namespace detail

// ---------------------------------------------------------------------------
// AgentYamlConfig::build
// ---------------------------------------------------------------------------

std::unique_ptr<IAgent> AgentYamlConfig::build() const {
    auto llm_provider = detail::build_llm(this->llm);
    auto tool_registry = detail::build_tools(tools);
    auto strategy = loop_strategy.into_loop_strategy();

    auto builder = Agent::builder();
    builder.llm(llm_provider);
    builder.tools(std::move(tool_registry));
    builder.loop_strategy(strategy);

    if (system) builder.system(*system);
    if (model) builder.model(*model);
    if (temperature) builder.temperature(*temperature);
    if (max_tokens) builder.max_tokens(*max_tokens);
    if (max_steps) builder.max_steps(*max_steps);

    // Set memory backend based on config.
    if (memory.type == "sliding_window") {
        builder.memory(std::make_unique<SlidingWindowMemory>(
            memory.window_size.value_or(10)));
    } else if (memory.type == "token_aware") {
        builder.memory(std::make_unique<TokenAwareMemory>(
            memory.max_tokens.value_or(4096)));
    } else if (memory.type == "vector") {
        builder.memory(std::make_unique<VectorMemory>());
    } else {
        // "in_memory" or default — use the builder's default InMemoryMemory.
        builder.memory(std::make_unique<InMemoryMemory>());
    }

    return builder.build();
}

// ---------------------------------------------------------------------------
// MultiAgentYamlConfig::build
// ---------------------------------------------------------------------------

MultiAgentOrchestrator MultiAgentYamlConfig::build() const {
    auto orch_builder = MultiAgentOrchestrator::builder();

    std::vector<std::string> default_order;
    default_order.reserve(agents.size());

    for (const auto& named : agents) {
        default_order.push_back(named.name);
        orch_builder.add_agent(named.name, named.agent.build());
    }

    orch_builder.routing(routing.into_routing_strategy(default_order));
    return orch_builder.build();
}

// ---------------------------------------------------------------------------
// ConfiguredRuntime
// ---------------------------------------------------------------------------

ConfiguredRuntime::ConfiguredRuntime(std::unique_ptr<IAgent> agent)
    : kind_(Kind::Agent), agent_(std::move(agent)) {}

ConfiguredRuntime::ConfiguredRuntime(MultiAgentOrchestrator orchestrator)
    : kind_(Kind::Multi), orchestrator_(std::move(orchestrator)) {}

AgentOutput ConfiguredRuntime::run(const std::string& input) {
    switch (kind_) {
        case Kind::Agent:
            return agent_->run(input);
        case Kind::Multi:
            return orchestrator_->run(input);
    }
    throw AgentError("unreachable: unknown ConfiguredRuntime kind");
}

// ---------------------------------------------------------------------------
// Free functions
// ---------------------------------------------------------------------------

std::unique_ptr<IAgent> load_agent_from_yaml(const std::string& path) {
    auto content = read_file(path);
    return load_agent_from_yaml_str(content);
}

std::unique_ptr<IAgent> load_agent_from_yaml_str(const std::string& yaml) {
    try {
        YAML::Node root = YAML::Load(yaml);
        auto cfg = parse_agent_config(root);
        return cfg.build();
    } catch (const YAML::Exception& e) {
        throw InvalidConfigurationError(std::string("YAML parse error: ") + e.what());
    }
}

MultiAgentOrchestrator load_multi_agent_from_yaml(const std::string& path) {
    auto content = read_file(path);
    return load_multi_agent_from_yaml_str(content);
}

MultiAgentOrchestrator load_multi_agent_from_yaml_str(const std::string& yaml) {
    try {
        YAML::Node root = YAML::Load(yaml);
        auto cfg = parse_multi_agent_config(root);
        return cfg.build();
    } catch (const YAML::Exception& e) {
        throw InvalidConfigurationError(std::string("YAML parse error: ") + e.what());
    }
}

ConfiguredRuntime load_runtime_from_yaml(const std::string& path) {
    auto content = read_file(path);
    return load_runtime_from_yaml_str(content);
}

ConfiguredRuntime load_runtime_from_yaml_str(const std::string& yaml) {
    try {
        YAML::Node root = YAML::Load(yaml);
        auto runtime_cfg = parse_runtime_config(root);

        if (runtime_cfg.kind == "agent" && runtime_cfg.agent) {
            return ConfiguredRuntime(runtime_cfg.agent->build());
        }
        if (runtime_cfg.kind == "multi_agent" && runtime_cfg.multi_agent) {
            return ConfiguredRuntime(runtime_cfg.multi_agent->build());
        }

        throw InvalidConfigurationError("invalid runtime config: kind=" + runtime_cfg.kind);
    } catch (const YAML::Exception& e) {
        throw InvalidConfigurationError(std::string("YAML parse error: ") + e.what());
    }
}

} // namespace agentrs
