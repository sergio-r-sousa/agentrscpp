// yaml_single_agent.cpp — Load a single agent from a YAML config file.
// C++ equivalent of examples/yaml_single_agent.rs

#include <iostream>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
    load_dotenv();

    auto agent = load_agent_from_yaml("examples/configs/single-agent.yaml");
    auto output = agent->run("What is 21 * 2?");
    std::cout << output.text << "\n";
    return 0;
}
