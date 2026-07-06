/**
 * @file Wait.hpp
 * @brief Wait leaf node.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Leaves/Leaf.hpp"

namespace bt {

// ****************************************************************************
//! \brief The Wait leaf waits for a specified duration and then returns
//! SUCCESS. This is useful for adding delays in behavior tree execution.
// ****************************************************************************
class Wait final: public Leaf
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Wait".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Wait";
    }

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a wait duration.
    //! \param[in] p_milliseconds The wait duration in milliseconds.
    // ------------------------------------------------------------------------
    explicit Wait(size_t p_milliseconds = 1000)
        : m_default_duration(p_milliseconds)
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Copy default duration into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.duration_ms = m_default_duration;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the wait duration in milliseconds.
    //! \return The wait duration.
    // ------------------------------------------------------------------------
    [[nodiscard]] size_t getMilliseconds() const
    {
        return tree().config(index()).duration_ms;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitWait(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitWait(*this);
    }

private:

    //! \brief Default wait duration when no port remapping is provided.
    size_t m_default_duration;
};

template <> struct NodeKindTraits<Wait>
{
    static constexpr NodeKind value = NodeKind::Wait;
};

} // namespace bt
