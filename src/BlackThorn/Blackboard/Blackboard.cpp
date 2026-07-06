/**
 * @file Blackboard.cpp
 * @brief Blackboard class for shared data storage in behavior trees.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <iomanip>
#include <sstream>

namespace bt {

Blackboard::Blackboard(Blackboard::Ptr p_parent) : m_parent(std::move(p_parent))
{
}

// ------------------------------------------------------------------------
std::optional<Blackboard::Value> Blackboard::raw(const Key& p_key) const
{
    if (auto it = m_data.find(p_key); it != m_data.end())
    {
        return it->second;
    }

    if (m_parent)
    {
        return m_parent->raw(p_key);
    }

    return std::nullopt;
}

// ------------------------------------------------------------------------
bool Blackboard::has(const Key& p_key) const
{
    if (m_data.find(p_key) != m_data.end())
    {
        return true;
    }
    if (m_parent)
    {
        return m_parent->has(p_key);
    }
    return false;
}

// ------------------------------------------------------------------------
std::string Blackboard::dump(std::string const& p_title,
                             bool p_showParent) const
{
    std::ostringstream oss;
    oss << "=== " << p_title << " ===" << std::endl;

    for (const auto& [key, value] : m_data)
    {
        oss << "  " << key << " = " << valueToString(value);

        if (auto it = m_portRemapping.find(key); it != m_portRemapping.end())
        {
            oss << "  [remapped to port of parent tree: " << it->second << "]";
        }
        oss << std::endl;
    }

    for (const auto& [localKey, parentKey] : m_portRemapping)
    {
        if (m_data.find(localKey) == m_data.end())
        {
            oss << "  [" << localKey << "] remapped to port of parent tree ["
                << parentKey << "]" << std::endl;
        }
    }

    if (p_showParent && m_parent)
    {
        oss << "  --- Parent Blackboard ---" << std::endl;
        for (const auto& [key, value] : m_parent->m_data)
        {
            oss << "    " << key << " = " << valueToString(value) << std::endl;
        }
    }

    return oss.str();
}

// ------------------------------------------------------------------------
//! \brief Get all keys stored in this blackboard (not including parent).
// ------------------------------------------------------------------------
std::vector<Blackboard::Key> Blackboard::keys() const
{
    std::vector<Key> result;
    result.reserve(m_data.size());
    for (const auto& [key, _] : m_data)
    {
        result.push_back(key);
    }
    return result;
}

std::string Blackboard::valueToString(Value const& p_value)
{
    if (auto const* v = std::get_if<std::string>(&p_value.asBase()))
    {
        return "\"" + *v + "\" (string)";
    }
    if (auto const* v = std::get_if<int>(&p_value.asBase()))
    {
        return std::to_string(*v) + " (int)";
    }
    if (auto const* v = std::get_if<double>(&p_value.asBase()))
    {
        return std::to_string(*v) + " (double)";
    }
    if (auto const* v = std::get_if<float>(&p_value.asBase()))
    {
        return std::to_string(*v) + " (float)";
    }
    if (auto const* v = std::get_if<bool>(&p_value.asBase()))
    {
        return (*v ? "true" : "false") + std::string(" (bool)");
    }
    if (auto const* v = std::get_if<std::size_t>(&p_value.asBase()))
    {
        return std::to_string(*v) + " (size_t)";
    }
    if (auto const* v = std::get_if<std::any>(&p_value.asBase()))
    {
        return std::string("(") + v->type().name() + ")";
    }

    return "(complex)";
}

std::string Blackboard::displayValue(Value const& p_value)
{
    if (auto const* v = std::get_if<int>(&p_value.asBase()))
    {
        return std::to_string(*v);
    }
    if (auto const* v = std::get_if<double>(&p_value.asBase()))
    {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << *v;
        return oss.str();
    }
    if (auto const* v = std::get_if<bool>(&p_value.asBase()))
    {
        return *v ? "true" : "false";
    }
    if (auto const* v = std::get_if<std::string>(&p_value.asBase()))
    {
        return *v;
    }
    if (auto const* map = std::get_if<BlackboardMap>(&p_value.asBase()))
    {
        std::string result = "{";
        std::size_t count = 0;
        for (auto const& [field_key, _] : *map)
        {
            if (count > 0)
            {
                result += ", ";
            }
            result += field_key;
            ++count;
            if (count >= 3 && map->size() > 3)
            {
                result += ", ...";
                break;
            }
        }
        result += "}";
        return result;
    }
    if (auto const* vec =
            std::get_if<std::vector<BlackboardValue>>(&p_value.asBase()))
    {
        return "[" + std::to_string(vec->size()) + " items]";
    }
    if (auto const* vec = std::get_if<std::vector<double>>(&p_value.asBase()))
    {
        return "[" + std::to_string(vec->size()) + " doubles]";
    }
    return "...";
}

std::string Blackboard::displayKey(Blackboard const* p_blackboard,
                                   std::string const& p_key)
{
    if (!p_blackboard)
    {
        return "";
    }

    auto raw_value = p_blackboard->raw(p_key);
    if (!raw_value.has_value())
    {
        return "";
    }

    return displayValue(*raw_value);
}

} // namespace bt
