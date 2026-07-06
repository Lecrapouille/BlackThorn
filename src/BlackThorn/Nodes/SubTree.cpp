/**
 * @file SubTree.cpp
 * @brief SubTreeNode implementation.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Nodes/SubTree.hpp"

namespace bt {

SubTreeNode::SubTreeNode(std::string p_reference, Tree::Ptr p_tree)
    : m_reference(std::move(p_reference)), m_subtree(std::move(p_tree))
{
}

bool SubTreeNode::isValidCustom() const
{
    return (m_subtree != nullptr) && m_subtree->hasRoot() &&
           m_subtree->isValid();
}

void SubTreeNode::accept(ConstBehaviorTreeVisitor& p_visitor) const
{
    p_visitor.visitSubTree(*this);
}

void SubTreeNode::accept(BehaviorTreeVisitor& p_visitor)
{
    p_visitor.visitSubTree(*this);
}

Blackboard::Ptr SubTreeNode::blackboard() const
{
    return m_subtree ? m_subtree->blackboard() : nullptr;
}

} // namespace bt
