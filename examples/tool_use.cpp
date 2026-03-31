// tool_use.cpp — Custom tool registered with an agent.
// C++ equivalent of examples/tool_use.rs
//
// Rust uses a proc-macro (#[tool]) to derive a tool struct from a function.
// In C++ we define the tool class manually by implementing ITool.

#include <algorithm>
#include <iostream>
#include <string>

#include <agentrs/agentrs.hpp>

using namespace agentrs;

/// Custom tool that reverses a string.
class ReverseTextTool : public ITool {
public:
    [[nodiscard]] std::string name() const override { return "reverse_text"; }

    [[nodiscard]] std::string description() const override {
        return "Reverse a string";
    }

    [[nodiscard]] nlohmann::json schema() const override {
        return {
            {"type", "object"},
            {"properties", {
                {"text", {{"type", "string"}, {"description", "The text to reverse"}}}
            }},
            {"required", nlohmann::json::array({"text"})}
        };
    }

    ToolOutput call(const nlohmann::json& input) override {
        auto text = input.at("text").get<std::string>();
        std::reverse(text.begin(), text.end());
        return ToolOutput::text(text);
    }
};

int main() {
    load_dotenv();

    auto llm = OpenAiProvider::from_env().build();

    auto agent = Agent::builder()
        .llm(llm)
        .tool(std::make_unique<ReverseTextTool>())
        .build();

    auto output = agent->run("Use reverse_text to invert 'rust'");
    std::cout << output.text << "\n";
    return 0;
}
