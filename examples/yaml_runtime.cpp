// yaml_runtime.cpp — Load a ConfiguredRuntime from YAML.
// C++ equivalent of examples/yaml_runtime.rs

#include <iostream>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
    load_dotenv();

    auto runtime = load_runtime_from_yaml("examples/configs/runtime-agent.yaml");
    auto output = runtime.run("Summarize Rust async best practices");
    std::cout << output.text << "\n";
    return 0;
}
