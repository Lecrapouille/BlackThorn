/**
 * @file Serializer.cpp
 * @brief Blackboard serialization to/from YAML.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Serializer.hpp"

#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Builder/Yaml.hpp"

#include <sstream>
#include <unordered_map>
#include <vector>

namespace bt {

// ------------------------------------------------------------------------
static void appendIndent(std::ostringstream& p_out, int p_indent)
{
    p_out << std::string(static_cast<std::size_t>(p_indent) * 2U, ' ');
}

// ------------------------------------------------------------------------
static void appendQuotedString(std::ostringstream& p_out,
                               std::string const& p_value)
{
    p_out << '"';
    for (char ch : p_value)
    {
        if (ch == '"' || ch == '\\')
        {
            p_out << '\\';
        }
        p_out << ch;
    }
    p_out << '"';
}

// ------------------------------------------------------------------------
static bool needsQuotes(std::string const& p_value)
{
    if (p_value.empty())
    {
        return true;
    }

    static char const* const reserved[] = {
        "true", "false", "null", "yes", "no", "~"};

    for (char const* word : reserved)
    {
        if (p_value == word)
        {
            return true;
        }
    }

    for (char ch : p_value)
    {
        if (ch == ':' || ch == '#' || ch == '[' || ch == ']' || ch == '{'
            || ch == '}' || ch == ',' || ch == '\n' || ch == '\r' || ch == '\t'
            || ch == '"' || ch == '\'' || ch == '&' || ch == '*' || ch == '!'
            || ch == '|' || ch == '>' || ch == '%' || ch == '@' || ch == '`')
        {
            return true;
        }
    }

    if (p_value.front() == ' ' || p_value.back() == ' ')
    {
        return true;
    }

    return false;
}

// ------------------------------------------------------------------------
static void appendScalar(std::ostringstream& p_out, std::string const& p_value)
{
    if (needsQuotes(p_value))
    {
        appendQuotedString(p_out, p_value);
    }
    else
    {
        p_out << p_value;
    }
}

// ------------------------------------------------------------------------
static void accumulateSequenceEntry(BlackboardValue p_entry,
                                    std::vector<BlackboardValue>& p_generic,
                                    std::vector<double>& p_numeric,
                                    bool& p_numeric_only)
{
    auto const& base = p_entry.asBase();
    if (auto const* as_int = std::get_if<int>(&base))
    {
        p_numeric.push_back(static_cast<double>(*as_int));
    }
    else if (auto const* as_double = std::get_if<double>(&base))
    {
        p_numeric.push_back(*as_double);
    }
    else
    {
        p_numeric_only = false;
    }

    p_generic.push_back(std::move(p_entry));
}

// ------------------------------------------------------------------------
void BlackboardSerializer::load(Blackboard& p_target,
                                YamlNode const& p_node,
                                Blackboard const* p_reference)
{
    if (!p_node.isMap())
    {
        return;
    }

    Blackboard const* scope = p_reference != nullptr ? p_reference : &p_target;

    p_node.forEachMap([&](std::string_view p_key, YamlNode p_value) {
        p_target.m_data[std::string(p_key)] = toValue(p_value, scope);
    });
}

// ------------------------------------------------------------------------
BlackboardValue BlackboardSerializer::valueFromNode(YamlNode const& p_node,
                                                    Blackboard const* p_scope)
{
    return toValue(p_node, p_scope);
}

// ------------------------------------------------------------------------
std::string BlackboardSerializer::dump(Blackboard const& p_source)
{
    if (p_source.m_data.empty())
    {
        return {};
    }

    std::ostringstream out;
    bool first = true;
    for (auto const& [key, value] : p_source.m_data)
    {
        if (!first)
        {
            out << '\n';
        }
        first = false;

        appendIndent(out, 0);
        out << key << ':';
        appendYamlValue(out, value, 0);
    }

    return out.str();
}

// ------------------------------------------------------------------------
bool BlackboardSerializer::isReference(std::string const& p_literal,
                                       std::string& p_key)
{
    if (auto ref = extractBlackboardReference(p_literal))
    {
        p_key = std::move(*ref);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------------
BlackboardValue BlackboardSerializer::toValue(YamlNode const& p_node,
                                              Blackboard const* p_scope)
{
    if (!p_node.valid())
    {
        return {};
    }

    if (p_node.isScalar())
    {
        std::string literal = p_node.scalar();
        std::string key;
        if (p_scope != nullptr && isReference(literal, key))
        {
            if (auto value = p_scope->raw(key))
            {
                return *value;
            }
        }

        if (auto v = p_node.asInt())
        {
            return *v;
        }

        if (auto v = p_node.asDouble())
        {
            return *v;
        }

        if (auto v = p_node.asBool())
        {
            return *v;
        }

        return literal;
    }

    if (p_node.isSeq())
    {
        std::vector<BlackboardValue> generic;
        generic.reserve(p_node.size());

        bool numeric_only = true;
        std::vector<double> numeric_values;
        numeric_values.reserve(p_node.size());

        p_node.forEachSeq([&](YamlNode p_element) {
            accumulateSequenceEntry(toValue(p_element, p_scope),
                                    generic,
                                    numeric_values,
                                    numeric_only);
        });

        if (numeric_only && !numeric_values.empty())
        {
            return numeric_values;
        }

        return generic;
    }

    if (p_node.isMap())
    {
        BlackboardMap map;
        p_node.forEachMap([&](std::string_view p_key, YamlNode p_value) {
            map.emplace(std::string(p_key), toValue(p_value, p_scope));
        });
        return map;
    }

    return {};
}

// ------------------------------------------------------------------------
void BlackboardSerializer::appendYamlValue(std::ostringstream& p_out,
                                           BlackboardValue const& p_value,
                                           int p_indent)
{
    if (std::holds_alternative<std::monostate>(
            static_cast<BlackboardValue::Base const&>(p_value)))
    {
        p_out << " null";
        return;
    }

    if (auto const* v = std::get_if<int>(&p_value.asBase()))
    {
        p_out << ' ' << *v;
        return;
    }
    if (auto const* v = std::get_if<double>(&p_value.asBase()))
    {
        p_out << ' ' << *v;
        return;
    }
    if (auto const* v = std::get_if<bool>(&p_value.asBase()))
    {
        p_out << ' ' << (*v ? "true" : "false");
        return;
    }
    if (auto const* v = std::get_if<std::string>(&p_value.asBase()))
    {
        p_out << ' ';
        appendScalar(p_out, *v);
        return;
    }
    if (auto const* v = std::get_if<std::vector<double>>(&p_value.asBase()))
    {
        p_out << '\n';
        for (double entry : *v)
        {
            appendIndent(p_out, p_indent + 1);
            p_out << "- " << entry << '\n';
        }
        return;
    }
    if (auto const* v =
            std::get_if<std::vector<BlackboardValue>>(&p_value.asBase()))
    {
        p_out << '\n';
        for (auto const& entry : *v)
        {
            appendIndent(p_out, p_indent + 1);
            p_out << "-";
            appendYamlValue(p_out, entry, p_indent + 1);
            p_out << '\n';
        }
        return;
    }
    if (auto const* v = std::get_if<BlackboardMap>(&p_value.asBase()))
    {
        p_out << '\n';
        bool first = true;
        for (auto const& [key, entry] : *v)
        {
            if (!first)
            {
                p_out << '\n';
            }
            first = false;
            appendIndent(p_out, p_indent + 1);
            p_out << key << ':';
            appendYamlValue(p_out, entry, p_indent + 1);
        }
        return;
    }
    if (auto const* v = std::get_if<std::any>(&p_value.asBase()))
    {
        if (v->type() == typeid(int))
        {
            p_out << ' ' << std::any_cast<int>(*v);
            return;
        }
        if (v->type() == typeid(double))
        {
            p_out << ' ' << std::any_cast<double>(*v);
            return;
        }
        if (v->type() == typeid(bool))
        {
            p_out << ' ' << (std::any_cast<bool>(*v) ? "true" : "false");
            return;
        }
        if (v->type() == typeid(std::string))
        {
            p_out << ' ';
            appendScalar(p_out, std::any_cast<std::string>(*v));
            return;
        }
    }

    p_out << " null";
}

} // namespace bt
