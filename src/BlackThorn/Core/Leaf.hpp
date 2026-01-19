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
//! The blackboard is inherited from Node base class.
// ****************************************************************************
class Leaf: public Node
{
public:

    // ------------------------------------------------------------------------
    //! \brief Default constructor.
    // ------------------------------------------------------------------------
    Leaf() = default;

    // ------------------------------------------------------------------------
    //! \brief Check if the leaf node is valid.
    //! \return True if the leaf node is valid, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isValid() const override
    {
        return true;
    }
};

} // namespace bt
