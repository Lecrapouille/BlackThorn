/**
 * @file Logical.hpp
 * @brief Logical decorator nodes: Inverter, ForceSuccess, ForceFailure,
 * RunOnce.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Decorators/Decorator.hpp"

namespace bt {

// ****************************************************************************
//! \brief The ForceSuccess decorator returns RUNNING if the child is RUNNING,
//! else returns SUCCESS, regardless of what happens to the child.
// ****************************************************************************
class ForceSuccess final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "ForceSuccess".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "ForceSuccess";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitForceSuccess(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitForceSuccess(*this);
    }
};

// ****************************************************************************
//! \brief The ForceFailure decorator returns RUNNING if the child is RUNNING,
//! else returns FAILURE, regardless of what happens to the child.
// ****************************************************************************
class ForceFailure final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "ForceFailure".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "ForceFailure";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitForceFailure(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitForceFailure(*this);
    }
};

// ****************************************************************************
//! \brief The Inverter decorator returns RUNNING if the child is RUNNING,
//! else returns the opposite of the child's status, i.e. FAILURE becomes
//! SUCCESS and SUCCESS becomes FAILURE.
// ****************************************************************************
class Inverter final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Inverter".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Inverter";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitInverter(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitInverter(*this);
    }
};

// ****************************************************************************
//! \brief The RunOnce decorator executes its child only once.
//! Subsequent ticks return the cached status without re-running the child.
// ****************************************************************************
class RunOnce final: public Decorator
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "RunOnce".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "RunOnce";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitRunOnce(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitRunOnce(*this);
    }
};

template <> struct NodeKindTraits<Inverter>
{
    static constexpr NodeKind value = NodeKind::Inverter;
};
template <> struct NodeKindTraits<ForceSuccess>
{
    static constexpr NodeKind value = NodeKind::ForceSuccess;
};
template <> struct NodeKindTraits<ForceFailure>
{
    static constexpr NodeKind value = NodeKind::ForceFailure;
};
template <> struct NodeKindTraits<RunOnce>
{
    static constexpr NodeKind value = NodeKind::RunOnce;
};

} // namespace bt
