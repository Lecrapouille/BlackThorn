/**
 * @file Blackboard.cpp
 * @brief Shared key-value storage for behavior tree nodes.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <iomanip>
#include <sstream>

namespace bt {

// ------------------------------------------------------------------------
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

// ------------------------------------------------------------------------
Blackboard::Blackboard(Blackboard::Ptr p_parent) : m_parent(std::move(p_parent))
{
}

// ------------------------------------------------------------------------
void Blackboard::setRaw(Key const& p_key, Value const& p_value)
{
    m_data[p_key] = p_value;
    invalidateKey(p_key);
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
void Blackboard::remove(Key const& p_key)
{
    m_data.erase(p_key);
    invalidateKey(p_key);
}

// ------------------------------------------------------------------------
std::shared_ptr<Blackboard> Blackboard::createChild()
{
    return std::make_shared<Blackboard>(shared_from_this());
}

// ------------------------------------------------------------------------
void Blackboard::setPortRemapping(
    std::unordered_map<std::string, std::string> const& p_remapping)
{
    m_portRemapping = p_remapping;
}

// ------------------------------------------------------------------------
void Blackboard::beginTick()
{
    ++m_tick_generation;
    if (m_read_cache.size() > 256)
    {
        m_read_cache.clear();
    }
}

// ------------------------------------------------------------------------
void Blackboard::invalidateKey(Key const& p_key)
{
    m_read_cache.erase(p_key);
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
        size_t count = 0;
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

// ------------------------------------------------------------------------
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
