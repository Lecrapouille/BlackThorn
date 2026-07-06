/**
 * @file Blackboard.hpp
 * @brief Shared key-value storage for behavior tree nodes.
 *
 * A blackboard lets nodes exchange typed data without direct coupling.
 * Child blackboards inherit keys from their parent when a local key is absent.
 *
 * \code
 *   auto bb = std::make_shared<Blackboard>();
 *   bb->set("health", 100);
 *   bb->set("target", std::string("enemy"));
 *
 *   auto health = bb->get<int>("health");   // optional<int>{100}
 *   auto missing = bb->get<int>("mana");    // nullopt
 * \endcode
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Blackboard/BlackboardValue.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace bt {

class BlackboardSerializer;

// ****************************************************************************
//! \brief Hierarchical typed key-value store shared by tree nodes.
// ****************************************************************************
class Blackboard final: public std::enable_shared_from_this<Blackboard>
{
public:

    using Key = std::string;
    using Value = BlackboardValue;
    using Ptr = std::shared_ptr<Blackboard>;

    explicit Blackboard(Blackboard::Ptr p_parent = nullptr);

    template <typename T>
    void set(Key const& p_key, T&& p_value)
    {
        m_data[p_key] = detail::toStoredValue(std::forward<T>(p_value));
        invalidateKey(p_key);
    }

    void setRaw(Key const& p_key, Value const& p_value)
    {
        m_data[p_key] = p_value;
        invalidateKey(p_key);
    }

    [[nodiscard]] std::optional<Value> raw(Key const& p_key) const;

    template <typename T>
    [[nodiscard]] std::optional<T> get(Key const& p_key) const
    {
        if (auto cached = readFromCache<T>(p_key))
        {
            return cached;
        }

        if (auto it = m_data.find(p_key); it != m_data.end())
        {
            if (auto value = detail::valueAs<T>(it->second))
            {
                storeInCache(p_key, *value);
                return value;
            }
        }

        if (m_parent)
        {
            return m_parent->get<T>(p_key);
        }

        return std::nullopt;
    }

    template <typename T>
    [[nodiscard]] T getOrDefault(Key const& p_key, T p_default = T()) const
    {
        if (auto value = get<T>(p_key))
        {
            return *value;
        }
        return p_default;
    }

    [[nodiscard]] bool has(Key const& p_key) const;

    void remove(Key const& p_key)
    {
        m_data.erase(p_key);
        invalidateKey(p_key);
    }

    [[nodiscard]] std::shared_ptr<Blackboard> createChild()
    {
        return std::make_shared<Blackboard>(shared_from_this());
    }

    void setPortRemapping(
        std::unordered_map<std::string, std::string> const& p_remapping)
    {
        m_portRemapping = p_remapping;
    }

    [[nodiscard]] std::string dump(std::string const& p_title = "Blackboard",
                                   bool p_showParent = false) const;

    [[nodiscard]] std::vector<Key> keys() const;

    void beginTick()
    {
        ++m_tick_generation;
        if (m_read_cache.size() > 256)
        {
            m_read_cache.clear();
        }
    }

    void invalidateKey(Key const& p_key)
    {
        m_read_cache.erase(p_key);
    }

    //! \brief Format a stored value for logging or visualizers.
    [[nodiscard]] static std::string displayValue(Value const& p_value);

    //! \brief Read and format a key for visualizer port display.
    [[nodiscard]] static std::string
    displayKey(Blackboard const* p_blackboard, std::string const& p_key);

private:

    template <typename T>
    [[nodiscard]] std::optional<T> readFromCache(Key const& p_key) const
    {
        auto it = m_read_cache.find(p_key);
        if (it == m_read_cache.end() ||
            it->second.generation != m_tick_generation)
        {
            return std::nullopt;
        }

        if (it->second.value.index() == 0)
        {
            return std::nullopt;
        }

        return detail::valueAs<T>(it->second.value);
    }

    template <typename T>
    void storeInCache(Key const& p_key, T const& /*p_value*/) const
    {
        if (auto it = m_data.find(p_key); it != m_data.end())
        {
            m_read_cache[p_key] = CacheEntry{m_tick_generation, it->second};
        }
    }

    static std::string valueToString(Value const& p_value);

    struct CacheEntry
    {
        std::uint64_t generation = 0;
        Value value;
    };

    friend class BlackboardSerializer;

    std::unordered_map<Key, Value> m_data;
    std::shared_ptr<Blackboard> m_parent;
    std::unordered_map<std::string, std::string> m_portRemapping;
    mutable std::uint64_t m_tick_generation = 0;
    mutable std::unordered_map<Key, CacheEntry> m_read_cache;
};

} // namespace bt
