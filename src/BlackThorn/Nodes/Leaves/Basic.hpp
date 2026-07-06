/**
 * @file Basic.hpp
 * @brief Basic leaf nodes: Success, Failure.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Leaves/Leaf.hpp"

namespace bt {

// ****************************************************************************
//! \brief Simple leaf that always returns SUCCESS.
// ****************************************************************************
class Success final: public Leaf
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Success".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Success";
    }

    // ------------------------------------------------------------------------
    //! \brief Return the node kind used when adopting factory-built nodes.
    //! \return Always \ref NodeKind::Success.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeKind registrationKind() const override
    {
        return NodeKind::Success;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSuccess(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSuccess(*this);
    }
};

// ****************************************************************************
//! \brief Simple leaf that always returns FAILURE.
// ****************************************************************************
class Failure final: public Leaf
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Failure".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Failure";
    }

    // ------------------------------------------------------------------------
    //! \brief Return the node kind used when adopting factory-built nodes.
    //! \return Always \ref NodeKind::Failure.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeKind registrationKind() const override
    {
        return NodeKind::Failure;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitFailure(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitFailure(*this);
    }
};

template <> struct NodeKindTraits<Success>
{
    static constexpr NodeKind value = NodeKind::Success;
};
template <> struct NodeKindTraits<Failure>
{
    static constexpr NodeKind value = NodeKind::Failure;
};

} // namespace bt
