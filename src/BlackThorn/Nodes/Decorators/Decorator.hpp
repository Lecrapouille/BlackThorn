/**
 * @file Decorator.hpp
 * @brief Base class for decorator nodes that can have only one child.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Node.hpp"
#include "BlackThorn/Nodes/Tree.hpp"

namespace bt {

// ****************************************************************************
//! \brief Base class for decorator nodes that can have only one child.
//! Decorator nodes are used to modify the behavior of their child node.
// ****************************************************************************
class Decorator: public Node
{
public:

    // ------------------------------------------------------------------------
    //! \brief Set the child node index of the decorator.
    //! \param[in] p_child_index Storage index of the child inside the tree.
    // ------------------------------------------------------------------------
    void setChildIndex(std::size_t p_child_index)
    {
        m_child_index = p_child_index;
        tree().runtime().setDecoratorChild(index(), p_child_index);
    }

    // ------------------------------------------------------------------------
    //! \brief Create and attach a single child node of type T.
    //! \tparam T Concrete node type derived from \ref Node.
    //! \param[in] p_args Arguments forwarded to the node constructor.
    //! \return A reference to the new child node.
    // ------------------------------------------------------------------------
    template <class T, typename... Args>
    [[nodiscard]] inline T& createChild(Args&&... p_args)
    {
        static_assert(std::is_base_of_v<Node, T>, "T must inherit from Node");
        T& child = tree().emplaceNode<T>(std::forward<Args>(p_args)...);
        setChildIndex(child.index());
        return child;
    }

    // ------------------------------------------------------------------------
    //! \brief Check if the decorator has a child node.
    //! \return True if the decorator has a child node, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline bool hasChild() const
    {
        return m_child_index != INVALID_NODE_INDEX;
    }

    // ------------------------------------------------------------------------
    //! \brief Get the child node of the decorator (const version).
    //! \note The child node shall exist. Use hasChild() to check.
    //! \return The child node reference.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Node const& childNode() const
    {
        return tree().node(m_child_index);
    }

    // ------------------------------------------------------------------------
    //! \brief Get the child node of the decorator (non-const version).
    //! \note The child node shall exist. Use hasChild() to check.
    //! \return The child node reference.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline Node& childNode()
    {
        return tree().node(m_child_index);
    }

protected:

    //! \brief Storage index of the single child node.
    std::size_t m_child_index = INVALID_NODE_INDEX;
};

} // namespace bt
