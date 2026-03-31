// integration_test.cpp — Integration tests for agentrs C++ SDK.
// C++ equivalent of tests/integration_test.rs
//
// Uses simple assert-based testing (no external test framework required).
// Each test function returns 0 on success, throws on failure.
// The CMakeLists.txt already wires tests/*.cpp into CTest.

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------

static int tests_run = 0;
static int tests_passed = 0;

#define RUN_TEST(fn)                                                      \
    do {                                                                   \
        ++tests_run;                                                       \
        try {                                                              \
            fn();                                                          \
            ++tests_passed;                                                \
            std::cout << "  PASS  " << #fn << "\n";                        \
        } catch (const std::exception& e) {                                \
            std::cerr << "  FAIL  " << #fn << ": " << e.what() << "\n";    \
        } catch (...) {                                                    \
            std::cerr << "  FAIL  " << #fn << ": unknown exception\n";     \
        }                                                                  \
    } while (0)

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

/// Loads a single agent from a YAML string.
void test_loads_agent_from_yaml_string() {
    const char* yaml = R"(
name: yaml-agent
llm:
  provider: open_ai
  api_key: test-key
  base_url: http://localhost:1234/v1
  model: test-model
system: You are loaded from YAML.
tools:
  - type: calculator
loop_strategy:
  type: chain_of_thought
)";

    auto agent = load_agent_from_yaml_str(yaml);
    assert(agent != nullptr);
}

/// Loads a multi-agent orchestrator from a YAML string.
void test_loads_multi_agent_from_yaml_string() {
    const char* yaml = R"(
agents:
  - name: first
    llm:
      provider: open_ai
      api_key: test-key
      base_url: http://localhost:1234/v1
      model: test-model
    system: First agent.
  - name: second
    llm:
      provider: open_ai
      api_key: test-key
      base_url: http://localhost:1234/v1
      model: test-model
    system: Second agent.
routing:
  type: parallel
  agents:
    - first
    - second
)";

    auto orchestrator = load_multi_agent_from_yaml_str(yaml);
    auto names = orchestrator.agent_names();
    assert(names.size() == 2);
}

/// Loads a runtime from a YAML string (kind: agent).
void test_loads_runtime_from_yaml_string() {
    const char* yaml = R"(
kind: agent
llm:
  provider: open_ai
  api_key: test-key
  base_url: http://localhost:1234/v1
  model: test-model
tools:
  - type: calculator
)";

    auto runtime = load_runtime_from_yaml_str(yaml);
    assert(runtime.kind() == ConfiguredRuntime::Kind::Agent);
}

/// Agent executes tool calls using a mock LLM provider.
void test_agent_executes_tool_calls() {
    auto llm = std::make_shared<MockLlmProvider>(
        MockLlmProvider::with_tool_call_sequence(
            "calculator",
            nlohmann::json{{"expression", "2 + 2"}},
            "The answer is 4."));

    auto agent = Agent::builder()
        .llm(llm)
        .tool(std::make_unique<CalculatorTool>())
        .build();

    auto output = agent->run("What is 2 + 2?");
    assert(output.text.find('4') != std::string::npos);
    assert(output.steps >= 1);
}

/// Sequential multi-agent pipeline with mock providers.
void test_sequential_multi_agent_pipeline() {
    auto researcher_llm = std::make_shared<MockLlmProvider>(
        MockLlmProvider::with_text_responses({
            "Rust uses ownership to prevent data races.",
        }));

    auto writer_llm = std::make_shared<MockLlmProvider>(
        MockLlmProvider::with_text_responses({
            "Final answer: ownership enables safe concurrency.",
        }));

    auto researcher = Agent::builder()
        .llm(researcher_llm)
        .build();

    auto writer = Agent::builder()
        .llm(writer_llm)
        .build();

    auto orchestrator = MultiAgentOrchestrator::builder()
        .add_agent("researcher", std::move(researcher))
        .add_agent("writer", std::move(writer))
        .routing(SequentialStrategy{{"researcher", "writer"}})
        .build();

    auto output = orchestrator.run("Explain Rust concurrency");
    assert(output.text.find("Final answer") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Main
// ---------------------------------------------------------------------------

int main() {
    std::cout << "Running integration tests...\n\n";

    RUN_TEST(test_loads_agent_from_yaml_string);
    RUN_TEST(test_loads_multi_agent_from_yaml_string);
    RUN_TEST(test_loads_runtime_from_yaml_string);
    RUN_TEST(test_agent_executes_tool_calls);
    RUN_TEST(test_sequential_multi_agent_pipeline);

    std::cout << "\n" << tests_passed << "/" << tests_run << " tests passed.\n";
    return (tests_passed == tests_run) ? 0 : 1;
}
