/**
 * @file Blackboard.hpp
 * @brief Blackboard class for shared data storage in behavior trees.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include <any>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bt {

// Forward declaration for friend class
class BlackboardSerializer;

// ****************************************************************************
//! \brief Class representing a blackboard.
//!
//! A blackboard is a shared data storage mechanism used in behavior trees to
//! enable communication and data sharing between different nodes. It acts as
//! a key-value store where nodes can read and write data of any type.
//!
//! Key features:
//! - Type-safe storage using std::any for values of any type.
//! - Hierarchical structure: child blackboards can access parent data.
//! - Automatic parent lookup when a key is not found locally.
//! - Support for creating child blackboards with createChild().
//!
//! Usage example:
//! \code
//!   auto bb = std::make_shared<Blackboard>();
//!   bb->set("health", 100);
//!   bb->set("target", std::string("enemy"));
//!   auto health = bb->get<int>("health");
//! \endcode
// ****************************************************************************
class Blackboard final: public std::enable_shared_from_this<Blackboard>
{
    friend class BlackboardSerializer;

public:

    using Key = std::string;
    using Value = std::any;
    using Ptr = std::shared_ptr<Blackboard>;

    // ------------------------------------------------------------------------
    //! \brief Constructor.
    //! \param[in] p_parent The parent blackboard.
    // ------------------------------------------------------------------------
    explicit Blackboard(Blackboard::Ptr p_parent = nullptr) : m_parent(p_parent)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Set a value with generic type.
    //! \param[in] key The key to set the value.
    //! \param[in] value The value to set.
    // ------------------------------------------------------------------------
    template <typename T>
    void set(const Key& p_key, T&& p_value)
    {
        m_data[p_key] = std::forward<T>(p_value);
    }

    // ------------------------------------------------------------------------
    //! \brief Get the raw stored value without casting.
    //! \details Searches locally first, then in the parent blackboard if not
    //!          found. Returns the stored std::any as-is without type
    //!          conversion.
    //! \param[in] p_key The key to get the value.
    //! \return The stored std::any if present, std::nullopt otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::optional<Value> raw(const Key& p_key) const
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
    //! \brief Get a value with automatic type conversion.
    //! \details Searches locally first. If the key is found but the type does
    //!          not match, continues searching in the parent blackboard.
    //!          If the key is not found locally, searches in the parent.
    //! \param[in] p_key The key to get the value.
    //! \return The converted value if found and type matches, std::nullopt
    //!         otherwise.
    // ------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] std::optional<T> get(const Key& p_key) const
    {
        // Search locally first
        if (auto it = m_data.find(p_key); it != m_data.end())
        {
            try
            {
                return std::any_cast<T>(it->second);
            }
            catch (const std::bad_any_cast&)
            {
                // Type mismatch, fall through to parent
            }
        }

        // Search in the parent if not found locally or type mismatch
        if (m_parent)
        {
            return m_parent->get<T>(p_key);
        }

        return std::nullopt;
    }

    // ------------------------------------------------------------------------
    //! \brief Get a value with automatic type conversion or a default value.
    //! \details Uses get<T>() internally, so follows the same search strategy:
    //!          searches locally first, then in parent if not found or type
    //!          mismatch. Returns the default value if the key is not found
    //!          or cannot be converted to type T.
    //! \param[in] p_key The key to get the value.
    //! \param[in] p_default The default value to return if the key is not
    //!                      found or type conversion fails.
    //! \return The converted value if found, otherwise the default value.
    // ------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] T getOrDefault(const Key& p_key, T p_default = T()) const
    {
        if (auto value = get<T>(p_key))
        {
            return *value;
        }
        return p_default;
    }

    // ------------------------------------------------------------------------
    //! \brief Check if a key exists.
    //! \param[in] p_key The key to check.
    //! \return True if the key exists, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool has(const Key& p_key) const
    {
        if (m_data.find(p_key) != m_data.end())
            return true;
        if (m_parent)
            return m_parent->has(p_key);
        return false;
    }

    // ------------------------------------------------------------------------
    //! \brief Remove a key.
    //! \param[in] p_key The key to remove.
    // ------------------------------------------------------------------------
    void remove(const Key& p_key)
    {
        m_data.erase(p_key);
    }

    // ------------------------------------------------------------------------
    //! \brief Create a child blackboard.
    //! \return A shared pointer to the child blackboard.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::shared_ptr<Blackboard> createChild()
    {
        return std::make_shared<Blackboard>(this->shared_from_this());
    }

    // ------------------------------------------------------------------------
    //! \brief Dump the contents of the blackboard and its parent to the
    //! console.
    // ------------------------------------------------------------------------
    void dump() const
    {
        std::cout << "=== Blackboard Content ===" << std::endl;
        for (const auto& [key, value] : m_data)
        {
            std::cout << "  " << key << " : " << value.type().name()
                      << std::endl;
        }
        if (m_parent)
        {
            std::cout << "  (+ parent data)" << std::endl;
            m_parent->dump();
        }
    }

    // ------------------------------------------------------------------------
    //! \brief Get all keys stored in this blackboard (not including parent).
    //! \return Vector of all keys.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::vector<Key> keys() const
    {
        std::vector<Key> result;
        result.reserve(m_data.size());
        for (const auto& [key, _] : m_data)
        {
            result.push_back(key);
        }
        return result;
    }

    // QQ: AFFICHER lES PARENTS

private:

    std::unordered_map<Key, Value> m_data;
    std::shared_ptr<Blackboard> m_parent;
};

} // namespace bt
