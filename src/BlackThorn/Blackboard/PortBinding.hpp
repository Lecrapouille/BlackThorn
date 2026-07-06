/**
 * @file PortBinding.hpp
 * @brief Compile-time port remapping resolved from YAML/XML expressions.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace bt {

// ****************************************************************************
//! \brief How a port expression was resolved at build time.
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
// ****************************************************************************
struct PortBinding
{
    PortBindingKind kind = PortBindingKind::Literal;
    std::string data;
};

using ResolvedPortMap = std::unordered_map<std::string, PortBinding>;

// ---------------------------------------------------------------------------
//! \brief Fast check for a single \c ${key} reference (no regex).
// ---------------------------------------------------------------------------
inline bool isBlackboardReference(std::string_view p_expr)
{
    if (p_expr.size() < 4 || p_expr.front() != '$' || p_expr[1] != '{' ||
        p_expr.back() != '}')
    {
        return false;
    }
    // Exactly one ${key} reference (no trailing text or second placeholder).
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

} // namespace bt
