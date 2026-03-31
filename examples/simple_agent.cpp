// simple_agent.cpp — Minimal agent with a calculator tool.
// C++ equivalent of examples/simple_agent.rs

#include <iostream>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
    load_dotenv();

    auto llm = OpenAiProvider::from_env().build();

    auto agent = Agent::builder()
        .llm(llm)
        .system("You are a concise assistant.")
        .tool(std::make_unique<CalculatorTool>())
        .loop_strategy(LoopStrategy::react(4))
        .build();

    auto output = agent->run("What is 7 * (8 + 1)?");
    std::cout << output.text << "\n";
    return 0;
}
