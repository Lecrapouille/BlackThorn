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
//!
//! Key features:
//! - Parent/child scopes with transparent key lookup.
//! - Per-tick read cache to avoid repeated variant conversions.
//! - Port remapping for subtree blackboard wiring.
// ****************************************************************************
class Blackboard final: public std::enable_shared_from_this<Blackboard>
{
    friend class BlackboardSerializer;

public:

    using Key = std::string;
    using Value = BlackboardValue;
    using Ptr = std::shared_ptr<Blackboard>;

    // ------------------------------------------------------------------------
    //! \brief Construct a blackboard, optionally linked to a parent scope.
    //! \param[in] p_parent Parent blackboard used for inherited key lookup.
    // ------------------------------------------------------------------------
    explicit Blackboard(Blackboard::Ptr p_parent = nullptr);

    // ------------------------------------------------------------------------
    //! \brief Store a typed value under \p p_key.
    //! \param[in] p_key Entry name.
    //! \param[in] p_value Value to store (converted via \ref
    //! detail::toStoredValue).
    // ------------------------------------------------------------------------
    template <typename T>
    void set(Key const& p_key, T&& p_value)
    {
        m_data[p_key] = detail::toStoredValue(std::forward<T>(p_value));
        invalidateKey(p_key);
    }

    // ------------------------------------------------------------------------
    //! \brief Store a pre-built variant value under \p p_key.
    //! \param[in] p_key Entry name.
    //! \param[in] p_value Stored blackboard value.
    // ------------------------------------------------------------------------
    void setRaw(Key const& p_key, Value const& p_value);

    // ------------------------------------------------------------------------
    //! \brief Read the raw stored variant for \p p_key.
    //! \param[in] p_key Entry name.
    //! \return The stored value, searching parent scopes when absent locally.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::optional<Value> raw(Key const& p_key) const;

    // ------------------------------------------------------------------------
    //! \brief Read and convert a typed value for \p p_key.
    //! \param[in] p_key Entry name.
    //! \return Converted value, or \c std::nullopt when missing or
    //! incompatible.
    // ------------------------------------------------------------------------
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

    // ------------------------------------------------------------------------
    //! \brief Read a typed value, returning a default when absent.
    //! \param[in] p_key Entry name.
    //! \param[in] p_default Value returned when the key is missing.
    //! \return Stored value or \p p_default.
    // ------------------------------------------------------------------------
    template <typename T>
    [[nodiscard]] T getOrDefault(Key const& p_key, T p_default = T()) const
    {
        if (auto value = get<T>(p_key))
        {
            return *value;
        }
        return p_default;
    }

    // ------------------------------------------------------------------------
    //! \brief Whether \p p_key exists in this scope or a parent.
    //! \param[in] p_key Entry name.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool has(Key const& p_key) const;

    // ------------------------------------------------------------------------
    //! \brief Remove a local entry and invalidate its read cache.
    //! \param[in] p_key Entry name.
    // ------------------------------------------------------------------------
    void remove(Key const& p_key);

    // ------------------------------------------------------------------------
    //! \brief Create a child blackboard that inherits from this one.
    //! \return Shared pointer to the new child scope.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::shared_ptr<Blackboard> createChild();

    // ------------------------------------------------------------------------
    //! \brief Map local port names to parent blackboard keys.
    //! \param[in] p_remapping Local port name → parent key table.
    // ------------------------------------------------------------------------
    void setPortRemapping(
        std::unordered_map<std::string, std::string> const& p_remapping);

    // ------------------------------------------------------------------------
    //! \brief Human-readable dump of stored entries.
    //! \param[in] p_title Header printed at the top of the dump.
    //! \param[in] p_showParent When \c true, append parent entries.
    //! \return Multi-line text representation.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::string dump(std::string const& p_title = "Blackboard",
                                   bool p_showParent = false) const;

    // ------------------------------------------------------------------------
    //! \brief List keys stored locally (parent keys excluded).
    //! \return Vector of local key names.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::vector<Key> keys() const;

    // ------------------------------------------------------------------------
    //! \brief Begin a new tree tick; bumps the read-cache generation.
    //!
    //! Clears the cache when it grows beyond 256 entries to bound memory.
    // ------------------------------------------------------------------------
    void beginTick();

    // ------------------------------------------------------------------------
    //! \brief Drop cached reads for \p p_key after a write.
    //! \param[in] p_key Entry name whose cache slot is invalidated.
    // ------------------------------------------------------------------------
    void invalidateKey(Key const& p_key);

    // ------------------------------------------------------------------------
    //! \brief Format a stored value for logging or visualizers.
    //! \param[in] p_value Stored blackboard value.
    //! \return Short human-readable text.
    // ------------------------------------------------------------------------
    [[nodiscard]] static std::string displayValue(Value const& p_value);

    // ------------------------------------------------------------------------
    //! \brief Read and format a key for visualizer port display.
    //! \param[in] p_blackboard Blackboard to query (may be \c nullptr).
    //! \param[in] p_key Entry name.
    //! \return Formatted value, or an empty string when absent.
    // ------------------------------------------------------------------------
    [[nodiscard]] static std::string displayKey(Blackboard const* p_blackboard,
                                                std::string const& p_key);

private:

    // ------------------------------------------------------------------------
    //! \brief Read a typed value from the per-tick cache when still valid.
    //! \param[in] p_key Entry name.
    //! \return Cached value, or \c std::nullopt on miss or stale generation.
    // ------------------------------------------------------------------------
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

    // ------------------------------------------------------------------------
    //! \brief Store the raw variant for \p p_key in the read cache.
    //! \param[in] p_key Entry name.
    //! \param[in] p_value Typed value (only used to trigger instantiation).
    // ------------------------------------------------------------------------
    template <typename T>
    void storeInCache(Key const& p_key, T const& /*p_value*/) const
    {
        if (auto it = m_data.find(p_key); it != m_data.end())
        {
            m_read_cache[p_key] = CacheEntry{m_tick_generation, it->second};
        }
    }

    // ------------------------------------------------------------------------
    //! \brief Format a stored value for \ref dump (includes type suffix).
    //! \param[in] p_value Stored blackboard value.
    //! \return Debug string with type annotation.
    // ------------------------------------------------------------------------
    static std::string valueToString(Value const& p_value);

    // ------------------------------------------------------------------------
    //! \brief One cached read tied to a tick generation counter.
    // ------------------------------------------------------------------------
    struct CacheEntry
    {
        //! \brief Tick generation when this entry was stored.
        std::uint64_t generation = 0;
        //! \brief Raw variant value copied from \ref m_data.
        Value value;
    };

    //! \brief Local key/value entries owned by this scope.
    std::unordered_map<Key, Value> m_data;
    //! \brief Parent scope searched when a key is absent locally.
    std::shared_ptr<Blackboard> m_parent;
    //! \brief Local port name → parent blackboard key remapping for subtrees.
    std::unordered_map<std::string, std::string> m_portRemapping;
    //! \brief Current tick generation; bumped by \ref beginTick.
    mutable std::uint64_t m_tick_generation = 0;
    //! \brief Per-key read cache invalidated each tick or on write.
    mutable std::unordered_map<Key, CacheEntry> m_read_cache;
};

} // namespace bt
