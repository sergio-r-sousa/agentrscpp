#pragma once
// dotenv.hpp — Header-only .env file loader.
//
// Parses a `.env` file and sets environment variables that are NOT already
// defined, so real environment variables always take precedence.
//
// Usage:
//   agentrs::load_dotenv();               // searches CWD + parent dirs
//   agentrs::load_dotenv("path/.env");    // explicit path
//
// File format:
//   - Lines starting with '#' are comments.
//   - Empty lines are ignored.
//   - KEY=VALUE sets the variable.
//   - Values may be optionally wrapped in double or single quotes.
//   - Inline comments after values are NOT supported (the whole line
//     after '=' is the value, modulo surrounding quotes).

#include <cstdlib>
#include <fstream>
#include <string>

#ifdef _WIN32
#include <cstdio>   // _dupenv_s (optional)
#endif

namespace agentrs {

namespace detail {

/// Trim leading and trailing whitespace from a string.
inline std::string trim(const std::string& s) {
    auto start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

/// Remove surrounding quotes (single or double) if present.
inline std::string unquote(const std::string& s) {
    if (s.size() >= 2) {
        char first = s.front();
        char last  = s.back();
        if ((first == '"' && last == '"') || (first == '\'' && last == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

/// Set an environment variable only if it is not already defined.
/// Returns true on success.
inline bool set_env_if_absent(const std::string& key, const std::string& value) {
    // Check if already set.
    const char* existing = std::getenv(key.c_str());
    if (existing != nullptr) {
        return false; // do not overwrite
    }

#ifdef _WIN32
    // Windows: _putenv_s is the safe setter.
    return _putenv_s(key.c_str(), value.c_str()) == 0;
#else
    // POSIX: setenv with overwrite=0.
    return setenv(key.c_str(), value.c_str(), 0) == 0;
#endif
}

/// Parse a single .env file and set environment variables.
/// Returns the number of variables set.
inline int parse_dotenv_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return -1;

    int count = 0;
    std::string line;

    while (std::getline(file, line)) {
        // Remove trailing \r (Windows line endings).
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        std::string trimmed = trim(line);

        // Skip empty lines and comments.
        if (trimmed.empty() || trimmed[0] == '#') {
            continue;
        }

        // Find the first '=' separator.
        auto eq_pos = trimmed.find('=');
        if (eq_pos == std::string::npos) {
            continue; // malformed line, skip
        }

        std::string key   = trim(trimmed.substr(0, eq_pos));
        std::string value = trim(trimmed.substr(eq_pos + 1));

        // Remove optional quotes around the value.
        value = unquote(value);

        if (!key.empty()) {
            if (set_env_if_absent(key, value)) {
                ++count;
            }
        }
    }

    return count;
}

} // namespace detail

/// Loads environment variables from a `.env` file at the given path.
/// Variables already set in the environment are NOT overwritten.
///
/// @param path  Path to the .env file.
/// @return Number of new variables set, or -1 if the file could not be opened.
inline int load_dotenv(const std::string& path) {
    return detail::parse_dotenv_file(path);
}

/// Loads environment variables from a `.env` file, searching the current
/// working directory and up to 4 parent directories (project root heuristic).
/// Variables already set in the environment are NOT overwritten.
///
/// Search order: `./.env`, `../.env`, `../../.env`, `../../../.env`, `../../../../.env`
///
/// @return Number of new variables set, or -1 if no .env file was found.
inline int load_dotenv() {
    // Try increasingly higher parent directories.
    std::string prefix;
    for (int i = 0; i < 5; ++i) {
        std::string candidate = prefix + ".env";
        int result = detail::parse_dotenv_file(candidate);
        if (result >= 0) {
            return result;
        }
        prefix += "../";
    }
    return -1; // no .env file found — not an error, just nothing to load
}

} // namespace agentrs
