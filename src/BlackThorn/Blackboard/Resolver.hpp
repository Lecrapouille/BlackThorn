/**
 * @file Resolver.hpp
 * @brief Blackboard references, port bindings, and variable resolution.
 *
 * This header groups everything related to \c ${key} expressions:
 * - compile-time port binding (\ref PortBinding, \ref resolvePortRemapping)
 * - run-time string substitution (\ref VariableResolver::resolve, load-time only)
 * - typed reads from bindings (\ref VariableResolver::resolveBinding, tick-time)
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <cstdint>
#include <optional>
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

// ------------------------------------------------------------------------
//! \brief Fast check for a single \c ${key} reference (no regex).
//! \param[in] p_expr Port expression text.
//! \return \c true when \p p_expr is exactly \c "${key}" with no extra text.
// ------------------------------------------------------------------------
[[nodiscard]] bool isBlackboardReference(std::string_view p_expr);

// ------------------------------------------------------------------------
//! \brief Extract the key from \c ${key}.
//! \param[in] p_expr Port expression text.
//! \return Inner key name, or \c std::nullopt when not a reference.
// ------------------------------------------------------------------------
[[nodiscard]] std::optional<std::string>
extractBlackboardReference(std::string_view p_expr);

// ------------------------------------------------------------------------
//! \brief Resolve a YAML port expression once during tree construction.
//! \param[in] p_expr Raw parameter value from YAML.
//! \return Binding classified as blackboard key or literal.
// ------------------------------------------------------------------------
[[nodiscard]] PortBinding resolvePortExpression(std::string const& p_expr);

// ------------------------------------------------------------------------
//! \brief Resolve a map of port expressions from YAML parameters.
//! \param[in] p_raw Port name → expression table from node definition.
//! \return Pre-resolved bindings for tick-time access.
// ------------------------------------------------------------------------
[[nodiscard]] ResolvedPortMap
resolvePortRemapping(std::unordered_map<std::string, std::string> const& p_raw);

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
    //! \param[in] p_str Source string, possibly containing placeholders.
    //! \param[in] p_bb Blackboard used for key lookup.
    //! \return String with known placeholders expanded.
    // ------------------------------------------------------------------------
    [[nodiscard]] static std::string resolve(std::string const& p_str,
                                             Blackboard const& p_bb);

    // ------------------------------------------------------------------------
    //! \brief Resolve a typed value from a reference or a literal string.
    //! \param[in] p_expr Port expression (\c "${key}" or literal text).
    //! \param[in] p_bb Blackboard used when \p p_expr is a reference.
    //! \return Parsed or looked-up value.
    // ------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] static std::optional<T>
    resolveValue(std::string const& p_expr, Blackboard const& p_bb)
    {
        if (auto key = extractBlackboardReference(p_expr))
        {
            return p_bb.get<T>(*key);
        }
        return parseLiteral<T>(p_expr);
    }

    // ------------------------------------------------------------------------
    //! \brief Read a typed input port through a pre-resolved binding.
    //! \param[in] p_binding Binding built at tree construction time.
    //! \param[in] p_bb Blackboard used for key lookups.
    //! \return Parsed or looked-up value.
    // ------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] static std::optional<T>
    resolveBinding(PortBinding const& p_binding, Blackboard const& p_bb)
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
    //! \param[in] p_binding Output port binding.
    //! \return Target blackboard key name.
    // ------------------------------------------------------------------------
    [[nodiscard]] static std::string
    resolveOutputKey(PortBinding const& p_binding)
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
