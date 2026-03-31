// mcp_integration.cpp — Agent with MCP (Model Context Protocol) tools.
// C++ equivalent of examples/mcp_integration.rs
//
// Demonstrates mixing local tools (CalculatorTool) with remote MCP tools
// (Context7 documentation endpoint) in the same agent.

#include <cstdlib>
#include <iostream>
#include <string>
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

    // Build MCP options for the Context7 endpoint.
    agentrs::WebMcpOptions mcp_opts;
    const char* api_key_env = std::getenv("CONTEXT7_API_KEY");
    if (api_key_env) {
        mcp_opts.api_key(api_key_env)
                .api_key_header("X-Context7-API-Key")
                .api_key_prefix("");
    }

    ToolRegistry tools;
    tools.register_tool(std::make_unique<CalculatorTool>());
    tools.register_mcp_http_with_options("https://mcp.context7.com/mcp", mcp_opts);

    auto agent = Agent::builder()
        .llm(llm)
        .tools(std::move(tools))
        .build();

    auto output = agent->run(
        "Use Context7 MCP to find Tokio documentation for spawning tasks.");
    std::cout << output.text << "\n";
    return 0;
}
