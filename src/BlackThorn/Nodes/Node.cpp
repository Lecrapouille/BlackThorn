/**
 * @file Node.cpp
 * @brief Base node implementation.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Nodes/Node.hpp"

#include "BlackThorn/Nodes/Tree.hpp"

namespace bt {

void Node::bindToTree(Tree& p_tree, size_t p_index)
{
    m_tree = &p_tree;
    m_index = p_index;
}

Tree& Node::tree()
{
    assert(m_tree != nullptr);
    return *m_tree;
}

Tree const& Node::tree() const
{
    assert(m_tree != nullptr);
    return *m_tree;
}

uint32_t Node::visualizerId() const
{
    assert(m_tree != nullptr);
    return m_tree->metadata().visualizerId(m_index);
}

Status Node::tick()
{
    assert(m_tree != nullptr);
    return m_tree->tickNode(m_index);
}

Status Node::status() const
{
    assert(m_tree != nullptr);
    return m_tree->runtime().statuses[m_index];
}

char const* Node::typeName() const
{
    assert(m_tree != nullptr);
    return toString(m_tree->kind(m_index));
}

void Node::reset()
{
    assert(m_tree != nullptr);
    m_tree->resetNode(m_index);
}

void Node::halt()
{
    assert(m_tree != nullptr);
    m_tree->haltNode(m_index);
}

bool Node::isValid() const
{
    if (m_tree == nullptr)
    {
        return false;
    }
    return m_tree->isNodeValid(m_index);
}

char const* toString(Node::Kind p_kind)
{
    switch (p_kind)
    {
        case Node::Kind::Sequence:
            return "Sequence";
        case Node::Kind::ReactiveSequence:
            return "ReactiveSequence";
        case Node::Kind::SequenceWithMemory:
            return "SequenceWithMemory";
        case Node::Kind::Selector:
            return "Selector";
        case Node::Kind::ReactiveSelector:
            return "ReactiveSelector";
        case Node::Kind::SelectorWithMemory:
            return "SelectorWithMemory";
        case Node::Kind::Parallel:
            return "Parallel";
        case Node::Kind::ParallelAll:
            return "ParallelAll";
        case Node::Kind::Inverter:
            return "Inverter";
        case Node::Kind::ForceSuccess:
            return "ForceSuccess";
        case Node::Kind::ForceFailure:
            return "ForceFailure";
        case Node::Kind::RunOnce:
            return "RunOnce";
        case Node::Kind::Repeater:
            return "Repeater";
        case Node::Kind::UntilSuccess:
            return "UntilSuccess";
        case Node::Kind::UntilFailure:
            return "UntilFailure";
        case Node::Kind::Timeout:
            return "Timeout";
        case Node::Kind::Delay:
            return "Delay";
        case Node::Kind::Cooldown:
            return "Cooldown";
        case Node::Kind::Success:
            return "Success";
        case Node::Kind::Failure:
            return "Failure";
        case Node::Kind::Wait:
            return "Wait";
        case Node::Kind::SetBlackboard:
            return "SetBlackboard";
        case Node::Kind::Condition:
            return "Condition";
        case Node::Kind::Callback:
            return "Callback";
        case Node::Kind::SubTree:
            return "SubTree";
    }
    __builtin_unreachable();
}

bool isComposite(Node::Kind p_kind)
{
    switch (p_kind)
    {
        case Node::Kind::Sequence:
        case Node::Kind::ReactiveSequence:
        case Node::Kind::SequenceWithMemory:
        case Node::Kind::Selector:
        case Node::Kind::ReactiveSelector:
        case Node::Kind::SelectorWithMemory:
        case Node::Kind::Parallel:
        case Node::Kind::ParallelAll:
            return true;

        case Node::Kind::Inverter:
        case Node::Kind::ForceSuccess:
        case Node::Kind::ForceFailure:
        case Node::Kind::RunOnce:
        case Node::Kind::Repeater:
        case Node::Kind::UntilSuccess:
        case Node::Kind::UntilFailure:
        case Node::Kind::Timeout:
        case Node::Kind::Delay:
        case Node::Kind::Cooldown:
        case Node::Kind::Success:
        case Node::Kind::Failure:
        case Node::Kind::Wait:
        case Node::Kind::SetBlackboard:
        case Node::Kind::Condition:
        case Node::Kind::Callback:
        case Node::Kind::SubTree:
            return false;
    }
    __builtin_unreachable();
}

bool isDecorator(Node::Kind p_kind)
{
    switch (p_kind)
    {
        case Node::Kind::Inverter:
        case Node::Kind::ForceSuccess:
        case Node::Kind::ForceFailure:
        case Node::Kind::RunOnce:
        case Node::Kind::Repeater:
        case Node::Kind::UntilSuccess:
        case Node::Kind::UntilFailure:
        case Node::Kind::Timeout:
        case Node::Kind::Delay:
        case Node::Kind::Cooldown:
            return true;

        case Node::Kind::Sequence:
        case Node::Kind::ReactiveSequence:
        case Node::Kind::SequenceWithMemory:
        case Node::Kind::Selector:
        case Node::Kind::ReactiveSelector:
        case Node::Kind::SelectorWithMemory:
        case Node::Kind::Parallel:
        case Node::Kind::ParallelAll:
        case Node::Kind::Success:
        case Node::Kind::Failure:
        case Node::Kind::Wait:
        case Node::Kind::SetBlackboard:
        case Node::Kind::Condition:
        case Node::Kind::Callback:
        case Node::Kind::SubTree:
            return false;
    }
    __builtin_unreachable();
}

} // namespace bt
