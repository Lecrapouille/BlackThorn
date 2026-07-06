/**
 * @file Temporal.hpp
 * @brief Temporal decorator nodes: Timeout, Delay, Cooldown.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Decorators/Decorator.hpp"

#include <chrono>

namespace bt {

// ****************************************************************************
//! \brief The Timeout decorator runs its child with a time limit.
//! If the child does not complete within the specified time, it returns
//! FAILURE and halts the child. This is useful for preventing long-running
//! actions from blocking the tree.
//!
//! The timeout duration can be read from the blackboard via port remapping.
// ****************************************************************************
class Timeout final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Timeout".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Timeout";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a timeout duration.
    //! \param[in] p_milliseconds The timeout duration in milliseconds.
    // ------------------------------------------------------------------------
    explicit Timeout(size_t p_milliseconds = 1000)
        : m_default_timeout(p_milliseconds)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Get the ports provided by the node.
    //! \return The ports provided by the node.
    // ------------------------------------------------------------------------
    [[nodiscard]] PortList providedPorts() const override
    {
        PortList ports;
        ports.addInput<size_t>("milliseconds");
        return ports;
    }

    // ------------------------------------------------------------------------
    //! \brief Copy default timeout into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.duration_ms = m_default_timeout;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the active timeout duration in milliseconds.
    //! \return Runtime timeout after port remapping is applied.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getMilliseconds() const
    {
        return tree().runtime().duration_ms_runtime[index()];
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitTimeout(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitTimeout(*this);
    }

private:

    //! \brief Default timeout when no port remapping is provided.
    size_t m_default_timeout;
};

// ****************************************************************************
//! \brief The Delay decorator waits before ticking its child.
//! Returns RUNNING until the delay elapses, then forwards to the child.
// ****************************************************************************
class Delay final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Delay".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Delay";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a delay duration.
    //! \param[in] p_milliseconds The delay duration in milliseconds.
    // ------------------------------------------------------------------------
    explicit Delay(size_t p_milliseconds = 1000)
        : m_default_delay(p_milliseconds)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Copy default delay into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.duration_ms = m_default_delay;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the delay duration in milliseconds.
    //! \return Configured delay duration.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getMilliseconds() const
    {
        return tree().config(index()).duration_ms;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitDelay(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitDelay(*this);
    }

private:

    //! \brief Default delay when no port remapping is provided.
    size_t m_default_delay;
};

// ****************************************************************************
//! \brief The Cooldown decorator blocks re-execution for a duration after the
//! child completes. While cooling down, the decorator returns FAILURE.
// ****************************************************************************
class Cooldown final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Cooldown".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Cooldown";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a cooldown duration.
    //! \param[in] p_milliseconds The cooldown duration in milliseconds.
    // ------------------------------------------------------------------------
    explicit Cooldown(size_t p_milliseconds = 1000)
        : m_default_cooldown(p_milliseconds)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Copy default cooldown into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.duration_ms = m_default_cooldown;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the cooldown duration in milliseconds.
    //! \return Configured cooldown duration.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getMilliseconds() const
    {
        return tree().config(index()).duration_ms;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitCooldown(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitCooldown(*this);
    }

private:

    //! \brief Default cooldown when no port remapping is provided.
    size_t m_default_cooldown;
};

template <> struct NodeKindTraits<Timeout>
{
    static constexpr NodeKind value = NodeKind::Timeout;
};
template <> struct NodeKindTraits<Delay>
{
    static constexpr NodeKind value = NodeKind::Delay;
};
template <> struct NodeKindTraits<Cooldown>
{
    static constexpr NodeKind value = NodeKind::Cooldown;
};

} // namespace bt
