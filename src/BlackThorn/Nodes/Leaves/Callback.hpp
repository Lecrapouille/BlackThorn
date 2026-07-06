/**
 * @file Callback.hpp
 * @brief Callback leaf node backed by std::function.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Leaves/Leaf.hpp"

#include <functional>
#include <type_traits>

namespace bt {

// ****************************************************************************
//! \brief Leaf node that executes a user-provided callback each tick.
//!
//! Replaces the former Action/SugarAction hierarchy. The callback and optional
//! reset handler are copied into the parallel \ref NodeConfig slot of the
//! owning tree when the node is registered.
// ****************************************************************************
class CallbackLeaf: public Leaf
{
public:

    //! \brief Function invoked by the tree interpreter on each tick.
    using Function = std::function<Status()>;
    //! \brief Optional function invoked when the node is reset.
    using ResetFunction = std::function<void()>;

    // ------------------------------------------------------------------------
    //! \brief Get the YAML type name for callback/action nodes.
    //! \return The string "Action".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "Action";
    }

    // ------------------------------------------------------------------------
    //! \brief Create a callback leaf from a tick function.
    //! \param[in] p_func Function returning the node status.
    // ------------------------------------------------------------------------
    explicit CallbackLeaf(Function p_func) : m_func(std::move(p_func)) {}

    // ------------------------------------------------------------------------
    //! \brief Create a callback leaf with an attached blackboard.
    //! \param[in] p_func Function returning the node status.
    //! \param[in] p_blackboard Blackboard used by the callback.
    // ------------------------------------------------------------------------
    CallbackLeaf(Function p_func, Blackboard::Ptr p_blackboard)
        : m_func(std::move(p_func))
    {
        setBlackboard(std::move(p_blackboard));
    }

    // ------------------------------------------------------------------------
    //! \brief Create a callback leaf with tick and reset handlers.
    //! \param[in] p_func Function returning the node status.
    //! \param[in] p_reset Function invoked on node reset.
    // ------------------------------------------------------------------------
    CallbackLeaf(Function p_func, ResetFunction p_reset)
        : m_func(std::move(p_func)), m_reset(std::move(p_reset))
    {
    }

    // ------------------------------------------------------------------------
    //! \brief Copy callbacks into the tree configuration slot.
    //! \param[in,out] p_config Configuration slot to populate.
    // ------------------------------------------------------------------------
    void fillConfig(NodeConfig& p_config) const override
    {
        p_config.callback = m_func;
        p_config.on_reset = m_reset;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the node kind used when adopting factory-built nodes.
    //! \return Always \ref NodeKind::Callback.
    // ------------------------------------------------------------------------
    [[nodiscard]] NodeKind registrationKind() const override
    {
        return NodeKind::Callback;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override;
    void accept(BehaviorTreeVisitor& p_visitor) override;

private:

    //! \brief User callback executed by the tree interpreter.
    Function m_func;
    //! \brief Optional reset callback invoked by the tree interpreter.
    ResetFunction m_reset;
};

template <typename T>
struct NodeKindTraits<T, std::enable_if_t<std::is_base_of_v<CallbackLeaf, T>>>
{
    static constexpr NodeKind value = NodeKind::Callback;
};

} // namespace bt
