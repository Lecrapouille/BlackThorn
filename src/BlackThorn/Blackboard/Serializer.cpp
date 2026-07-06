/**
 * @file Serializer.cpp
 * @brief Blackboard serialization to/from YAML.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Serializer.hpp"

#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Yaml/Document.hpp"

#include <unordered_map>
#include <vector>
#include <yaml-cpp/yaml.h>

namespace bt {

// ------------------------------------------------------------------------
void BlackboardSerializer::load(Blackboard& p_target,
                                YAML::Node const& p_node,
                                Blackboard const* p_reference)
{
    if (!p_node || !p_node.IsMap())
    {
        return;
    }

    Blackboard const* scope = p_reference != nullptr ? p_reference : &p_target;

    for (auto const& entry : p_node)
    {
        auto key = entry.first.as<std::string>();
        p_target.m_data[key] = toValue(entry.second, scope);
    }
}

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

BlackboardValue
BlackboardSerializer::valueFromNode(YamlNode const& p_node,
                                    Blackboard const* p_scope)
{
    return toValue(p_node, p_scope);
}

// ------------------------------------------------------------------------
YAML::Node BlackboardSerializer::dump(Blackboard const& p_source)
{
    YAML::Node node(YAML::NodeType::Map);
    for (auto const& [key, value] : p_source.m_data)
    {
        node[key] = toYaml(value);
    }
    return node;
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

BlackboardValue BlackboardSerializer::toValue(YAML::Node const& p_node,
                                              Blackboard const* p_scope)
{
    if (!p_node)
    {
        return {};
    }

    // Convert a scalar value into a std::any value
    if (p_node.IsScalar())
    {
        std::string literal = p_node.Scalar();
        std::string key;
        if (p_scope != nullptr && isReference(literal, key))
        {
            if (auto value = p_scope->raw(key))
            {
                return *value;
            }
        }

        try
        {
            return p_node.as<int>();
        }
        catch (...)
        {
            /* nothing to do */
        }

        try
        {
            return p_node.as<double>();
        }
        catch (...)
        {
            /* nothing to do */
        }

        try
        {
            return p_node.as<bool>();
        }
        catch (...)
        {
            /* nothing to do */
        }

        return literal;
    }

    // Convert a sequence value into a std::any value
    if (p_node.IsSequence())
    {
        std::vector<BlackboardValue> generic;
        generic.reserve(p_node.size());

        bool numeric_only = true;
        std::vector<double> numeric_values;
        numeric_values.reserve(p_node.size());

        for (auto const& element : p_node)
        {
            auto entry = toValue(element, p_scope);

            if (std::holds_alternative<int>(static_cast<BlackboardValue::Base const&>(entry)))
            {
                numeric_values.push_back(
                    static_cast<double>(std::get<int>(static_cast<BlackboardValue::Base const&>(entry))));
            }
            else if (std::holds_alternative<double>(
                         static_cast<BlackboardValue::Base const&>(entry)))
            {
                numeric_values.push_back(std::get<double>(
                    static_cast<BlackboardValue::Base const&>(entry)));
            }
            else
            {
                numeric_only = false;
            }

            generic.push_back(std::move(entry));
        }

        if (numeric_only)
        {
            return numeric_values;
        }

        return generic;
    }

    // Convert a map value into a std::any value
    if (p_node.IsMap())
    {
        BlackboardMap map;
        for (auto const& element : p_node)
        {
            map.emplace(element.first.as<std::string>(),
                        toValue(element.second, p_scope));
        }
        return map;
    }

    return {};
}

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
            auto entry = toValue(p_element, p_scope);

            if (std::holds_alternative<int>(
                    static_cast<BlackboardValue::Base const&>(entry)))
            {
                numeric_values.push_back(static_cast<double>(std::get<int>(
                    static_cast<BlackboardValue::Base const&>(entry))));
            }
            else if (std::holds_alternative<double>(
                         static_cast<BlackboardValue::Base const&>(entry)))
            {
                numeric_values.push_back(std::get<double>(
                    static_cast<BlackboardValue::Base const&>(entry)));
            }
            else
            {
                numeric_only = false;
            }

            generic.push_back(std::move(entry));
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

YAML::Node BlackboardSerializer::toYaml(BlackboardValue const& p_value)
{
    if (std::holds_alternative<std::monostate>(
        static_cast<BlackboardValue::Base const&>(p_value)))
    {
        return YAML::Node();
    }
    if (auto const* v = std::get_if<int>(&p_value.asBase()))
    {
        return YAML::Node(*v);
    }
    if (auto const* v = std::get_if<double>(&p_value.asBase()))
    {
        return YAML::Node(*v);
    }
    if (auto const* v = std::get_if<bool>(&p_value.asBase()))
    {
        return YAML::Node(*v);
    }
    if (auto const* v = std::get_if<std::string>(&p_value.asBase()))
    {
        return YAML::Node(*v);
    }
    if (auto const* v = std::get_if<std::vector<double>>(&p_value.asBase()))
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for (double entry : *v)
        {
            node.push_back(entry);
        }
        return node;
    }
    if (auto const* v = std::get_if<std::vector<BlackboardValue>>(&p_value.asBase()))
    {
        YAML::Node node(YAML::NodeType::Sequence);
        for (auto const& entry : *v)
        {
            node.push_back(toYaml(entry));
        }
        return node;
    }
    if (auto const* v = std::get_if<BlackboardMap>(&p_value.asBase()))
    {
        YAML::Node node(YAML::NodeType::Map);
        for (auto const& [key, entry] : *v)
        {
            node[key] = toYaml(entry);
        }
        return node;
    }
    if (auto const* v = std::get_if<std::any>(&p_value.asBase()))
    {
        if (v->type() == typeid(int))
        {
            return YAML::Node(std::any_cast<int>(*v));
        }
        if (v->type() == typeid(double))
        {
            return YAML::Node(std::any_cast<double>(*v));
        }
        if (v->type() == typeid(bool))
        {
            return YAML::Node(std::any_cast<bool>(*v));
        }
        if (v->type() == typeid(std::string))
        {
            return YAML::Node(std::any_cast<std::string>(*v));
        }
    }

    return YAML::Node();
}

} // namespace bt
