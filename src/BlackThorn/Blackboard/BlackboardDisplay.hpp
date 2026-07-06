/**
 * @file BlackboardDisplay.hpp
 * @brief Helpers to display blackboard values in tools and visualizers.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/Blackboard.hpp"

#include <iomanip>
#include <sstream>
#include <string>

namespace bt {

inline std::string displayBlackboardValue(BlackboardValue const& p_value)
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
    if (auto const* vec = std::get_if<std::vector<BlackboardValue>>(&p_value.asBase()))
    {
        return "[" + std::to_string(vec->size()) + " items]";
    }
    if (auto const* vec = std::get_if<std::vector<double>>(&p_value.asBase()))
    {
        return "[" + std::to_string(vec->size()) + " doubles]";
    }
    return "...";
}

inline std::string getBlackboardDisplayValue(Blackboard const* p_blackboard,
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

    return displayBlackboardValue(*raw_value);
}

} // namespace bt
