/**
 * @file BlackboardValue.hpp
 * @brief Typed storage for blackboard entries.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <any>
#include <cstddef>
#include <optional>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <variant>
#include <vector>

namespace bt {

using BlackboardMap = std::unordered_map<std::string, struct BlackboardValue>;

// ****************************************************************************
//! \brief Value stored in a blackboard entry.
// ****************************************************************************
struct BlackboardValue : std::variant<std::monostate,
                                      int,
                                      double,
                                      float,
                                      bool,
                                      std::string,
                                      std::size_t,
                                      std::vector<double>,
                                      std::vector<BlackboardValue>,
                                      BlackboardMap,
                                      std::any>
{
    using Base = std::variant<std::monostate,
                              int,
                              double,
                              float,
                              bool,
                              std::string,
                              std::size_t,
                              std::vector<double>,
                              std::vector<BlackboardValue>,
                              BlackboardMap,
                              std::any>;

    using Base::Base;
    using Base::operator=;

    [[nodiscard]] Base const& asBase() const
    {
        return static_cast<Base const&>(*this);
    }

    [[nodiscard]] Base& asBase()
    {
        return static_cast<Base&>(*this);
    }
};

namespace detail {

template <typename T, typename = void>
struct is_blackboard_value : std::false_type
{
};

template <>
struct is_blackboard_value<BlackboardValue> : std::true_type
{
};

template <typename T>
inline constexpr bool is_blackboard_value_v = is_blackboard_value<T>::value;

template <typename T>
std::optional<T> valueAs(BlackboardValue const& p_value)
{
    using Decayed = std::decay_t<T>;

    if constexpr (std::is_same_v<Decayed, BlackboardValue>)
    {
        return p_value;
    }
    else if constexpr (std::is_same_v<Decayed, std::any>)
    {
        if (auto const* boxed = std::get_if<std::any>(&p_value.asBase()))
        {
            return *boxed;
        }
        return std::nullopt;
    }
    else if constexpr (std::is_same_v<Decayed, int> ||
                       std::is_same_v<Decayed, double> ||
                       std::is_same_v<Decayed, float> ||
                       std::is_same_v<Decayed, bool> ||
                       std::is_same_v<Decayed, std::string> ||
                       std::is_same_v<Decayed, std::size_t> ||
                       std::is_same_v<Decayed, std::vector<double>> ||
                       std::is_same_v<Decayed, std::vector<BlackboardValue>> ||
                       std::is_same_v<Decayed, BlackboardMap>)
    {
        if (auto const* direct = std::get_if<Decayed>(&p_value.asBase()))
        {
            return *direct;
        }
    }

    if (auto const* boxed = std::get_if<std::any>(&p_value.asBase()))
    {
        if (boxed->type() == typeid(T))
        {
            return std::any_cast<T>(*boxed);
        }
    }
    return std::nullopt;
}

template <typename T>
BlackboardValue toStoredValue(T&& p_value)
{
    using Decayed = std::decay_t<T>;
    if constexpr (is_blackboard_value_v<Decayed>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, int>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, double>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, float>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, bool>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, std::string>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, std::size_t>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, std::any>)
    {
        return std::forward<T>(p_value);
    }
    else
    {
        return BlackboardValue(std::in_place_type<std::any>,
                               std::any(std::forward<T>(p_value)));
    }
}

} // namespace detail

} // namespace bt
