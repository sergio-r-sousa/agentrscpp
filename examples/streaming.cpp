// streaming.cpp — Streaming agent events.
// C++ equivalent of examples/streaming.rs
//
// Rust uses an async stream (Stream trait). In C++ the streaming API is
// callback-based: stream_run() calls the callback for each AgentEvent.

#include <iostream>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
    load_dotenv();

    auto llm = OpenAiProvider::from_env()
        .model("openai/gpt-oss-20b")
        .build();

    auto agent = Agent::builder()
        .llm(llm)
        .build();

    agent->stream_run("Say hello in five words", [](const AgentEvent& event) -> bool {
        switch (event.type) {
            case AgentEvent::Type::Token:
                std::cout << event.text << std::flush;
                break;
            case AgentEvent::Type::Done:
                std::cout << "\n";
                break;
            default:
                break;
        }
        return true; // continue streaming
    });

    return 0;
}
