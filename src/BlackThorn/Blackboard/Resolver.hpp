/**
 * @file Resolver.hpp
 * @brief Variable resolver for blackboard references.
 *
 * Provides utilities to substitute \c ${key} placeholders in strings and to
 * resolve typed values from either blackboard references or literal text.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"
#include "BlackThorn/Blackboard/PortBinding.hpp"

#include <optional>
#include <regex>
#include <string>

namespace bt {

// ****************************************************************************
//! \brief Class representing a variable resolver.
//!
//! The VariableResolver handles resolution of \c ${variable} syntax in strings
//! and values, looking up the actual values from a Blackboard.
//!
//! Usage example:
//! \code
//!   Blackboard bb;
//!   bb.set("target", std::string("enemy_42"));
//!   bb.set("greeting", std::string("Hello"));
//!
//!   // String substitution (multiple placeholders supported)
//!   auto msg = VariableResolver::resolve("Attack ${target}!", bb);
//!   // msg == "Attack enemy_42!"
//!
//!   // Typed value: reference or literal
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
    //! value from the blackboard. Unknown keys are left unchanged. Multiple
    //! placeholders in the same string are all resolved.
    //!
    //! \param[in] p_str The string containing \c ${key} placeholders.
    //! \param[in] p_bb The blackboard used for lookups.
    //! \return The string with resolved placeholders.
    //!
    //! \code
    //!   Blackboard bb;
    //!   bb.set("name", std::string("Alice"));
    //!   bb.set("role", std::string("guard"));
    //!
    //!   auto msg = VariableResolver::resolve("${name} is the ${role}", bb);
    //!   // msg == "Alice is the guard"
    //!
    //!   auto msg = VariableResolver::resolve("No variables here", bb);
    //!   // msg == "No variables here"
    //!
    //!   auto msg = VariableResolver::resolve("Missing ${unknown}", bb);
    //!   // msg == "Missing ${unknown}"  (key not found, placeholder kept)
    //!
    //!   auto msg = VariableResolver::resolve("${greeting} ${name}!", bb);
    //!   // with greeting="Hi" -> "Hi Alice!"
    //! \endcode
    // ------------------------------------------------------------------------
    static std::string resolve(const std::string& p_str, const Blackboard& p_bb)
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
                const auto offset = static_cast<std::string::size_type>(
                    std::distance(result.cbegin(), search_start));
                const auto pos = offset + static_cast<std::string::size_type>(
                                              match.position(0));
                const auto len =
                    static_cast<std::string::size_type>(match.length(0));
                result.replace(pos, len, *value);
                const std::string::size_type new_index = pos + value->length();
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
    //! \brief Resolve a typed value from a reference or a literal.
    //!
    //! If \p p_expr matches \c ${key} exactly, the value is read from the
    //! blackboard with type \c T. Otherwise, \p p_expr is parsed as a literal
    //! (see parseLiteral()).
    //!
    //! \tparam T The expected type (\c int, \c double, \c bool, \c
    //! std::string).
    //! \param[in] p_expr A blackboard reference (\c ${key}) or a literal
    //! string.
    //! \param[in] p_bb The blackboard used for reference lookups.
    //! \return The resolved value, or \c std::nullopt on failure.
    //!
    //! \code
    //!   bb.set("health", 100);
    //!   bb.set("enabled", true);
    //!   bb.set("label", std::string("idle"));
    //!
    //!   resolveValue<int>("${health}", bb);   // -> 100
    //!   resolveValue<int>("42", bb);          // -> 42 (literal)
    //!   resolveValue<bool>("${enabled}", bb); // -> true
    //!   resolveValue<bool>("false", bb);      // -> false (literal)
    //!   resolveValue<std::string>("${label}", bb); // -> "idle"
    //!   resolveValue<std::string>("raw text", bb); // -> "raw text"
    //!   resolveValue<int>("${missing}", bb);  // -> std::nullopt
    //!   resolveValue<int>("not_a_number", bb); // -> std::nullopt
    //! \endcode
    // ------------------------------------------------------------------------
    template <typename T>
    static std::optional<T> resolveValue(const std::string& p_expr,
                                         const Blackboard& p_bb)
    {
        if (auto key = extractBlackboardReference(p_expr))
        {
            return p_bb.get<T>(*key);
        }

        // Otherwise, it is a literal value
        return parseLiteral<T>(p_expr);
    }

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

    static std::string resolveOutputKey(PortBinding const& p_binding)
    {
        if (p_binding.kind == PortBindingKind::BlackboardKey)
        {
            return p_binding.data;
        }
        return p_binding.data;
    }

private:

    // ------------------------------------------------------------------------
    //! \brief Parse a literal value from a raw string.
    //!
    //! Used internally when \p p_expr is not a \c ${key} reference.
    //! Supported types and accepted formats:
    //! - \c std::string : returned as-is
    //! - \c int : decimal integer (e.g. \c "42", \c "-1")
    //! - \c double : floating-point (e.g. \c "3.14")
    //! - \c bool : \c "true"/\c "false" or \c "1"/\c "0"
    //!
    //! \tparam T The type to parse into.
    //! \param[in] p_str The raw string to parse.
    //! \return The parsed value, or \c std::nullopt if parsing fails.
    //!
    //! \code
    //!   parseLiteral<std::string>("hello"); // -> "hello"
    //!   parseLiteral<int>("42");            // -> 42
    //!   parseLiteral<int>("abc");           // -> std::nullopt
    //!   parseLiteral<double>("3.14");       // -> 3.14
    //!   parseLiteral<bool>("true");         // -> true
    //!   parseLiteral<bool>("0");            // -> false
    //! \endcode
    // ------------------------------------------------------------------------
    template <typename T>
    static std::optional<T> parseLiteral(const std::string& p_str)
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
