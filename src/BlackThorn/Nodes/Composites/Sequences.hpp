// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file Sequences.hpp
 * @brief Sequence composite nodes: Sequence, ReactiveSequence,
 * SequenceWithMemory.
 *
 */

#pragma once

#include "BlackThorn/Nodes/Composites/Composite.hpp"

namespace bt {

class Sequence: public Composite
{
public:

    [[nodiscard]] static constexpr char const* toString()
    {
        return "Sequence";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSequence(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSequence(*this);
    }
};

class ReactiveSequence final: public Composite
{
public:

    [[nodiscard]] static constexpr char const* toString()
    {
        return "ReactiveSequence";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitReactiveSequence(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitReactiveSequence(*this);
    }
};

class SequenceWithMemory final: public Composite
{
public:

    [[nodiscard]] static constexpr char const* toString()
    {
        return "SequenceWithMemory";
    }

    void accept(ConstBehaviorTreeVisitor& p_visitor) const override
    {
        p_visitor.visitSequenceWithMemory(*this);
    }
    void accept(BehaviorTreeVisitor& p_visitor) override
    {
        p_visitor.visitSequenceWithMemory(*this);
    }
};

template <> struct NodeKindTraits<Sequence>
{
    static constexpr NodeKind value = NodeKind::Sequence;
};
template <> struct NodeKindTraits<ReactiveSequence>
{
    static constexpr NodeKind value = NodeKind::ReactiveSequence;
};
template <> struct NodeKindTraits<SequenceWithMemory>
{
    static constexpr NodeKind value = NodeKind::SequenceWithMemory;
};

} // namespace bt
