/**
 * @file VisualizerClient.cpp
 * @brief TCP client implementation for behavior tree visualization.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "VisualizerClient.hpp"
#include "../BlackThorn.hpp"

#include <SFML/Network.hpp>
#include <iostream>
#include <sstream>

namespace bt {

// ****************************************************************************
//! \brief Visitor that serializes a behavior tree to YAML format.
//! This produces the same structure as the Builder expects when parsing.
// ****************************************************************************
class YamlExportVisitor: public ConstBehaviorTreeVisitor
{
public:

    std::string indent() const
    {
        return std::string(indent_level * 2, ' ');
    }

    void writeNodeStart(std::string const& p_type, Node const& p_node)
    {
        if (m_is_root)
        {
            yaml << indent() << p_type << ":\n";
            m_is_root = false; // Reset after first use
        }
        else
        {
            yaml << indent() << "- " << p_type << ":\n";
        }
        indent_level++;
        yaml << indent() << "_id: " << p_node.id() << "\n";
        yaml << indent() << "name: " << p_node.name << "\n";
    }

    void writeChildrenStart()
    {
        yaml << indent() << "children:\n";
        indent_level++;
    }

    void writeChildrenEnd()
    {
        indent_level--;
    }

    void writeNodeEnd()
    {
        indent_level--;
    }

    // Composite nodes
    void visitSequence(Sequence const& p_node) override
    {
        writeNodeStart("Sequence", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitReactiveSequence(ReactiveSequence const& p_node) override
    {
        writeNodeStart("ReactiveSequence", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitSequenceWithMemory(SequenceWithMemory const& p_node) override
    {
        writeNodeStart("SequenceWithMemory", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitSelector(Selector const& p_node) override
    {
        writeNodeStart("Selector", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitReactiveSelector(ReactiveSelector const& p_node) override
    {
        writeNodeStart("ReactiveSelector", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitSelectorWithMemory(SelectorWithMemory const& p_node) override
    {
        writeNodeStart("SelectorWithMemory", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitParallel(Parallel const& p_node) override
    {
        writeNodeStart("Parallel", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitParallelAll(ParallelAll const& p_node) override
    {
        writeNodeStart("ParallelAll", p_node);
        if (p_node.hasChildren())
        {
            writeChildrenStart();
            for (auto const& child : p_node.getChildren())
            {
                child->accept(*this);
            }
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    // Decorator nodes
    void visitInverter(Inverter const& p_node) override
    {
        writeNodeStart("Inverter", p_node);
        if (p_node.hasChild())
        {
            writeChildrenStart();
            p_node.getChild().accept(*this);
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitRepeat(Repeat const& p_node) override
    {
        writeNodeStart("Repeat", p_node);
        if (p_node.hasChild())
        {
            writeChildrenStart();
            p_node.getChild().accept(*this);
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitUntilSuccess(UntilSuccess const& p_node) override
    {
        writeNodeStart("UntilSuccess", p_node);
        if (p_node.hasChild())
        {
            writeChildrenStart();
            p_node.getChild().accept(*this);
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitUntilFailure(UntilFailure const& p_node) override
    {
        writeNodeStart("UntilFailure", p_node);
        if (p_node.hasChild())
        {
            writeChildrenStart();
            p_node.getChild().accept(*this);
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitForceSuccess(ForceSuccess const& p_node) override
    {
        writeNodeStart("ForceSuccess", p_node);
        if (p_node.hasChild())
        {
            writeChildrenStart();
            p_node.getChild().accept(*this);
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitForceFailure(ForceFailure const& p_node) override
    {
        writeNodeStart("ForceFailure", p_node);
        if (p_node.hasChild())
        {
            writeChildrenStart();
            p_node.getChild().accept(*this);
            writeChildrenEnd();
        }
        writeNodeEnd();
    }

    void visitTimeout(Timeout const& p_node) override
    {
        writeNodeStart("Timeout", p_node);
        yaml << indent() << "milliseconds: " << p_node.getMilliseconds()
             << "\n";
        if (p_node.hasChild())
        {
            yaml << indent() << "child:\n";
            indent_level++;
            p_node.getChild().accept(*this);
            indent_level--;
        }
        writeNodeEnd();
    }

    void visitDelay(Delay const& p_node) override
    {
        writeNodeStart("Delay", p_node);
        yaml << indent() << "milliseconds: " << p_node.getMilliseconds()
             << "\n";
        if (p_node.hasChild())
        {
            yaml << indent() << "child:\n";
            indent_level++;
            p_node.getChild().accept(*this);
            indent_level--;
        }
        writeNodeEnd();
    }

    void visitCooldown(Cooldown const& p_node) override
    {
        writeNodeStart("Cooldown", p_node);
        yaml << indent() << "milliseconds: " << p_node.getMilliseconds()
             << "\n";
        if (p_node.hasChild())
        {
            yaml << indent() << "child:\n";
            indent_level++;
            p_node.getChild().accept(*this);
            indent_level--;
        }
        writeNodeEnd();
    }

    void visitRunOnce(RunOnce const& p_node) override
    {
        writeNodeStart("RunOnce", p_node);
        if (p_node.hasChild())
        {
            yaml << indent() << "child:\n";
            indent_level++;
            p_node.getChild().accept(*this);
            indent_level--;
        }
        writeNodeEnd();
    }

    // Leaf nodes
    void visitSuccess(Success const& p_node) override
    {
        writeNodeStart("Success", p_node);
        writeNodeEnd();
    }

    void visitFailure(Failure const& p_node) override
    {
        writeNodeStart("Failure", p_node);
        writeNodeEnd();
    }

    void visitCondition(Condition const& p_node) override
    {
        writeNodeStart("Condition", p_node);
        writeNodeEnd();
    }

    void visitAction(Action const& p_node) override
    {
        writeNodeStart("Action", p_node);
        writeNodeEnd();
    }

    void visitSugarAction(SugarAction const& p_node) override
    {
        writeNodeStart("Action", p_node);
        writeNodeEnd();
    }

    void visitSubTree(SubTreeNode const& p_node) override
    {
        writeNodeStart("SubTree", p_node);
        if (p_node.handle())
        {
            yaml << indent() << "reference: " << p_node.handle()->id() << "\n";
            // Inline the subtree content as children for visualization
            Tree const& subtree = p_node.handle()->tree();
            if (subtree.hasRoot())
            {
                writeChildrenStart();
                subtree.getRoot().accept(*this);
                writeChildrenEnd();
            }
        }
        writeNodeEnd();
    }

    void visitWait(Wait const& p_node) override
    {
        writeNodeStart("Wait", p_node);
        yaml << indent() << "milliseconds: " << p_node.getMilliseconds()
             << "\n";
        writeNodeEnd();
    }

    void visitSetBlackboard(SetBlackboard const& p_node) override
    {
        writeNodeStart("SetBlackboard", p_node);
        yaml << indent() << "key: " << p_node.getKey() << "\n";
        yaml << indent() << "value: " << p_node.getValue() << "\n";
        writeNodeEnd();
    }

    void visitTree(Tree const& tree) override
    {
        yaml << "BehaviorTree:\n";
        indent_level = 1;
        if (tree.hasRoot())
        {
            m_is_root = true;
            tree.getRoot().accept(*this);
        }
    }

public:

    std::stringstream yaml;
    size_t indent_level = 0;
    bool m_is_root = false;
};

// ----------------------------------------------------------------------------
VisualizerClient::VisualizerClient() = default;

// ----------------------------------------------------------------------------
VisualizerClient::~VisualizerClient()
{
    disconnect();
}

// ----------------------------------------------------------------------------
bool VisualizerClient::connect(std::string const& p_host, uint16_t p_port)
{
    if (m_socket)
    {
        disconnect();
    }

    m_socket = std::make_unique<sf::TcpSocket>();

    if (m_socket->connect(p_host, p_port) != sf::Socket::Done)
    {
        std::cerr << "VisualizerClient: Failed to connect to " << p_host << ":"
                  << p_port << std::endl;
        m_socket.reset();
        return false;
    }

    std::cout << "VisualizerClient: Connected to " << p_host << ":" << p_port
              << std::endl;

    m_tree_sent = false;
    m_last_states.clear();

    return true;
}

// ----------------------------------------------------------------------------
void VisualizerClient::disconnect()
{
    if (m_socket)
    {
        m_socket->disconnect();
        m_socket.reset();
        std::cout << "VisualizerClient: Disconnected" << std::endl;
    }

    m_tree_sent = false;
    m_last_states.clear();
}

// ----------------------------------------------------------------------------
bool VisualizerClient::isConnected() const
{
    return m_socket != nullptr;
}

// ----------------------------------------------------------------------------
bool VisualizerClient::send(std::string const& p_message)
{
    if (!m_socket)
    {
        return false;
    }

    sf::Socket::Status status =
        m_socket->send(p_message.data(), p_message.size());

    if (status != sf::Socket::Done)
    {
        std::cerr << "VisualizerClient: Send failed, disconnecting"
                  << std::endl;
        disconnect();
        return false;
    }

    return true;
}

// ----------------------------------------------------------------------------
std::string VisualizerClient::serializeTreeToYaml(Tree const& p_tree) const
{
    YamlExportVisitor visitor;
    p_tree.accept(visitor);
    return visitor.yaml.str();
}

// ----------------------------------------------------------------------------
void VisualizerClient::sendTree(Tree const& p_tree)
{
    if (!isConnected() || m_tree_sent)
    {
        return;
    }

    std::string yaml = serializeTreeToYaml(p_tree);
    std::string message = "YAML:" + yaml + "END_YAML\n";

    if (send(message))
    {
        m_tree_sent = true;
        std::cout << "VisualizerClient: Tree sent (" << yaml.size() << " bytes)"
                  << std::endl;
    }
}

// ****************************************************************************
//! \brief Visitor that collects all nodes for state change detection.
// ****************************************************************************
class NodeCollectorVisitor: public ConstBehaviorTreeVisitor
{
public:

    std::vector<Node const*> nodes;

    void collectNode(Node const& node)
    {
        nodes.push_back(&node);
    }

    void visitComposite(Composite const& p_node)
    {
        collectNode(p_node);
        for (auto const& child : p_node.getChildren())
        {
            child->accept(*this);
        }
    }

    void visitDecorator(Decorator const& p_node)
    {
        collectNode(p_node);
        if (p_node.hasChild())
        {
            p_node.getChild().accept(*this);
        }
    }

    void visitSequence(Sequence const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitReactiveSequence(ReactiveSequence const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitSequenceWithMemory(SequenceWithMemory const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitSelector(Selector const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitReactiveSelector(ReactiveSelector const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitSelectorWithMemory(SelectorWithMemory const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitParallel(Parallel const& p_node) override
    {
        visitComposite(p_node);
    }
    void visitParallelAll(ParallelAll const& p_node) override
    {
        visitComposite(p_node);
    }

    void visitInverter(Inverter const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitRepeat(Repeat const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitUntilSuccess(UntilSuccess const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitUntilFailure(UntilFailure const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitForceSuccess(ForceSuccess const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitForceFailure(ForceFailure const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitTimeout(Timeout const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitDelay(Delay const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitCooldown(Cooldown const& p_node) override
    {
        visitDecorator(p_node);
    }
    void visitRunOnce(RunOnce const& p_node) override
    {
        visitDecorator(p_node);
    }

    void visitSuccess(Success const& p_node) override
    {
        collectNode(p_node);
    }
    void visitFailure(Failure const& p_node) override
    {
        collectNode(p_node);
    }
    void visitCondition(Condition const& p_node) override
    {
        collectNode(p_node);
    }
    void visitAction(Action const& p_node) override
    {
        collectNode(p_node);
    }
    void visitSugarAction(SugarAction const& p_node) override
    {
        collectNode(p_node);
    }
    void visitSubTree(SubTreeNode const& p_node) override
    {
        collectNode(p_node);
        // Also collect nodes from the subtree content
        if (p_node.handle())
        {
            Tree const& subtree = p_node.handle()->tree();
            if (subtree.hasRoot())
            {
                subtree.getRoot().accept(*this);
            }
        }
    }
    void visitWait(Wait const& p_node) override
    {
        collectNode(p_node);
    }
    void visitSetBlackboard(SetBlackboard const& p_node) override
    {
        collectNode(p_node);
    }

    void visitTree(Tree const& tree) override
    {
        if (tree.hasRoot())
        {
            tree.getRoot().accept(*this);
        }
    }
};

// ----------------------------------------------------------------------------
void VisualizerClient::sendStateChanges(Tree const& p_tree)
{
    if (!isConnected())
    {
        return;
    }

    // Send tree first if not already done
    if (!m_tree_sent)
    {
        sendTree(p_tree);
    }

    // Collect all nodes
    NodeCollectorVisitor collector;
    p_tree.accept(collector);

    // Build delta message using node IDs
    std::string message;
    bool has_changes = false;

    for (Node const* node : collector.nodes)
    {
        uint32_t node_id = node->id();
        auto current = int(node->status());

        // Check if state changed
        auto it = m_last_states.find(node_id);
        bool changed = (it == m_last_states.end()) || (it->second != current);

        if (changed)
        {
            if (has_changes)
            {
                message += ",";
            }
            message += std::to_string(node_id) + ":" + std::to_string(current);
            has_changes = true;

            // Update cache
            m_last_states[node_id] = current;
        }
    }

    // Send only if there are changes
    if (has_changes)
    {
        send("S:" + message + "\n");
    }
}

} // namespace bt
