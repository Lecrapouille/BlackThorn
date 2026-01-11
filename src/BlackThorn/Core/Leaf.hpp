// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file Leaf.hpp
 * @brief Base class for leaf nodes that have no children.
 *
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
