// yaml_multi_agent.cpp — Load a multi-agent orchestrator from YAML.
// C++ equivalent of examples/yaml_multi_agent.rs

#include <iostream>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
    load_dotenv();

    auto orchestrator = load_multi_agent_from_yaml("examples/configs/multi-agent.yaml");
    auto output = orchestrator.run("Research and explain how Tokio tasks are spawned");
    std::cout << output.text << "\n";
    return 0;
}
