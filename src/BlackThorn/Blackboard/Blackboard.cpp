/**
 * @file Blackboard.cpp
 * @brief Blackboard class for shared data storage in behavior trees.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <sstream>

namespace bt {

// ------------------------------------------------------------------------
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
        oss << "  " << key << " = " << anyToString(value);

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
            oss << "    " << key << " = " << anyToString(value) << std::endl;
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

// ------------------------------------------------------------------------
std::string Blackboard::anyToString(Value const& p_value)
{
    if (auto* v = std::any_cast<std::string>(&p_value))
    {
        return "\"" + *v + "\" (string)";
    }
    if (auto* v = std::any_cast<int>(&p_value))
    {
        return std::to_string(*v) + " (int)";
    }
    if (auto* v = std::any_cast<double>(&p_value))
    {
        return std::to_string(*v) + " (double)";
    }
    if (auto* v = std::any_cast<float>(&p_value))
    {
        return std::to_string(*v) + " (float)";
    }
    if (auto* v = std::any_cast<bool>(&p_value))
    {
        return (*v ? "true" : "false") + std::string(" (bool)");
    }
    if (auto* v = std::any_cast<size_t>(&p_value))
    {
        return std::to_string(*v) + " (size_t)";
    }

    return std::string("(") + p_value.type().name() + ")";
}

} // namespace bt
