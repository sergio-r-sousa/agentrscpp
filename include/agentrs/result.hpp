#pragma once
// result.hpp — Result<T,E> for C++17 (GCC)
// Rust-equivalent Result type with combinators.
// Compatible with: g++ -std=c++17 -Wall -Wextra

#include <variant>
#include <functional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace agentrs {

// --- Err sentinel type -------------------------------------------------------

template<typename E>
struct ErrWrapper { E value; };

template<typename E>
ErrWrapper<E> Err(E e) { return {std::move(e)}; }

// --- Result<T, E> ------------------------------------------------------------

template<typename T, typename E = std::string>
class [[nodiscard]] Result {
    static_assert(!std::is_reference_v<T>, "T cannot be a reference");
    static_assert(!std::is_reference_v<E>, "E cannot be a reference");

    std::variant<T, E> data_;

public:
    using value_type = T;
    using error_type = E;

    // -- Constructors ---------------------------------------------------------

    // Ok
    Result(T val) : data_(std::in_place_index<0>, std::move(val)) {}

    // Err via ErrWrapper
    Result(ErrWrapper<E> e) : data_(std::in_place_index<1>, std::move(e.value)) {}

    // -- Queries --------------------------------------------------------------

    [[nodiscard]] bool is_ok()  const noexcept { return data_.index() == 0; }
    [[nodiscard]] bool is_err() const noexcept { return data_.index() == 1; }
    explicit operator bool() const noexcept { return is_ok(); }

    // -- Access ---------------------------------------------------------------

    T& value() & {
        if (is_err()) throw std::runtime_error("Result::value() called on Err");
        return std::get<0>(data_);
    }
    const T& value() const& {
        if (is_err()) throw std::runtime_error("Result::value() called on Err");
        return std::get<0>(data_);
    }
    T value() && {
        if (is_err()) throw std::runtime_error("Result::value() called on Err");
        return std::get<0>(std::move(data_));
    }

    E& error() & {
        if (is_ok()) throw std::runtime_error("Result::error() called on Ok");
        return std::get<1>(data_);
    }
    const E& error() const& {
        if (is_ok()) throw std::runtime_error("Result::error() called on Ok");
        return std::get<1>(data_);
    }

    // -- Combinators (Rust-style) ---------------------------------------------

    // .unwrap_or(default)
    T value_or(T def) const& { return is_ok() ? std::get<0>(data_) : std::move(def); }
    T value_or(T def) &&     { return is_ok() ? std::get<0>(std::move(data_)) : std::move(def); }

    // .map(f) — transforms T, keeps E
    template<typename F>
    auto map(F&& f) const& -> Result<std::invoke_result_t<F, const T&>, E> {
        if (is_ok()) return {std::invoke(std::forward<F>(f), std::get<0>(data_))};
        return agentrs::Err<E>(std::get<1>(data_));
    }
    template<typename F>
    auto map(F&& f) && -> Result<std::invoke_result_t<F, T&&>, E> {
        if (is_ok()) return {std::invoke(std::forward<F>(f), std::get<0>(std::move(data_)))};
        return agentrs::Err<E>(std::get<1>(std::move(data_)));
    }

    // .map_err(f) — transforms E, keeps T
    template<typename F>
    auto map_err(F&& f) const& -> Result<T, std::invoke_result_t<F, const E&>> {
        if (is_err()) return agentrs::Err(std::invoke(std::forward<F>(f), std::get<1>(data_)));
        return {std::get<0>(data_)};
    }

    // .and_then(f) — chains Results (flatMap / bind)
    template<typename F>
    auto and_then(F&& f) const& -> std::invoke_result_t<F, const T&> {
        if (is_ok()) return std::invoke(std::forward<F>(f), std::get<0>(data_));
        using R = std::invoke_result_t<F, const T&>;
        return agentrs::Err<typename R::error_type>(std::get<1>(data_));
    }
    template<typename F>
    auto and_then(F&& f) && -> std::invoke_result_t<F, T&&> {
        if (is_ok()) return std::invoke(std::forward<F>(f), std::get<0>(std::move(data_)));
        using R = std::invoke_result_t<F, T&&>;
        return agentrs::Err<typename R::error_type>(std::get<1>(std::move(data_)));
    }

    // .or_else(f) — fallback on Err
    template<typename F>
    Result or_else(F&& f) const& {
        if (is_err()) return std::invoke(std::forward<F>(f), std::get<1>(data_));
        return *this;
    }
};

// --- Ok helper ---------------------------------------------------------------

template<typename T, typename E = std::string>
Result<T, E> Ok(T val) { return Result<T, E>(std::move(val)); }

// --- Unit Result helper (for Result<void, E> equivalent) ---------------------
// In Rust: Result<(), E> — we use Result<std::monostate, E>
using Unit = std::monostate;

template<typename E = std::string>
Result<Unit, E> OkVoid() { return Result<Unit, E>(Unit{}); }

} // namespace agentrs

// --- TRY macro ---------------------------------------------------------------
// Equivalent to Rust's `?` operator.
// Uses GCC statement expressions ({ ... })

#define AGENTRS_TRY(EXPR)                                   \
    ({                                                      \
        auto _res_ = (EXPR);                                \
        if (!_res_.is_ok()) return ::agentrs::Err(std::move(_res_.error())); \
        std::move(_res_.value());                           \
    })
