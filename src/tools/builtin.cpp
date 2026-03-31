// builtin.cpp — Built-in tool implementations.
// Equivalent to agentrs-tools/src/builtin.rs

#include "agentrs/tools/builtin.hpp"

#include <cmath>
#include <sstream>
#include <stdexcept>

namespace agentrs {

// ===========================================================================
// detail::ExpressionError
// ===========================================================================

namespace detail {

std::string ExpressionError::message() const {
    switch (kind) {
        case Kind::UnexpectedEnd:
            return "unexpected end of expression";
        case Kind::UnexpectedToken:
            return "unexpected token at position " + std::to_string(position);
        case Kind::DivisionByZero:
            return "division by zero";
        case Kind::InvalidNumber:
            return "invalid number starting at position " + std::to_string(position);
    }
    return "unknown expression error";
}

// ===========================================================================
// detail::ExpressionParser
// ===========================================================================

ExpressionParser::ExpressionParser(std::string_view input)
    : input_(input), pos_(0) {}

double ExpressionParser::parse() {
    double value = parse_expression();
    skip_whitespace();
    if (!is_eof()) {
        throw ToolExecutionError(
            ExpressionError{ExpressionError::Kind::UnexpectedToken, pos_}.message()
        );
    }
    return value;
}

double ExpressionParser::parse_expression() {
    double value = parse_term();
    for (;;) {
        skip_whitespace();
        if (is_eof()) break;
        char c = peek();
        if (c == '+') {
            consume();
            value += parse_term();
        } else if (c == '-') {
            consume();
            value -= parse_term();
        } else {
            break;
        }
    }
    return value;
}

double ExpressionParser::parse_term() {
    double value = parse_factor();
    for (;;) {
        skip_whitespace();
        if (is_eof()) break;
        char c = peek();
        if (c == '*') {
            consume();
            value *= parse_factor();
        } else if (c == '/') {
            consume();
            double divisor = parse_factor();
            if (divisor == 0.0) {
                throw ToolExecutionError(
                    ExpressionError{ExpressionError::Kind::DivisionByZero, 0}.message()
                );
            }
            value /= divisor;
        } else {
            break;
        }
    }
    return value;
}

double ExpressionParser::parse_factor() {
    skip_whitespace();
    if (is_eof()) {
        throw ToolExecutionError(
            ExpressionError{ExpressionError::Kind::UnexpectedEnd, 0}.message()
        );
    }

    char c = peek();

    if (c == '(') {
        consume();
        double value = parse_expression();
        skip_whitespace();
        if (is_eof()) {
            throw ToolExecutionError(
                ExpressionError{ExpressionError::Kind::UnexpectedEnd, 0}.message()
            );
        }
        char closing = consume();
        if (closing != ')') {
            throw ToolExecutionError(
                ExpressionError{ExpressionError::Kind::UnexpectedToken, pos_ - 1}.message()
            );
        }
        return value;
    }

    if (c == '+') {
        consume();
        return parse_factor();
    }

    if (c == '-') {
        consume();
        return -parse_factor();
    }

    if (std::isdigit(static_cast<unsigned char>(c)) || c == '.') {
        return parse_number();
    }

    throw ToolExecutionError(
        ExpressionError{ExpressionError::Kind::UnexpectedToken, pos_}.message()
    );
}

double ExpressionParser::parse_number() {
    std::size_t start = pos_;
    bool seen_dot = false;

    while (!is_eof()) {
        char c = peek();
        if (std::isdigit(static_cast<unsigned char>(c))) {
            consume();
        } else if (c == '.' && !seen_dot) {
            seen_dot = true;
            consume();
        } else {
            break;
        }
    }

    std::string num_str(input_.substr(start, pos_ - start));
    try {
        std::size_t idx = 0;
        double result = std::stod(num_str, &idx);
        if (idx != num_str.size()) {
            throw ToolExecutionError(
                ExpressionError{ExpressionError::Kind::InvalidNumber, start}.message()
            );
        }
        return result;
    } catch (const std::invalid_argument&) {
        throw ToolExecutionError(
            ExpressionError{ExpressionError::Kind::InvalidNumber, start}.message()
        );
    } catch (const std::out_of_range&) {
        throw ToolExecutionError(
            ExpressionError{ExpressionError::Kind::InvalidNumber, start}.message()
        );
    }
}

void ExpressionParser::skip_whitespace() {
    while (!is_eof() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
        ++pos_;
    }
}

bool ExpressionParser::is_eof() const {
    return pos_ >= input_.size();
}

char ExpressionParser::peek() const {
    return input_[pos_];
}

char ExpressionParser::consume() {
    return input_[pos_++];
}

} // namespace detail

// ===========================================================================
// CalculatorTool
// ===========================================================================

std::string CalculatorTool::name() const { return "calculator"; }

std::string CalculatorTool::description() const {
    return "Evaluate mathematical expressions safely.";
}

nlohmann::json CalculatorTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"expression", {
                {"type", "string"},
                {"description", "Mathematical expression to evaluate"}
            }}
        }},
        {"required", nlohmann::json::array({"expression"})}
    };
}

ToolOutput CalculatorTool::call(const nlohmann::json& input) {
    auto it = input.find("expression");
    if (it == input.end() || !it->is_string()) {
        throw InvalidInputError("missing 'expression'");
    }

    std::string expression = it->get<std::string>();
    detail::ExpressionParser parser(expression);
    double result = parser.parse();

    // Format: avoid trailing zeros for integers, use full precision otherwise.
    std::ostringstream oss;
    if (result == std::floor(result) && std::abs(result) < 1e15) {
        oss << static_cast<long long>(result);
    } else {
        oss << result;
    }
    return ToolOutput::text(oss.str());
}

// ===========================================================================
// WebFetchTool
// ===========================================================================

std::string WebFetchTool::name() const { return "web_fetch"; }

std::string WebFetchTool::description() const {
    return "Fetch a URL and return the text body.";
}

nlohmann::json WebFetchTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"url", {{"type", "string"}}},
            {"max_chars", {{"type", "integer"}, {"default", 4000}}}
        }},
        {"required", nlohmann::json::array({"url"})}
    };
}

ToolOutput WebFetchTool::call(const nlohmann::json& /*input*/) {
    throw ToolExecutionError("not yet implemented");
}

// ===========================================================================
// WebSearchTool
// ===========================================================================

std::string WebSearchTool::name() const { return "web_search"; }

std::string WebSearchTool::description() const {
    return "Search the web for current public information.";
}

nlohmann::json WebSearchTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"query", {{"type", "string"}}},
            {"max_results", {{"type", "integer"}, {"default", 5}}}
        }},
        {"required", nlohmann::json::array({"query"})}
    };
}

ToolOutput WebSearchTool::call(const nlohmann::json& /*input*/) {
    throw ToolExecutionError("not yet implemented");
}

// ===========================================================================
// FileReadTool
// ===========================================================================

std::string FileReadTool::name() const { return "file_read"; }

std::string FileReadTool::description() const {
    return "Read UTF-8 text files from allowed paths.";
}

nlohmann::json FileReadTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}}}
        }},
        {"required", nlohmann::json::array({"path"})}
    };
}

ToolOutput FileReadTool::call(const nlohmann::json& /*input*/) {
    throw ToolExecutionError("not yet implemented");
}

// ===========================================================================
// FileWriteTool
// ===========================================================================

std::string FileWriteTool::name() const { return "file_write"; }

std::string FileWriteTool::description() const {
    return "Write UTF-8 text files to allowed paths.";
}

nlohmann::json FileWriteTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"path", {{"type", "string"}}},
            {"content", {{"type", "string"}}}
        }},
        {"required", nlohmann::json::array({"path", "content"})}
    };
}

ToolOutput FileWriteTool::call(const nlohmann::json& /*input*/) {
    throw ToolExecutionError("not yet implemented");
}

// ===========================================================================
// BashTool
// ===========================================================================

std::string BashTool::name() const { return "bash"; }

std::string BashTool::description() const {
    return "Execute shell commands and capture stdout/stderr.";
}

nlohmann::json BashTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"command", {{"type", "string"}}},
            {"cwd", {{"type", "string"}}},
            {"timeout_secs", {{"type", "integer"}, {"default", 30}}}
        }},
        {"required", nlohmann::json::array({"command"})}
    };
}

ToolOutput BashTool::call(const nlohmann::json& /*input*/) {
    throw ToolExecutionError("not yet implemented");
}

// ===========================================================================
// PythonTool
// ===========================================================================

std::string PythonTool::name() const { return "python"; }

std::string PythonTool::description() const {
    return "Execute inline Python code.";
}

nlohmann::json PythonTool::schema() const {
    return {
        {"type", "object"},
        {"properties", {
            {"script", {{"type", "string"}}}
        }},
        {"required", nlohmann::json::array({"script"})}
    };
}

ToolOutput PythonTool::call(const nlohmann::json& /*input*/) {
    throw ToolExecutionError("not yet implemented");
}

} // namespace agentrs
