/**
 * @file SubTree.hpp
 * @brief Node that embeds and executes a nested behavior tree.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Node.hpp"
#include "BlackThorn/Nodes/Tree.hpp"

#include <cassert>
#include <memory>
#include <string>

namespace bt {

// ****************************************************************************
//! \brief Node that executes another Tree instance as a child.
//!
//! A SubTreeNode owns a nested \ref Tree and ticks it when running.
//! Output port remapping is propagated from the nested tree to the parent
//! blackboard through \ref Tree::propagateOutputs().
//!
//! \code{.cpp}
//! using namespace bt;
//!
//! auto subtree = Tree::create();
//! subtree->createRoot<Success>();
//!
//! auto tree = Tree::create();
//! tree->createRoot<SubTreeNode>("patrol", std::move(subtree));
//! \endcode
// ****************************************************************************
class SubTreeNode final: public Node
{
public:

    // ------------------------------------------------------------------------
    //! \brief Construct a subtree node referencing a nested tree.
    //! \param[in] p_reference Reference name used in YAML definitions.
    //! \param[in] p_tree Owned nested tree executed on tick.
    // ------------------------------------------------------------------------
    SubTreeNode(std::string p_reference, Tree::Ptr p_tree);

    // ------------------------------------------------------------------------
    //! \brief Get the string representation of the node type.
    //! \return The string "SubTree".
    // ------------------------------------------------------------------------
    [[nodiscard]] static constexpr char const* toString()
    {
        return "SubTree";
    }

    // ------------------------------------------------------------------------
    //! \brief Validate nested tree presence and structure.
    //! \return True if the nested tree has a valid root graph.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool isValidCustom() const override;

    // ------------------------------------------------------------------------
    //! \brief Return the YAML reference name of this subtree.
    //! \return Subtree reference string.
    // ------------------------------------------------------------------------
    [[nodiscard]] std::string const& reference() const
    {
        return m_reference;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the nested tree (non-const).
    //! \pre \c m_subtree is not null.
    //! \return Reference to the owned nested tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] Tree& subtree()
    {
        assert(m_subtree != nullptr);
        return *m_subtree;
    }

    // ------------------------------------------------------------------------
    //! \brief Return the nested tree (const).
    //! \pre \c m_subtree is not null.
    //! \return Const reference to the owned nested tree.
    // ------------------------------------------------------------------------
    [[nodiscard]] Tree const& subtree() const
    {
        assert(m_subtree != nullptr);
        return *m_subtree;
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override;
    void accept(BehaviorTreeVisitor& p_visitor) override;

    // ------------------------------------------------------------------------
    //! \brief Return the blackboard attached to the nested tree.
    //! \return Nested tree blackboard, or nullptr if absent.
    // ------------------------------------------------------------------------
    [[nodiscard]] Blackboard::Ptr blackboard() const;

private:

    //! \brief Reference name used to resolve the subtree definition.
    std::string m_reference;
    //! \brief Owned nested tree executed on each tick.
    Tree::Ptr m_subtree;
};

template <>
struct NodeKindTraits<SubTreeNode>
{
    static constexpr NodeKind value = NodeKind::SubTree;
};

} // namespace bt
