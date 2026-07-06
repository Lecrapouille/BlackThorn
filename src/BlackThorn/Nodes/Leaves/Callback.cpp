// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file Callback.cpp
 * @brief Callback leaf node implementation.
 *
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
