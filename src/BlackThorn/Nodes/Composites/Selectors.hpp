// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file Selectors.hpp
 * @brief Selector composite nodes: Selector, ReactiveSelector,
 * SelectorWithMemory.
 *
 */

#pragma once

#include "BlackThorn/Nodes/Composites/Composite.hpp"

namespace bt {

class Selector final: public Composite
{
public:

    [[nodiscard]] static constexpr char const* toString()
    {
        return "Selector";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSelector(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSelector(*this);
    }
};

class ReactiveSelector final: public Composite
{
public:

    [[nodiscard]] static constexpr char const* toString()
    {
        return "ReactiveSelector";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitReactiveSelector(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitReactiveSelector(*this);
    }
};

class SelectorWithMemory final: public Composite
{
public:

    [[nodiscard]] static constexpr char const* toString()
    {
        return "SelectorWithMemory";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSelectorWithMemory(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSelectorWithMemory(*this);
    }
};

template <> struct NodeKindTraits<Selector>
{
    static constexpr NodeKind value = NodeKind::Selector;
};
template <> struct NodeKindTraits<ReactiveSelector>
{
    static constexpr NodeKind value = NodeKind::ReactiveSelector;
};
template <> struct NodeKindTraits<SelectorWithMemory>
{
    static constexpr NodeKind value = NodeKind::SelectorWithMemory;
};

} // namespace bt
