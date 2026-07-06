/**
 * @file Condition.hpp
 * @brief Condition leaf node.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Leaves/Leaf.hpp"

#include <functional>

namespace bt {

// ****************************************************************************
//! \brief Condition node that evaluates a user-provided predicate.
//! Returns SUCCESS when the predicate is true, FAILURE otherwise.
// ****************************************************************************
class Condition final: public Leaf
{
public:

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "Condition".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Condition";
    }

    // ------------------------------------------------------------------------
    //! \brief Type alias for the condition function.
    // ------------------------------------------------------------------------
    using Function = std::function<bool()>;

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a function to evaluate.
    //! \param[in] p_func The function to evaluate when the condition runs.
    // ------------------------------------------------------------------------
    explicit Condition(Function p_func) : m_func(std::move(p_func)) {}

    // ------------------------------------------------------------------------
    //! \brief Constructor taking a function and blackboard.
    //! \param[in] p_func The function to evaluate when the condition runs.
    //! \param[in] p_blackboard The blackboard to use.
    // ------------------------------------------------------------------------
    Condition(Function p_func, Blackboard::Ptr p_blackboard)
        : m_func(std::move(p_func))
    {
        setBlackboard(p_blackboard);
    }

    // ------------------------------------------------------------------------
    //! \brief Copy predicate into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.condition = m_func;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the node kind used when adopting factory-built nodes.
    //! \return Always \ref NodeKind::Condition.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeKind registrationKind() const override
    {
        return NodeKind::Condition;
    }

    // ------------------------------------------------------------------------
    //! \brief Check whether the predicate callback is set.
    //! \return True if a condition function was provided.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isValidCustom() const override
    {
        return static_cast<bool>(m_func);
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitCondition(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitCondition(*this);
    }

private:

    //! \brief Predicate evaluated by the tree interpreter on each tick.
    Function m_func = nullptr;
};

template <> struct NodeKindTraits<Condition>
{
    static constexpr NodeKind value = NodeKind::Condition;
};

} // namespace bt
