/**
 * @file Resolver.cpp
 * @brief Blackboard references, port bindings, and variable resolution.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Resolver.hpp"

namespace bt {

// ------------------------------------------------------------------------
bool isBlackboardReference(std::string_view p_expr)
{
    if (p_expr.size() < 4 || p_expr.front() != '$' || p_expr[1] != '{' ||
        p_expr.back() != '}')
    {
        return false;
    }
    return p_expr.find('}', 2) == p_expr.size() - 1;
}

// ------------------------------------------------------------------------
std::optional<std::string> extractBlackboardReference(std::string_view p_expr)
{
    if (!isBlackboardReference(p_expr))
    {
        return std::nullopt;
    }
    return std::string(p_expr.substr(2, p_expr.size() - 3));
}

// ------------------------------------------------------------------------
PortBinding resolvePortExpression(std::string const& p_expr)
{
    if (auto key = extractBlackboardReference(p_expr))
    {
        return PortBinding{PortBindingKind::BlackboardKey, std::move(*key)};
    }
    return PortBinding{PortBindingKind::Literal, p_expr};
}

// ------------------------------------------------------------------------
ResolvedPortMap
resolvePortRemapping(std::unordered_map<std::string, std::string> const& p_raw)
{
    ResolvedPortMap resolved;
    resolved.reserve(p_raw.size());
    for (auto const& [port, expr] : p_raw)
    {
        resolved.emplace(port, resolvePortExpression(expr));
    }
    return resolved;
}

// ------------------------------------------------------------------------
std::string VariableResolver::resolve(std::string const& p_str,
                                      Blackboard const& p_bb)
{
    if (auto key = extractBlackboardReference(p_str))
    {
        return p_bb.getOrDefault<std::string>(*key, p_str);
    }

    std::string result;
    result.reserve(p_str.size());

    size_t pos = 0;
    while (pos < p_str.size())
    {
        auto const start = p_str.find("${", pos);
        if (start == std::string::npos)
        {
            result.append(p_str, pos, std::string::npos);
            break;
        }

        result.append(p_str, pos, start - pos);

        auto const end = p_str.find('}', start + 2);
        if (end == std::string::npos)
        {
            result.append(p_str, start, std::string::npos);
            break;
        }

        std::string const key(p_str, start + 2, end - start - 2);
        if (auto value = p_bb.get<std::string>(key))
        {
            result.append(*value);
        }
        else
        {
            result.append(p_str, start, end - start + 1);
        }

        pos = end + 1;
    }

    return result;
}

} // namespace bt
