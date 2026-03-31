// multi_agent.cpp — Sequential multi-agent pipeline.
// C++ equivalent of examples/multi_agent.rs

#include <iostream>
#if defined(_WIN32)
#include <windows.h> // Necessário para SetConsoleOutputCP
#endif

#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
#if defined(_WIN32)
    // Set console output to UTF-8 on Windows
    SetConsoleOutputCP(CP_UTF8);
#else
    std::locale::global(std::locale("en_US.UTF-8"));
#endif

    load_dotenv();

    auto llm = OpenAiProvider::from_env().build();

    auto researcher = Agent::builder()
        .llm(llm->clone_provider())
        .system("You research the topic and return facts.")
        .tool(std::make_unique<WebSearchTool>())
        .build();

    auto writer = Agent::builder()
        .llm(llm)
        .system("You write a polished answer from the input.")
        .build();

    auto orchestrator = MultiAgentOrchestrator::builder()
        .add_agent("researcher", std::move(researcher))
        .add_agent("writer", std::move(writer))
        .routing(SequentialStrategy{{"researcher", "writer"}})
        .build();

    auto output = orchestrator.run("Explain why Rust's ownership helps concurrency");
    std::cout << output.text << "\n";
    return 0;
}
