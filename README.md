# agentrscpp

`agentrscpp` is a C++17 SDK for building AI agents, ported from the [agentrs](https://github.com/anthropics/agentrs) Rust SDK. It provides a unified provider interface, composable tools, Model Context Protocol (MCP) integration, configurable memory, multi-agent orchestration, and YAML-driven runtime loading — all compiled with GCC and linked as a static library.

The SDK is designed for developers who want a clean C++ API without giving up architectural flexibility. You can start with a single assistant in a few lines, then scale to tool-using agents, MCP integrations, and orchestrated multi-agent workflows without changing the core programming model.

## Why agentrscpp

- Provider-agnostic abstractions for OpenAI, Azure OpenAI, Anthropic, Gemini, and Ollama
- Synchronous execution with callback-based streaming (no async runtime required)
- Tool calling with built-in tools and MCP tool adapters
- Memory backends for simple chat history, sliding windows, token budgets, and vector retrieval
- Multiple agent loop strategies: ReAct, chain-of-thought, and plan-and-execute
- Multi-agent orchestration with sequential, parallel, supervisor, and graph routing
- YAML configuration support for loading agents and orchestrators without recompilation
- CMake-based build with FetchContent for all dependencies

## Architecture

```text
agentrscpp/
├── include/agentrs/
│   ├── core/           # Core interfaces, types, error hierarchy, streaming
│   ├── llm/            # LLM providers: OpenAI, Azure, Anthropic, Gemini, Ollama
│   ├── tools/          # Tool registry and built-in tools
│   ├── mcp/            # MCP protocol, stdio/HTTP clients, tool adapter
│   ├── memory/         # Memory backends: InMemory, SlidingWindow, TokenAware, Vector, Redis
│   ├── agents/         # Agent builder and runner (ReAct, CoT, PlanAndExecute)
│   ├── multi/          # Multi-agent orchestrator, communication, graph routing
│   ├── agentrs.hpp     # Facade header (include everything)
│   ├── config.hpp      # YAML configuration loader
│   └── result.hpp      # Result<T,E> type and TRY macro
├── src/                # Implementation files (mirrors include structure)
├── examples/           # 8 runnable examples + YAML configs
├── tests/              # Integration tests
├── CMakeLists.txt      # Build system
└── Makefile            # Convenience wrapper
```

### Core Design Principles

- `core/` defines the contracts: `ILlmProvider`, `ITool`, `IMemory`, and `IAgent`
- `llm/` adapts concrete model providers into a common chat-completion interface
- `tools/` keeps tool execution decoupled from providers and agent loops
- `mcp/` lets the SDK consume both local MCP servers and remote web MCP endpoints
- `memory/` keeps memory as a pluggable concern rather than coupling it into the runner
- `agents/` focuses on agent behavior and loop control
- `multi/` composes multiple agents without changing the single-agent contract
- `agentrs.hpp` and `config.hpp` provide the top-level API and YAML loading

## Technologies Used

- **C++17** compiled with GCC
- **nlohmann/json** for JSON serialization (replaces serde_json)
- **yaml-cpp** for YAML configuration (replaces serde_yaml)
- **cpp-httplib** with OpenSSL for HTTP transport (replaces reqwest)
- **std::async / std::future** for parallel execution (replaces tokio)
- Custom `Result<T,E>` and `TRY()` macro for error propagation (replaces Rust `?`)

## Requirements

- GCC 9+ (C++17 support)
- CMake 3.16+
- OpenSSL development headers (for HTTPS in cpp-httplib)
- Internet connection for FetchContent on first build

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

Debug build with sanitizers:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

Build with tests:

```bash
cmake .. -DAGENTRS_BUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

Using the Makefile wrapper:

```bash
make            # Release build
make debug      # Debug build with sanitizers
make test       # Build and run tests
make clean      # Remove build artifacts
```

### CMake Options

| Option | Default | Description |
|---|---|---|
| `AGENTRS_BUILD_EXAMPLES` | `ON` | Build example executables |
| `AGENTRS_BUILD_TESTS` | `OFF` | Build test executables |
| `AGENTRS_ENABLE_REDIS` | `OFF` | Enable Redis memory backend (requires hiredis) |

## Quick Start

```cpp
#include <iostream>
#include <agentrs/agentrs.hpp>

using namespace agentrs;

int main() {
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
```

## Provider Integrations

All providers implement the `ILlmProvider` interface and return `std::shared_ptr`.

### OpenAI

```cpp
auto llm = OpenAiProvider::from_env()
    .model("gpt-4o-mini")
    .build();
```

### Azure OpenAI

```cpp
auto llm = AzureOpenAiProvider::from_env()
    .base_url("https://your-resource.openai.azure.com/openai/v1")
    .model("gpt-4o")
    .build();
```

### Anthropic

```cpp
auto llm = AnthropicProvider::from_env()
    .model("claude-3-5-sonnet-latest")
    .build();
```

### Gemini

```cpp
auto llm = GeminiProvider::from_env()
    .model("gemini-2.0-flash")
    .build();
```

### Ollama

```cpp
auto llm = OllamaProvider::builder()
    .base_url("http://localhost:11434/v1")
    .model("llama3.2")
    .build();
```

## Tools

Tools implement the `ITool` interface and are managed by a `ToolRegistry`.

### Built-in Tools

- `CalculatorTool` — Evaluates arithmetic expressions
- `WebFetchTool` — Fetches HTTP content from a URL
- `WebSearchTool` — Searches using DuckDuckGo instant answers
- `FileReadTool` — Reads files from allowed paths
- `FileWriteTool` — Writes files to allowed paths
- `BashTool` — Executes shell commands
- `PythonTool` — Executes inline Python code

### Registering Tools

```cpp
ToolRegistry tools;
tools.register_tool(std::make_unique<CalculatorTool>())
     .register_tool(std::make_unique<WebSearchTool>())
     .register_tool(std::make_unique<FileReadTool>());
```

### Custom Tools

Implement the `ITool` interface:

```cpp
class ReverseTextTool : public ITool {
public:
    std::string name() const override { return "reverse_text"; }
    std::string description() const override { return "Reverse a string"; }
    nlohmann::json schema() const override {
        return {{"type", "object"},
                {"properties", {{"text", {{"type", "string"}}}}},
                {"required", {"text"}}};
    }
    ToolOutput call(const nlohmann::json& input) override {
        std::string text = input["text"];
        std::reverse(text.begin(), text.end());
        return ToolOutput::text(text);
    }
};
```

## Memory Backends

Memory is pluggable and independent from the agent loop. All backends implement `IMemory`.

### In-Memory History

```cpp
auto memory = std::make_unique<InMemoryMemory>();
```

### Sliding Window Memory

```cpp
auto memory = std::make_unique<SlidingWindowMemory>(12);
```

### Token-Aware Memory

```cpp
auto memory = std::make_unique<TokenAwareMemory>(4000);
```

### Vector Memory

```cpp
auto memory = std::make_unique<VectorMemory>();
```

## Agent Execution Strategies

### ReAct

Best when the model may need to call tools iteratively.

```cpp
auto agent = Agent::builder()
    .llm(llm)
    .tools(std::move(tools))
    .loop_strategy(LoopStrategy::react(8))
    .build();
```

### Chain-of-Thought (Single Pass)

Best for direct reasoning without tools.

```cpp
auto agent = Agent::builder()
    .llm(llm)
    .loop_strategy(LoopStrategy::cot())
    .build();
```

### Plan and Execute

Useful for tasks that benefit from an explicit planning stage.

```cpp
auto agent = Agent::builder()
    .llm(llm)
    .tools(std::move(tools))
    .loop_strategy(LoopStrategy::plan_and_execute(6))
    .build();
```

### Streaming Runs

Streaming uses a callback-based approach instead of async iterators:

```cpp
agent->stream_run("Say hello in five words", [](const AgentEvent& event) {
    switch (event.type) {
        case AgentEvent::Type::Token:
            std::cout << event.text;
            break;
        case AgentEvent::Type::Done:
            std::cout << "\nDone: " << event.output.text << "\n";
            break;
        default:
            break;
    }
    return true; // return false to cancel
});
```

## MCP Integration

`agentrscpp` supports both local MCP servers over stdio and remote MCP endpoints over Streamable HTTP.

### Local MCP Server

```cpp
auto tools = ToolRegistry();
tools.register_mcp_stdio("npx -y @modelcontextprotocol/server-filesystem .");
```

### Remote MCP Endpoint

```cpp
auto tools = ToolRegistry();
tools.register_mcp_http_with_options("https://mcp.context7.com/mcp");
```

### Remote MCP with API Key

```cpp
WebMcpOptions options;
options.api_key("your-api-key")
       .api_key_header("X-Context7-API-Key")
       .api_key_prefix("");

auto tools = ToolRegistry();
tools.register_mcp_http_with_options("https://mcp.context7.com/mcp", options);
```

## Multi-Agent Orchestration

The multi-agent module composes existing agents into higher-level workflows.

### Sequential Routing

```cpp
auto researcher = Agent::builder()
    .llm(OpenAiProvider::from_env().model("gpt-4o-mini").build())
    .system("You gather facts.")
    .tool(std::make_unique<WebSearchTool>())
    .build();

auto writer = Agent::builder()
    .llm(OpenAiProvider::from_env().model("gpt-4o-mini").build())
    .system("You write the final answer.")
    .build();

auto orchestrator = MultiAgentOrchestrator::builder()
    .add_agent("researcher", std::move(researcher))
    .add_agent("writer", std::move(writer))
    .routing(SequentialStrategy{{"researcher", "writer"}})
    .build();

auto output = orchestrator.run("Explain Tokio task spawning");
std::cout << output.text << "\n";
```

### Parallel Routing

```cpp
auto orchestrator = MultiAgentOrchestrator::builder()
    .add_agent("researcher_a", std::move(agent_a))
    .add_agent("researcher_b", std::move(agent_b))
    .routing(ParallelStrategy{{"researcher_a", "researcher_b"}})
    .build();
```

## YAML Runtime Loading

Create agents and multi-agent runtimes from `.yaml` files without recompiling code.

### Load a Single Agent from YAML

```cpp
auto agent = load_agent_from_yaml("examples/configs/single-agent.yaml");
auto output = agent->run("What is 21 * 2?");
std::cout << output.text << "\n";
```

Example YAML:

```yaml
kind: agent
name: calculator-assistant
llm:
  provider: open_ai
  model: gpt-4o-mini
system: You are a concise assistant that uses tools when helpful.
memory:
  type: sliding_window
  window_size: 8
tools:
  - type: calculator
loop_strategy:
  type: re_act
  max_steps: 4
temperature: 0.1
max_tokens: 512
```

### Load a Multi-Agent Orchestrator from YAML

```cpp
auto orchestrator = load_multi_agent_from_yaml("examples/configs/multi-agent.yaml");
auto output = orchestrator.run("Research and explain how Tokio tasks are spawned");
std::cout << output.text << "\n";
```

Example YAML:

```yaml
kind: multi_agent
agents:
  - name: researcher
    llm:
      provider: open_ai
      model: gpt-4o-mini
    system: You gather technical facts and use documentation tools.
    tools:
      - type: web_search
      - type: mcp
        target: https://mcp.context7.com/mcp
        api_key_header: X-Context7-API-Key
        api_key_prefix: ""
    loop_strategy:
      type: re_act
      max_steps: 5
  - name: writer
    llm:
      provider: open_ai
      model: gpt-4o-mini
    system: You write a polished final answer from the previous agent output.
    memory:
      type: token_aware
      max_tokens: 1200
    loop_strategy:
      type: chain_of_thought
routing:
  type: sequential
  order:
    - researcher
    - writer
```

### Load a Generic Runtime from YAML

```cpp
auto runtime = load_runtime_from_yaml("examples/configs/runtime-agent.yaml");
auto output = runtime.run("Summarize Rust async best practices");
std::cout << output.text << "\n";
```

## Error Handling

The SDK uses a structured exception hierarchy rooted at `AgentError`:

```text
AgentError (std::runtime_error)
├── ProviderError
│   ├── MissingApiKeyError
│   ├── HttpError
│   ├── ApiError
│   ├── ParseError
│   ├── RateLimitedError
│   ├── ContextWindowExceeded
│   ├── InvalidResponseError
│   └── UnsupportedError
├── ToolError
│   ├── InvalidInputError
│   ├── ToolExecutionError
│   ├── ToolTimeoutError
│   └── ToolPermissionDenied
├── MemoryError
│   ├── MemoryBackendError
│   └── MemorySerializationError
├── McpError
│   ├── McpInvalidCommand
│   ├── McpSpawnFailed
│   ├── McpProtocolError
│   ├── McpResponseError
│   └── McpTimeoutError
├── MaxStepsReached
├── ToolFailure
├── NoLlmProviderError
├── ToolNotFoundError
├── AgentNotFoundError
├── InvalidConfigurationError
└── ...
```

Typical usage:

```cpp
try {
    auto output = agent->run("hello");
    std::cout << output.text << "\n";
} catch (const AgentError& e) {
    std::cerr << "agent failed: " << e.what() << "\n";
}
```

## Examples

The repository includes 8 runnable examples:

| Example | Description |
|---|---|
| `simple_agent` | Minimal agent with a calculator tool |
| `tool_use` | Agent with multiple registered tools |
| `streaming` | Streaming agent output via callbacks |
| `multi_agent` | Sequential multi-agent orchestration |
| `mcp_integration` | Agent with MCP tools from a web endpoint |
| `yaml_single_agent` | Load an agent from YAML config |
| `yaml_multi_agent` | Load a multi-agent orchestrator from YAML |
| `yaml_runtime` | Generic YAML runtime loading |

Run them after building:

```bash
cd build/examples
./example_simple_agent
./example_tool_use
./example_streaming
./example_multi_agent
./example_mcp_integration
./example_yaml_single_agent
./example_yaml_multi_agent
./example_yaml_runtime
```

## Environment Variables

Common environment variables used by the provider builders:

- `OPENAI_API_KEY`
- `OPENAI_BASE_URL`
- `OPENAI_MODEL`
- `AZURE_OPENAI_KEY`
- `AZURE_OPENAI_ENDPOINT`
- `AZURE_OPENAI_MODEL`
- `ANTHROPIC_API_KEY`
- `GEMINI_API_KEY`
- `CONTEXT7_API_KEY`

For Ollama you only need the local service running at `http://localhost:11434`.

See `.env_example` for a template.

## Current Scope and Notes

- OpenAI, Azure OpenAI, and Ollama provide the most complete request/stream integration
- Anthropic and Gemini support `complete()` and can be used today; their streaming implementations are intentionally conservative in this version
- Multi-agent YAML currently supports sequential and parallel routing
- Redis memory backend requires hiredis and is disabled by default
- The Rust `#[tool]` proc-macro is replaced by manual `ITool` subclasses in C++

## Relationship to the Rust SDK

This project is a complete port of the Rust `agentrs` SDK to C++17. See [MIGRATION.md](MIGRATION.md) for detailed documentation of all conversion decisions, crate-to-library mappings, and known differences.

## License

Licensed under either:

- MIT license
- Apache License, Version 2.0

at your option.
