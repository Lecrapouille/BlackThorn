/**
 * @file BlackboardValue.hpp
 * @brief Typed storage variant for blackboard entries.
 *
 * Built-in YAML types map directly to variant alternatives.
 * Custom C++ types are boxed inside \c std::any.
 *
 * \code
 *   BlackboardValue health = 100;
 *   BlackboardValue label = std::string("idle");
 *
 *   auto as_int = detail::valueAs<int>(health); // optional{100}
 *   bb.set("custom", MyStruct{...});             // stored as std::any
 * \endcode
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
//! \brief Value stored in a single blackboard entry.
//!
//! Key features:
//! - Native support for YAML scalar and collection types.
//! - \c std::any fallback for custom C++ types.
//! - Conversion helpers in \ref detail::valueAs and \ref detail::toStoredValue.
// ****************************************************************************
struct BlackboardValue: std::variant<std::monostate,
                                     int,
                                     double,
                                     float,
                                     bool,
                                     std::string,
                                     size_t,
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
                              size_t,
                              std::vector<double>,
                              std::vector<BlackboardValue>,
                              BlackboardMap,
                              std::any>;

    using Base::Base;
    using Base::operator=;

    // ------------------------------------------------------------------------
    //! \brief Const access to the underlying \c std::variant storage.
    //! \return Reference to the base variant.
    // ------------------------------------------------------------------------
    [[nodiscard]] Base const& asBase() const
    {
        return static_cast<Base const&>(*this);
    }

    // ------------------------------------------------------------------------
    //! \brief Mutable access to the underlying \c std::variant storage.
    //! \return Reference to the base variant.
    // ------------------------------------------------------------------------
    [[nodiscard]] Base& asBase()
    {
        return static_cast<Base&>(*this);
    }
};

namespace detail {

// ------------------------------------------------------------------------
//! \brief Trait detecting whether \c T is already a \ref BlackboardValue.
// ------------------------------------------------------------------------
template <typename T, typename = void>
struct is_blackboard_value : std::false_type
{
};

// ------------------------------------------------------------------------
//! \brief Specialization: \ref BlackboardValue is stored as-is.
// ------------------------------------------------------------------------
template <>
struct is_blackboard_value<BlackboardValue> : std::true_type
{
};

//! \brief Convenience variable template for \ref is_blackboard_value.
template <typename T>
inline constexpr bool is_blackboard_value_v = is_blackboard_value<T>::value;

// ------------------------------------------------------------------------
//! \brief Convert a stored variant to a requested type.
//! \param[in] p_value Source blackboard value.
//! \return Converted value, or \c std::nullopt when incompatible.
//!
//! Handles direct variant alternatives and \c std::any unboxing.
// ------------------------------------------------------------------------
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
                       std::is_same_v<Decayed, size_t> ||
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

// ------------------------------------------------------------------------
//! \brief Wrap an arbitrary C++ value for blackboard storage.
//! \param[in] p_value Value to store.
//! \return Variant alternative or \c std::any box for custom types.
// ------------------------------------------------------------------------
template <typename T>
BlackboardValue toStoredValue(T&& p_value)
{
    using Decayed = std::decay_t<T>;
    if constexpr (is_blackboard_value_v<Decayed>)
    {
        return std::forward<T>(p_value);
    }
    else if constexpr (std::is_same_v<Decayed, int> ||
                       std::is_same_v<Decayed, double> ||
                       std::is_same_v<Decayed, float> ||
                       std::is_same_v<Decayed, bool> ||
                       std::is_same_v<Decayed, std::string> ||
                       std::is_same_v<Decayed, size_t> ||
                       std::is_same_v<Decayed, std::any>)
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
