/**
 * @file Callback.cpp
 * @brief Callback leaf node implementation.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Nodes/Leaves/Callback.hpp"

#include "BlackThorn/Visitors/Visitor.hpp"

namespace bt {

void CallbackLeaf::accept(ConstBehaviorTreeVisitor& p_visitor) const
{
    p_visitor.visitCallback(*this);
}

void CallbackLeaf::accept(BehaviorTreeVisitor& p_visitor)
{
    p_visitor.visitCallback(*this);
}

} // namespace bt
