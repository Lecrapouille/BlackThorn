// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file SubTree.cpp
 * @brief SubTreeNode implementation.
 *
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
