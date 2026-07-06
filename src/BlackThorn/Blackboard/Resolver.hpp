/**
 * @file Resolver.hpp
 * @brief Blackboard references, port bindings, and variable resolution.
 *
 * This header groups everything related to \c ${key} expressions:
 * - compile-time port binding (\ref PortBinding, \ref resolvePortRemapping)
 * - run-time string substitution (\ref VariableResolver::resolve)
 * - typed reads from bindings (\ref VariableResolver::resolveBinding)
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <cstdint>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>

namespace bt {

// ****************************************************************************
//! \brief How a port expression was resolved at tree build time.
// ****************************************************************************
enum class PortBindingKind : std::uint8_t
{
    //! \brief Read/write a blackboard key directly (no \c ${} wrapper).
    BlackboardKey,
    //! \brief Literal scalar stored as text and parsed on read.
    Literal,
};

// ****************************************************************************
//! \brief Resolved port expression used at tick time without regex parsing.
//!
//! Built once from YAML \c parameters during tree construction.
//!
//! \code
//!   PortBinding ref{PortBindingKind::BlackboardKey, "health"};
//!   PortBinding lit{PortBindingKind::Literal, "42"};
//! \endcode
// ****************************************************************************
struct PortBinding
{
    PortBindingKind kind = PortBindingKind::Literal;
    std::string data;
};

using ResolvedPortMap = std::unordered_map<std::string, PortBinding>;

// ---------------------------------------------------------------------------
//! \brief Fast check for a single \c ${key} reference (no regex).
//!
//! \code
//!   isBlackboardReference("${hp}");       // true
//!   isBlackboardReference("${a} ${b}");   // false
//!   isBlackboardReference("literal");     // false
//! \endcode
// ---------------------------------------------------------------------------
inline bool isBlackboardReference(std::string_view p_expr)
{
    if (p_expr.size() < 4 || p_expr.front() != '$' || p_expr[1] != '{' ||
        p_expr.back() != '}')
    {
        return false;
    }
    return p_expr.find('}', 2) == p_expr.size() - 1;
}

// ---------------------------------------------------------------------------
//! \brief Extract the key from \c ${key}. Returns nullopt if not a reference.
// ---------------------------------------------------------------------------
inline std::optional<std::string>
extractBlackboardReference(std::string_view p_expr)
{
    if (!isBlackboardReference(p_expr))
    {
        return std::nullopt;
    }
    return std::string(p_expr.substr(2, p_expr.size() - 3));
}

// ---------------------------------------------------------------------------
//! \brief Resolve a YAML port expression once during tree construction.
// ---------------------------------------------------------------------------
inline PortBinding resolvePortExpression(std::string const& p_expr)
{
    if (auto key = extractBlackboardReference(p_expr))
    {
        return PortBinding{PortBindingKind::BlackboardKey, std::move(*key)};
    }
    return PortBinding{PortBindingKind::Literal, p_expr};
}

// ---------------------------------------------------------------------------
//! \brief Resolve a map of port expressions from YAML parameters.
//!
//! \code
//!   auto raw = std::unordered_map<std::string, std::string>{
//!       {"speed", "${max_speed}"},
//!       {"retries", "3"},
//!   };
//!   auto ports = resolvePortRemapping(raw);
//! \endcode
// ---------------------------------------------------------------------------
inline ResolvedPortMap resolvePortRemapping(
    std::unordered_map<std::string, std::string> const& p_raw)
{
    ResolvedPortMap resolved;
    resolved.reserve(p_raw.size());
    for (auto const& [port, expr] : p_raw)
    {
        resolved.emplace(port, resolvePortExpression(expr));
    }
    return resolved;
}

// ****************************************************************************
//! \brief Resolves \c ${variable} syntax in strings and typed port bindings.
//!
//! \code
//!   Blackboard bb;
//!   bb.set("target", std::string("enemy_42"));
//!
//!   auto msg = VariableResolver::resolve("Attack ${target}!", bb);
//!   // msg == "Attack enemy_42!"
//!
//!   auto speed = VariableResolver::resolveValue<int>("${max_speed}", bb);
//!   auto retries = VariableResolver::resolveValue<int>("3", bb); // literal
//! \endcode
// ****************************************************************************
class VariableResolver
{
public:

    // ------------------------------------------------------------------------
    //! \brief Resolve \c ${key} placeholders in a string.
    //!
    //! Each \c ${key} segment is replaced by the corresponding \c std::string
    //! value from the blackboard. Unknown keys are left unchanged.
    // ------------------------------------------------------------------------
    static std::string resolve(std::string const& p_str, Blackboard const& p_bb)
    {
        if (auto key = extractBlackboardReference(p_str))
        {
            return p_bb.getOrDefault<std::string>(*key, p_str);
        }

        static std::regex const pattern(R"(\$\{([^}]+)\})");
        std::string result = p_str;
        std::smatch match;

        auto search_start = result.cbegin();
        while (std::regex_search(search_start, result.cend(), match, pattern))
        {
            std::string key = match[1].str();

            if (auto value = p_bb.get<std::string>(key))
            {
                auto const offset = static_cast<std::string::size_type>(
                    std::distance(result.cbegin(), search_start));
                auto const pos = offset + static_cast<std::string::size_type>(
                                                match.position(0));
                auto const len =
                    static_cast<std::string::size_type>(match.length(0));
                result.replace(pos, len, *value);
                std::string::size_type const new_index = pos + value->length();
                search_start =
                    result.cbegin() + static_cast<std::ptrdiff_t>(new_index);
            }
            else
            {
                search_start = match.suffix().first;
            }
        }

        return result;
    }

    // ------------------------------------------------------------------------
    //! \brief Resolve a typed value from a reference or a literal string.
    // ------------------------------------------------------------------------
    template <typename T>
    static std::optional<T> resolveValue(std::string const& p_expr,
                                         Blackboard const& p_bb)
    {
        if (auto key = extractBlackboardReference(p_expr))
        {
            return p_bb.get<T>(*key);
        }
        return parseLiteral<T>(p_expr);
    }

    // ------------------------------------------------------------------------
    //! \brief Read a typed input port through a pre-resolved binding.
    // ------------------------------------------------------------------------
    template <typename T>
    static std::optional<T> resolveBinding(PortBinding const& p_binding,
                                           Blackboard const& p_bb)
    {
        switch (p_binding.kind)
        {
            case PortBindingKind::BlackboardKey:
                return p_bb.get<T>(p_binding.data);
            case PortBindingKind::Literal:
                return parseLiteral<T>(p_binding.data);
        }
        return std::nullopt;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the blackboard key used when writing an output port.
    // ------------------------------------------------------------------------
    static std::string resolveOutputKey(PortBinding const& p_binding)
    {
        return p_binding.data;
    }

private:

    template <typename T>
    static std::optional<T> parseLiteral(std::string const& p_str)
    {
        if constexpr (std::is_same_v<T, std::string>)
        {
            return p_str;
        }
        else if constexpr (std::is_same_v<T, int>)
        {
            try
            {
                return std::stoi(p_str);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
        else if constexpr (std::is_same_v<T, double>)
        {
            try
            {
                return std::stod(p_str);
            }
            catch (...)
            {
                return std::nullopt;
            }
        }
        else if constexpr (std::is_same_v<T, bool>)
        {
            if (p_str == "true" || p_str == "1")
            {
                return true;
            }
            if (p_str == "false" || p_str == "0")
            {
                return false;
            }
            return std::nullopt;
        }
        return std::nullopt;
    }
};

} // namespace bt
