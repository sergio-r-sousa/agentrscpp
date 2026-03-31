#pragma once
// builtin.hpp — Built-in tool implementations.
// Equivalent to agentrs-tools/src/builtin.rs

#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "agentrs/core/error.hpp"
#include "agentrs/core/interfaces.hpp"
#include "agentrs/core/types.hpp"

namespace agentrs {

// --- Expression parser (internal) -------------------------------------------

namespace detail {

/// Error type for expression parsing failures.
struct ExpressionError {
    enum class Kind {
        UnexpectedEnd,
        UnexpectedToken,
        DivisionByZero,
        InvalidNumber
    };

    Kind kind;
    std::size_t position = 0;

    [[nodiscard]] std::string message() const;
};

/// Simple recursive-descent parser for arithmetic expressions.
/// Supports: +, -, *, /, unary +/-, parentheses, integer and floating-point
/// numbers.
class ExpressionParser {
public:
    explicit ExpressionParser(std::string_view input);

    /// Parses the full expression. Throws ToolExecutionError on failure.
    double parse();

private:
    double parse_expression();
    double parse_term();
    double parse_factor();
    double parse_number();
    void skip_whitespace();
    [[nodiscard]] bool is_eof() const;
    [[nodiscard]] char peek() const;
    char consume();

    std::string_view input_;
    std::size_t pos_ = 0;
};

} // namespace detail

// --- CalculatorTool ---------------------------------------------------------

/// Math evaluation tool. Evaluates simple arithmetic expressions.
class CalculatorTool : public ITool {
public:
    CalculatorTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

// --- WebFetchTool -----------------------------------------------------------

/// Fetches HTTP content from a URL (stub).
class WebFetchTool : public ITool {
public:
    WebFetchTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

// --- WebSearchTool ----------------------------------------------------------

/// Searches the web using DuckDuckGo instant answers (stub).
class WebSearchTool : public ITool {
public:
    WebSearchTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

// --- FileReadTool -----------------------------------------------------------

/// Reads a file from allowed paths (stub).
class FileReadTool : public ITool {
public:
    FileReadTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

// --- FileWriteTool ----------------------------------------------------------

/// Writes a file to allowed paths (stub).
class FileWriteTool : public ITool {
public:
    FileWriteTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

// --- BashTool ---------------------------------------------------------------

/// Executes shell commands (stub).
class BashTool : public ITool {
public:
    BashTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

// --- PythonTool -------------------------------------------------------------

/// Executes inline Python code (stub).
class PythonTool : public ITool {
public:
    PythonTool() = default;

    [[nodiscard]] std::string name() const override;
    [[nodiscard]] std::string description() const override;
    [[nodiscard]] nlohmann::json schema() const override;
    ToolOutput call(const nlohmann::json& input) override;
};

} // namespace agentrs
