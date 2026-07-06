/**
 * @file Composite.hpp
 * @brief Base class for composite nodes that can have multiple children.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Node.hpp"
#include "BlackThorn/Nodes/Tree.hpp"
#include <vector>

namespace bt {

// ****************************************************************************
//! \brief Base class for composite nodes that can have multiple children.
//! Composite nodes are used to control the flow of the behavior tree.
// ****************************************************************************
class Composite: public Node
{
public:

    // ------------------------------------------------------------------------
    //! \brief Register an existing child node index.
    //! \param[in] p_child_index Storage index of the child inside the tree.
    // ------------------------------------------------------------------------
    void addChildIndex(std::size_t p_child_index)
    {
        m_child_indices.push_back(p_child_index);
        tree().runtime().appendChild(index(), p_child_index);
    }

    // ------------------------------------------------------------------------
    //! \brief Create and add a new child node of type T.
    //! \tparam T Concrete node type derived from \ref Node.
    //! \param[in] p_args Arguments forwarded to the node constructor.
    //! \return A reference to the new child node.
    // ------------------------------------------------------------------------
    template <class T, typename... Args>
    [[nodiscard]] inline T& addChild(Args&&... p_args)
    {
        static_assert(std::is_base_of_v<Node, T>, "T must inherit from Node");
        T& child = tree().emplaceNode<T>(std::forward<Args>(p_args)...);
        addChildIndex(child.index());
        return child;
    }

    // ------------------------------------------------------------------------
    //! \brief Check if the composite node has children.
    //! \return True if the composite node has children, false otherwise.
    // ------------------------------------------------------------------------
    [[nodiscard]] inline bool hasChildren() const
    {
        return !m_child_indices.empty();
    }

    // ------------------------------------------------------------------------
    //! \brief Return the storage indices of child nodes.
    //! \return Const reference to the vector of child indices.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::vector<std::size_t> const& childIndices() const
    {
        return m_child_indices;
    }

    // ------------------------------------------------------------------------
    //! \brief Access a child node by its position among siblings.
    //! \param[in] p_index Child position (0-based).
    //! \return Reference to the child node.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node& childAt(std::size_t p_index)
    {
        return tree().node(m_child_indices[p_index]);
    }

    // ------------------------------------------------------------------------
    //! \brief Access a child node by its position among siblings (const).
    //! \param[in] p_index Child position (0-based).
    //! \return Const reference to the child node.
    // ------------------------------------------------------------------------
    [[nodiscard]] Node const& childAt(std::size_t p_index) const
    {
        return tree().node(m_child_indices[p_index]);
    }

protected:

    //! \brief Storage indices of child nodes inside the owning tree.
    std::vector<std::size_t> m_child_indices;
};

} // namespace bt
