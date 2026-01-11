/**
 * @file Leaf.hpp
 * @brief Base class for leaf nodes that have no children.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Core/Node.hpp"

namespace bt {

// ****************************************************************************
//! \brief Base class for leaf nodes that have no children.
//! Leaf nodes are the nodes that actually do the work.
// ****************************************************************************
class Leaf: public Node
{
public:

    // ------------------------------------------------------------------------
    //! \brief Default constructor without blackboard.
    // ------------------------------------------------------------------------
    Leaf() = default;

    // ------------------------------------------------------------------------
    //! \brief Constructor with blackboard.
    // ------------------------------------------------------------------------
    explicit Leaf(Blackboard::Ptr p_blackboard) : m_blackboard(p_blackboard) {}

    // ------------------------------------------------------------------------
    //! \brief Get the blackboard for the node
    //! \return The blackboard for the node
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Blackboard::Ptr getBlackboard() const
    {
        return m_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Assign a new blackboard to the leaf node.
    //! \param[in] p_blackboard The blackboard to use.
    // ------------------------------------------------------------------------
    void setBlackboard(Blackboard::Ptr const& p_blackboard)
    {
        m_blackboard = p_blackboard;
    }

    // ------------------------------------------------------------------------
    //! \brief Check if the leaf node is valid.
    //! \return True if the leaf node is valid, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isValid() const override
    {
        return true;
    }

protected:

    //! \brief The blackboard for the node
    Blackboard::Ptr m_blackboard = nullptr;
};

} // namespace bt
