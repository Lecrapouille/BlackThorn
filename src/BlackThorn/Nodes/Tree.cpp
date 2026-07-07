/**
 * @file Tree.cpp
 * @brief Tree container implementation.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/Nodes/Tree.hpp"
#include "BlackThorn/Network/VisualizerClient.hpp"
#include "BlackThorn/Nodes/SubTree.hpp"

#include <chrono>

namespace bt {

namespace {

using Clock = std::chrono::steady_clock;
using Ms = std::chrono::milliseconds;

// ---------------------------------------------------------------------------
//! \brief Read a typed input port through the public node accessor.
// ---------------------------------------------------------------------------
template <typename T>
std::optional<T> readPort(Node const& p_node, std::string const& p_port)
{
    return p_node.getInputPublic<T>(p_port);
}

// ---------------------------------------------------------------------------
//! \brief Recursively search for a named subtree node.
// ---------------------------------------------------------------------------
SubTreeNode* findSubTreeInNodes(Tree const& p_tree,
                                std::vector<Node*> const& p_nodes,
                                std::string const& p_name)
{
    for (std::size_t i = 0; i < p_nodes.size(); ++i)
    {
        if (p_tree.kind(i) == NodeKind::SubTree && p_nodes[i]->name == p_name)
        {
            return static_cast<SubTreeNode*>(p_nodes[i]);
        }
    }

    for (std::size_t i = 0; i < p_nodes.size(); ++i)
    {
        if (p_tree.kind(i) == NodeKind::SubTree)
        {
            auto* subtree = static_cast<SubTreeNode*>(p_nodes[i]);
            if (auto* found = findSubTreeInNodes(
                    subtree->subtree(), subtree->subtree().nodes(), p_name))
            {
                return found;
            }
        }
    }

    return nullptr;
}

} // namespace

void Tree::propagateOutputs() const
{
    if (!m_parentBlackboard || m_outputRemapping.empty() || !m_blackboard)
    {
        return;
    }

    for (auto const& [childKey, parentKey] : m_outputRemapping)
    {
        if (auto value = m_blackboard->raw(childKey); value)
        {
            m_parentBlackboard->setRaw(parentKey, *value);
        }
    }
}

bool Tree::isValid() const
{
    return hasRoot() && isNodeValid(m_root_index);
}

// ---------------------------------------------------------------------------
//! \brief Execute one node using setup/run/teardown phases.
// ---------------------------------------------------------------------------
Status Tree::tickNode(std::size_t p_index)
{
    Status& status = m_runtime.statuses[p_index];
    if (status != Status::RUNNING)
    {
        status = setUpNode(p_index);
    }
    if (status != Status::FAILURE)
    {
        status = runNode(p_index);
        if (status != Status::RUNNING)
        {
            tearDownNode(p_index, status);
        }
    }
    return status;
}

void Tree::resetNode(std::size_t p_index)
{
    auto const& runtime = m_runtime;
    NodeKind const node_kind = kind(p_index);

    if (isComposite(node_kind))
    {
        for (std::size_t i = 0; i < runtime.childCount(p_index); ++i)
        {
            resetNode(runtime.childAt(p_index, i));
        }
        m_runtime.child_cursors[p_index] = 0;
    }
    else if (isDecorator(node_kind))
    {
        if (runtime.decorator_children[p_index] != INVALID_NODE_INDEX)
        {
            resetNode(runtime.decorator_children[p_index]);
        }
        if (node_kind == NodeKind::RunOnce)
        {
            m_runtime.setFlag(p_index, false);
            m_runtime.cached_statuses[p_index] = Status::INVALID;
        }
    }

    if (node_kind == NodeKind::Callback || node_kind == NodeKind::Condition)
    {
        if (config(p_index).on_reset)
        {
            config(p_index).on_reset();
        }
    }

    m_runtime.statuses[p_index] = Status::INVALID;
}

void Tree::haltNode(std::size_t p_index)
{
    if (m_runtime.statuses[p_index] == Status::RUNNING)
    {
        haltNodeInternal(p_index);
    }
    m_runtime.statuses[p_index] = Status::INVALID;
}

void Tree::haltNodeInternal(std::size_t p_index)
{
    auto const& runtime = m_runtime;
    NodeKind const node_kind = kind(p_index);

    if (isComposite(node_kind))
    {
        for (std::size_t i = 0; i < runtime.childCount(p_index); ++i)
        {
            haltNode(runtime.childAt(p_index, i));
        }
        node(p_index).onHaltPublic();
    }
    else if (isDecorator(node_kind))
    {
        if (runtime.decorator_children[p_index] != INVALID_NODE_INDEX)
        {
            haltNode(runtime.decorator_children[p_index]);
        }
        node(p_index).onHaltPublic();
    }
    else if (node_kind == NodeKind::SubTree)
    {
        auto& subtree = static_cast<SubTreeNode&>(node(p_index));
        if (subtree.subtree().hasRoot())
        {
            subtree.subtree().halt();
        }
        node(p_index).onHaltPublic();
    }
    else
    {
        node(p_index).onHaltPublic();
    }
}

bool Tree::isNodeValid(std::size_t p_index) const
{
    NodeKind const node_kind = kind(p_index);
    auto const& runtime = m_runtime;
    NodeConfig const& node_config = config(p_index);

    if (node_kind == NodeKind::Callback)
    {
        return static_cast<bool>(node_config.callback);
    }
    if (node_kind == NodeKind::Condition)
    {
        return static_cast<bool>(node_config.condition);
    }
    if (node_kind == NodeKind::SubTree)
    {
        return node(p_index).isValidCustom();
    }

    if (isComposite(node_kind))
    {
        if (runtime.childCount(p_index) == 0)
        {
            return false;
        }
        for (std::size_t i = 0; i < runtime.childCount(p_index); ++i)
        {
            if (!isNodeValid(runtime.childAt(p_index, i)))
            {
                return false;
            }
        }
        return true;
    }

    if (isDecorator(node_kind))
    {
        return runtime.decorator_children[p_index] != INVALID_NODE_INDEX &&
               isNodeValid(runtime.decorator_children[p_index]);
    }

    return true;
}

Status Tree::setUpNode(std::size_t p_index)
{
    auto& runtime = m_runtime;
    NodeKind const node_kind = kind(p_index);
    NodeConfig const& node_config = config(p_index);
    Node& node_ref = node(p_index);

    if (isComposite(node_kind))
    {
        if (node_kind == NodeKind::ReactiveSequence ||
            node_kind == NodeKind::ReactiveSelector)
        {
            runtime.child_cursors[p_index] = 0;
        }
        else
        {
            runtime.child_cursors[p_index] = 0;
        }
        return Status::RUNNING;
    }

    switch (node_kind)
    {
        case NodeKind::RunOnce:
            if (runtime.flag(p_index))
            {
                return runtime.cached_statuses[p_index];
            }
            return Status::RUNNING;

        case NodeKind::Repeater:
            if (auto reps = readPort<int>(node_ref, "repetitions");
                reps && *reps >= 0)
            {
                runtime.limits[p_index] = static_cast<std::size_t>(*reps);
            }
            else if (auto reps2 =
                         readPort<std::size_t>(node_ref, "repetitions");
                     reps2)
            {
                runtime.limits[p_index] = *reps2;
            }
            else
            {
                runtime.limits[p_index] = node_config.repeater_default;
            }
            runtime.counters[p_index] = 0;
            return Status::RUNNING;

        case NodeKind::UntilSuccess:
        case NodeKind::UntilFailure:
            runtime.counters[p_index] = 0;
            return Status::RUNNING;

        case NodeKind::Timeout:
            if (auto ms = readPort<int>(node_ref, "milliseconds");
                ms && *ms >= 0)
            {
                runtime.duration_ms_runtime[p_index] =
                    static_cast<std::size_t>(*ms);
            }
            else if (auto ms2 = readPort<std::size_t>(node_ref, "milliseconds");
                     ms2)
            {
                runtime.duration_ms_runtime[p_index] = *ms2;
            }
            else
            {
                runtime.duration_ms_runtime[p_index] = node_config.duration_ms;
            }
            runtime.time_points[p_index] = Clock::now();
            return Status::RUNNING;

        case NodeKind::Delay:
            runtime.time_points[p_index] = Clock::now();
            runtime.setFlag(p_index, false);
            return Status::RUNNING;

        case NodeKind::Cooldown:
            if (runtime.flag(p_index))
            {
                auto elapsed = std::chrono::duration_cast<Ms>(
                    Clock::now() - runtime.time_points[p_index]);
                if (elapsed.count() <
                    static_cast<long long>(node_config.duration_ms))
                {
                    return Status::FAILURE;
                }
                runtime.setFlag(p_index, false);
            }
            return Status::RUNNING;

        case NodeKind::Wait:
            runtime.time_points[p_index] = Clock::now();
            return Status::RUNNING;

        case NodeKind::SubTree:
            if (auto& subtree = static_cast<SubTreeNode&>(node_ref);
                subtree.subtree().hasRoot())
            {
                subtree.subtree().reset();
            }
            return Status::RUNNING;

        case NodeKind::Sequence:
        case NodeKind::ReactiveSequence:
        case NodeKind::SequenceWithMemory:
        case NodeKind::Selector:
        case NodeKind::ReactiveSelector:
        case NodeKind::SelectorWithMemory:
        case NodeKind::Parallel:
        case NodeKind::ParallelAll:
        case NodeKind::Inverter:
        case NodeKind::ForceSuccess:
        case NodeKind::ForceFailure:
        case NodeKind::Success:
        case NodeKind::Failure:
        case NodeKind::Condition:
        case NodeKind::Callback:
        case NodeKind::SetBlackboard:
            return Status::RUNNING;
    }
    __builtin_unreachable();
}

std::size_t Tree::decoratorChildIndex(std::size_t p_index) const
{
    return m_runtime.decorator_children[p_index];
}

Status Tree::tickDecoratorChild(std::size_t p_index)
{
    return tickNode(decoratorChildIndex(p_index));
}

Status Tree::runNode(std::size_t p_index)
{
    auto& runtime = m_runtime;
    NodeKind const node_kind = kind(p_index);
    NodeConfig const& node_config = config(p_index);
    Node& node_ref = node(p_index);

    switch (node_kind)
    {
        case NodeKind::Sequence:
        case NodeKind::ReactiveSequence:
        case NodeKind::SequenceWithMemory: {
            if (node_kind == NodeKind::ReactiveSequence)
            {
                runtime.child_cursors[p_index] = 0;
            }

            while (runtime.child_cursors[p_index] < runtime.childCount(p_index))
            {
                Status status = tickNode(
                    runtime.childAt(p_index, runtime.child_cursors[p_index]));
                if (status != Status::SUCCESS)
                {
                    return status;
                }
                ++runtime.child_cursors[p_index];
            }

            if (node_kind == NodeKind::SequenceWithMemory)
            {
                runtime.child_cursors[p_index] = 0;
            }
            return Status::SUCCESS;
        }

        case NodeKind::Selector:
        case NodeKind::ReactiveSelector:
        case NodeKind::SelectorWithMemory: {
            if (node_kind == NodeKind::ReactiveSelector)
            {
                runtime.child_cursors[p_index] = 0;
            }

            while (runtime.child_cursors[p_index] < runtime.childCount(p_index))
            {
                Status status = tickNode(
                    runtime.childAt(p_index, runtime.child_cursors[p_index]));
                if (status != Status::FAILURE)
                {
                    return status;
                }
                ++runtime.child_cursors[p_index];
            }

            if (node_kind == NodeKind::SelectorWithMemory)
            {
                runtime.child_cursors[p_index] = 0;
            }
            return Status::FAILURE;
        }

        case NodeKind::Parallel: {
            int total_success = 0;
            int total_fail = 0;
            for (std::size_t i = 0; i < runtime.childCount(p_index); ++i)
            {
                Status status = tickNode(runtime.childAt(p_index, i));
                if (status == Status::SUCCESS)
                {
                    ++total_success;
                }
                if (status == Status::FAILURE)
                {
                    ++total_fail;
                }
            }
            if (total_success >= node_config.parallel_min_success)
            {
                return Status::SUCCESS;
            }
            if (total_fail >= node_config.parallel_min_fail)
            {
                return Status::FAILURE;
            }
            return Status::RUNNING;
        }

        case NodeKind::ParallelAll: {
            size_t const minimum_success = node_config.parallel_success_on_all
                                               ? runtime.childCount(p_index)
                                               : 1;
            size_t const minimum_fail = node_config.parallel_fail_on_all
                                            ? runtime.childCount(p_index)
                                            : 1;
            size_t total_success = 0;
            size_t total_fail = 0;

            for (std::size_t i = 0; i < runtime.childCount(p_index); ++i)
            {
                Status status = tickNode(runtime.childAt(p_index, i));
                if (status == Status::SUCCESS)
                {
                    ++total_success;
                }
                if (status == Status::FAILURE)
                {
                    ++total_fail;
                }
            }
            if (total_success >= minimum_success)
            {
                return Status::SUCCESS;
            }
            if (total_fail >= minimum_fail)
            {
                return Status::FAILURE;
            }
            return Status::RUNNING;
        }

        case NodeKind::Inverter: {
            Status status = tickDecoratorChild(p_index);
            if (status == Status::SUCCESS)
            {
                return Status::FAILURE;
            }
            if (status == Status::FAILURE)
            {
                return Status::SUCCESS;
            }
            return status;
        }

        case NodeKind::ForceSuccess: {
            Status status = tickDecoratorChild(p_index);
            return status == Status::RUNNING ? Status::RUNNING
                                             : Status::SUCCESS;
        }

        case NodeKind::ForceFailure: {
            Status status = tickDecoratorChild(p_index);
            return status == Status::RUNNING ? Status::RUNNING
                                             : Status::FAILURE;
        }

        case NodeKind::RunOnce: {
            if (runtime.flag(p_index))
            {
                return runtime.cached_statuses[p_index];
            }

            Status status = tickDecoratorChild(p_index);
            if (status != Status::RUNNING)
            {
                runtime.setFlag(p_index, true);
                runtime.cached_statuses[p_index] = status;
            }
            return status;
        }

        case NodeKind::Repeater: {
            Status status = tickDecoratorChild(p_index);
            if (status == Status::RUNNING)
            {
                return Status::RUNNING;
            }

            resetNode(decoratorChildIndex(p_index));

            if (runtime.limits[p_index] > 0)
            {
                ++runtime.counters[p_index];
                if (runtime.counters[p_index] >= runtime.limits[p_index])
                {
                    runtime.counters[p_index] = runtime.limits[p_index];
                    return Status::SUCCESS;
                }
            }
            return Status::RUNNING;
        }

        case NodeKind::UntilSuccess: {
            Status status = tickDecoratorChild(p_index);
            if (status == Status::SUCCESS)
            {
                return Status::SUCCESS;
            }
            if (status == Status::RUNNING)
            {
                return Status::RUNNING;
            }

            resetNode(decoratorChildIndex(p_index));
            if (node_config.until_attempts > 0)
            {
                ++runtime.counters[p_index];
                if (runtime.counters[p_index] >= node_config.until_attempts)
                {
                    runtime.counters[p_index] = node_config.until_attempts;
                    return Status::FAILURE;
                }
            }
            return Status::RUNNING;
        }

        case NodeKind::UntilFailure: {
            Status status = tickDecoratorChild(p_index);
            if (status == Status::FAILURE)
            {
                return Status::SUCCESS;
            }
            if (status == Status::RUNNING)
            {
                return Status::RUNNING;
            }

            resetNode(decoratorChildIndex(p_index));
            if (node_config.until_attempts > 0)
            {
                ++runtime.counters[p_index];
                if (runtime.counters[p_index] >= node_config.until_attempts)
                {
                    runtime.counters[p_index] = node_config.until_attempts;
                    return Status::FAILURE;
                }
            }
            return Status::RUNNING;
        }

        case NodeKind::Timeout: {
            auto elapsed = std::chrono::duration_cast<Ms>(
                Clock::now() - runtime.time_points[p_index]);
            if (elapsed.count() >=
                static_cast<long long>(runtime.duration_ms_runtime[p_index]))
            {
                size_t const child = decoratorChildIndex(p_index);
                if (runtime.statuses[child] == Status::RUNNING)
                {
                    haltNode(child);
                }
                return Status::FAILURE;
            }
            return tickDecoratorChild(p_index);
        }

        case NodeKind::Delay: {
            if (!runtime.flag(p_index))
            {
                auto elapsed = std::chrono::duration_cast<Ms>(
                    Clock::now() - runtime.time_points[p_index]);
                if (elapsed.count() <
                    static_cast<long long>(node_config.duration_ms))
                {
                    return Status::RUNNING;
                }
                runtime.setFlag(p_index, true);
            }
            return tickDecoratorChild(p_index);
        }

        case NodeKind::Cooldown: {
            Status status = tickDecoratorChild(p_index);
            if (status != Status::RUNNING)
            {
                runtime.time_points[p_index] = Clock::now();
                runtime.setFlag(p_index, true);
            }
            return status;
        }

        case NodeKind::Success:
            return Status::SUCCESS;

        case NodeKind::Failure:
            return Status::FAILURE;

        case NodeKind::Wait: {
            auto elapsed = std::chrono::duration_cast<Ms>(
                Clock::now() - runtime.time_points[p_index]);
            if (elapsed.count() >=
                static_cast<long long>(node_config.duration_ms))
            {
                return Status::SUCCESS;
            }
            return Status::RUNNING;
        }

        case NodeKind::SetBlackboard:
            if (node_ref.blackboard())
            {
                node_ref.blackboard()->set(node_config.set_blackboard_key,
                                           node_config.set_blackboard_value);
            }
            return Status::SUCCESS;

        case NodeKind::Condition:
            if (node_config.condition)
            {
                return node_config.condition() ? Status::SUCCESS
                                               : Status::FAILURE;
            }
            return Status::FAILURE;

        case NodeKind::Callback:
            if (node_config.callback)
            {
                return node_config.callback();
            }
            return Status::FAILURE;

        case NodeKind::SubTree: {
            auto& subtree = static_cast<SubTreeNode&>(node_ref);
            if (!subtree.subtree().hasRoot())
            {
                return Status::FAILURE;
            }
            Status status = subtree.subtree().tick();
            subtree.subtree().propagateOutputs();
            return status;
        }
    }
    __builtin_unreachable();
}

void Tree::tearDownNode(std::size_t p_index, Status p_status)
{
    NodeKind const node_kind = kind(p_index);
    if (node_kind == NodeKind::SubTree)
    {
        auto& subtree = static_cast<SubTreeNode&>(node(p_index));
        if (subtree.subtree().hasRoot())
        {
            subtree.subtree().propagateOutputs();
            subtree.subtree().reset();
        }
    }
    else
    {
        node(p_index).onTearDownPublic(p_status);
    }
}
Status Tree::tick()
{
    if (!hasRoot())
    {
        m_status = Status::FAILURE;
        return m_status;
    }
    if (!isNodeValid(m_root_index))
    {
        m_status = Status::FAILURE;
        return m_status;
    }

    if (m_blackboard)
    {
        m_blackboard->beginTick();
    }

    m_status = tickNode(m_root_index);

    if (m_visualizer && m_visualizer->isConnected())
    {
        m_visualizer->sendStateChanges(*this);
    }

    return m_status;
}

void Tree::reset()
{
    if (hasRoot())
    {
        resetNode(m_root_index);
    }
    m_status = Status::INVALID;
}

void Tree::halt()
{
    if (hasRoot())
    {
        haltNode(m_root_index);
    }
    m_status = Status::INVALID;
}

void Tree::accept(ConstBehaviorTreeVisitor& p_visitor) const
{
    p_visitor.visitTree(*this);
}

void Tree::accept(BehaviorTreeVisitor& p_visitor)
{
    p_visitor.visitTree(*this);
}

SubTreeNode* Tree::findSubTree(std::string const& p_name)
{
    return findSubTreeInNodes(*this, m_pool.nodes(), p_name);
}

SubTreeNode const* Tree::findSubTree(std::string const& p_name) const
{
    return findSubTreeInNodes(*this, m_pool.nodes(), p_name);
}

} // namespace bt
