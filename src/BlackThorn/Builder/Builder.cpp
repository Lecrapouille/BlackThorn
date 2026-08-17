/**
 * @file Builder.cpp
 * @brief Builder class for creating behavior trees from YAML.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#include "BlackThorn/Builder/Builder.hpp"
#include "BlackThorn/BlackThorn.hpp"
#include "BlackThorn/Blackboard/Resolver.hpp"
#include "BlackThorn/Blackboard/Serializer.hpp"

#include <functional>
#include <unordered_map>

namespace bt {

// ****************************************************************************
//! \brief Context structure for parsing, containing factory and blackboard
// ****************************************************************************
struct ParsingContext
{
    NodeFactory const& factory;
    Blackboard::Ptr blackboard;
    SubTreeRegistry const* subtrees = nullptr;
    Tree* tree = nullptr;
    BuilderOptions options{};
};

// ----------------------------------------------------------------------------
//! \brief Assign visualizer ID from YAML _id field or auto-generate
// ----------------------------------------------------------------------------
static void assignNodeId(std::size_t p_node_index,
                         ParsingContext const& p_context,
                         YamlNode const& p_content)
{
    if (!p_context.options.assignVisualizerIds)
    {
        return;
    }

    auto const viz_index = static_cast<uint32_t>(p_node_index);

    if (p_content.hasKey("_id"))
    {
        if (auto id = p_content.child("_id").asSize())
        {
            p_context.tree->metadata().setVisualizerId(
                viz_index, static_cast<uint32_t>(*id));
        }
        else
        {
            p_context.tree->metadata().assignVisualizerId(viz_index);
        }
    }
    else
    {
        p_context.tree->metadata().assignVisualizerId(viz_index);
    }
}

using NodeCreatorMap =
    std::unordered_map<std::string,
                       std::function<Return<
                           uint32_t>(ParsingContext const&, YamlNode const&)>>;

static Return<uint32_t>
parseYAMLNodeInternal(ParsingContext const& p_context, YamlNode const& p_node);

// ----------------------------------------------------------------------------
//! \brief Count YAML nodes for pool pre-allocation.
// ----------------------------------------------------------------------------
static std::size_t countYamlNodes(YamlNode const& p_node)
{
    if (!p_node.valid() || !p_node.isMap())
    {
        return 0;
    }

    auto const [type, content] = p_node.typeEntry();
    if (type.empty())
    {
        return 0;
    }

    size_t count = 1;

    auto countField = [&](std::string_view p_field) {
        if (!content.hasKey(p_field))
        {
            return;
        }
        YamlNode const field = content.child(p_field);
        if (!field.isSeq())
        {
            return;
        }
        field.forEachSeq(
            [&](YamlNode const& p_child) { count += countYamlNodes(p_child); });
    };

    countField("children");
    countField("child");

    return count;
}

// ----------------------------------------------------------------------------
//! \brief Get the name of a node from YAML content
// ----------------------------------------------------------------------------
static std::string getNodeName(YamlNode const& p_content)
{
    if (p_content.hasKey("name"))
    {
        return p_content.child("name").scalar();
    }

    std::string firstKey;
    p_content.forEachMap([&](std::string_view p_key, YamlNode) {
        if (firstKey.empty())
        {
            firstKey = std::string(p_key);
        }
    });
    return firstKey;
}

// ----------------------------------------------------------------------------
//! \brief Extract port remapping from YAML parameters section.
//! Converts YAML parameters to a map of port name -> blackboard key.
// ----------------------------------------------------------------------------
static std::unordered_map<std::string, std::string>
extractPortRemapping(YamlNode const& p_parameters)
{
    std::unordered_map<std::string, std::string> remapping;
    if (!p_parameters.isMap())
    {
        return remapping;
    }

    p_parameters.forEachMap([&](std::string_view p_key, YamlNode p_value) {
        remapping[std::string(p_key)] = p_value.scalar();
    });
    return remapping;
}

// ----------------------------------------------------------------------------
//! \brief Load only literal parameters into blackboard.
//! Skips ${...} references which are only used for port remapping.
// ----------------------------------------------------------------------------
static void loadLiteralParameters(Blackboard& p_bb,
                                  YamlNode const& p_parameters)
{
    if (!p_parameters.isMap())
    {
        return;
    }

    p_parameters.forEachMap([&](std::string_view p_key, YamlNode p_value) {
        if (p_value.isScalar() && isBlackboardReference(p_value.scalar()))
        {
            return;
        }

        p_bb.setRaw(std::string(p_key),
                    BlackboardSerializer::valueFromNode(p_value, &p_bb));
    });
}

// ----------------------------------------------------------------------------
//! \brief Parse children nodes from YAML content
// ----------------------------------------------------------------------------
static Return<std::vector<uint32_t>>
parseChildren(ParsingContext const& p_context,
              YamlNode const& p_content,
              std::string const& p_field_name)
{
    if (!p_content.hasKey(p_field_name))
    {
        return Return<std::vector<uint32_t>>::error(
            "Node '" + getNodeName(p_content) + "' missing '" + p_field_name +
            "' field");
    }

    YamlNode const field = p_content.child(p_field_name);
    if (!field.isSeq())
    {
        return Return<std::vector<uint32_t>>::error(
            "Node '" + getNodeName(p_content) + "': '" + p_field_name +
            "' field must be a sequence");
    }

    if (field.size() == 0)
    {
        return Return<std::vector<uint32_t>>::error(
            "Node '" + getNodeName(p_content) +
            "' must have at least one child");
    }

    std::vector<uint32_t> children;
    for (std::size_t i = 0; i < field.size(); ++i)
    {
        auto result = parseYAMLNodeInternal(p_context, field.child(i));
        if (!result)
        {
            return Return<std::vector<uint32_t>>::error(
                result.getError());
        }
        children.push_back(result.getValue());
    }
    return Return<std::vector<uint32_t>>::success(std::move(children));
}

// ----------------------------------------------------------------------------
//! \brief Static creator functions for each node type
// ----------------------------------------------------------------------------
static Return<uint32_t> createSequence(ParsingContext const& p_context,
                                                YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Sequence>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "children");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    for (uint32_t child_idx : children.getValue())
    {
        node.addChildIndex(child_idx);
    }
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a selector node
// ----------------------------------------------------------------------------
static Return<uint32_t> createSelector(ParsingContext const& p_context,
                                                YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Selector>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "children");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    for (uint32_t child_idx : children.getValue())
    {
        node.addChildIndex(child_idx);
    }
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a parallel node
// ----------------------------------------------------------------------------
static Return<uint32_t> createParallel(ParsingContext const& p_context,
                                                YamlNode const& p_content)
{
    bool has_policies =
        p_content.hasKey("success_on_all") || p_content.hasKey("fail_on_all");
    bool has_thresholds = p_content.hasKey("success_threshold") ||
                          p_content.hasKey("failure_threshold");

    if (has_policies && has_thresholds)
    {
        return Return<uint32_t>::error(
            "Cannot specify both policies and thresholds");
    }
    if (!has_policies && !has_thresholds)
    {
        return Return<uint32_t>::error(
            "Missing policies or thresholds");
    }

    Composite* composite = nullptr;
    if (has_policies)
    {
        bool success_on_all =
            p_content.hasKey("success_on_all")
                ? p_content.child("success_on_all").asBool().value_or(true)
                : true;
        bool fail_on_all =
            p_content.hasKey("fail_on_all")
                ? p_content.child("fail_on_all").asBool().value_or(true)
                : true;
        auto& par = p_context.tree->emplaceNode<ParallelAll>(success_on_all,
                                                             fail_on_all);
        composite = &par;
    }
    else
    {
        size_t success_threshold =
            p_content.hasKey("success_threshold")
                ? p_content.child("success_threshold").asSize().value_or(1)
                : 1;
        size_t failure_threshold =
            p_content.hasKey("failure_threshold")
                ? p_content.child("failure_threshold").asSize().value_or(1)
                : 1;
        auto& par = p_context.tree->emplaceNode<Parallel>(success_threshold,
                                                          failure_threshold);
        composite = &par;
    }

    composite->name = getNodeName(p_content);
    assignNodeId(composite->index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "children");
    if (!children)
        return Return<uint32_t>::error(children.getError());

    for (uint32_t child_idx : children.getValue())
    {
        composite->addChildIndex(child_idx);
    }

    return Return<uint32_t>::success(
        static_cast<uint32_t>(composite->index()));
}

// ----------------------------------------------------------------------------
//! \brief Create an inverter node
// ----------------------------------------------------------------------------
static Return<uint32_t> createInverter(ParsingContext const& p_context,
                                                YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Inverter>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a repeater node
// ----------------------------------------------------------------------------
static Return<uint32_t> createRepeater(ParsingContext const& p_context,
                                                YamlNode const& p_content)
{
    size_t times = p_content.hasKey("times")
                       ? p_content.child("times").asSize().value_or(0)
                       : 0;
    auto& node = p_context.tree->emplaceNode<Repeater>(times);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);

    node.setBlackboard(p_context.blackboard);

    if (p_content.hasKey("parameters"))
    {
        node.setResolvedPorts(resolvePortRemapping(
            extractPortRemapping(p_content.child("parameters"))));
    }

    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a repeat until success node
// ----------------------------------------------------------------------------
static Return<uint32_t>
createRepeatUntilSuccess(ParsingContext const& p_context,
                         YamlNode const& p_content)
{
    size_t attempts = p_content.hasKey("attempts")
                          ? p_content.child("attempts").asSize().value_or(0)
                          : 0;
    auto& node = p_context.tree->emplaceNode<UntilSuccess>(attempts);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a repeat until failure node
// ----------------------------------------------------------------------------
static Return<uint32_t>
createRepeatUntilFailure(ParsingContext const& p_context,
                         YamlNode const& p_content)
{
    size_t attempts = p_content.hasKey("attempts")
                          ? p_content.child("attempts").asSize().value_or(0)
                          : 0;
    auto& node = p_context.tree->emplaceNode<UntilFailure>(attempts);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a force success node
// ----------------------------------------------------------------------------
static Return<uint32_t>
createForceSuccess(ParsingContext const& p_context, YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<ForceSuccess>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a force failure node
// ----------------------------------------------------------------------------
static Return<uint32_t>
createForceFailure(ParsingContext const& p_context, YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<ForceFailure>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create an action node
// ----------------------------------------------------------------------------
static Return<uint32_t> createAction(ParsingContext const& p_context,
                                              YamlNode const& p_content)
{
    if (!p_content.hasKey("name"))
    {
        return Return<uint32_t>::error(getNodeName(p_content) +
                                                " node missing 'name' field");
    }

    std::string name = p_content.child("name").scalar();

    auto node_ptr = p_context.factory.createNode(name);
    if (!node_ptr)
    {
        return Return<uint32_t>::error(
            "Failed to create " + getNodeName(p_content) + " node: " + name);
    }

    std::size_t node_index = p_context.tree->adoptNode(std::move(node_ptr));
    Node& node = p_context.tree->node(node_index);

    node.setBlackboard(p_context.blackboard);

    if (p_context.blackboard && p_content.hasKey("parameters"))
    {
        YamlNode const parameters = p_content.child("parameters");
        loadLiteralParameters(*p_context.blackboard, parameters);
        node.setResolvedPorts(
            resolvePortRemapping(extractPortRemapping(parameters)));
    }

    node.name = name;
    assignNodeId(node_index, p_context, p_content);
    return Return<uint32_t>::success(static_cast<uint32_t>(node_index));
}

// ----------------------------------------------------------------------------
//! \brief Create a condition node
// ----------------------------------------------------------------------------
static Return<uint32_t>
createCondition(ParsingContext const& p_context, YamlNode const& p_content)
{
    return createAction(p_context, p_content);
}

// ----------------------------------------------------------------------------
//! \brief Create a success node
// ----------------------------------------------------------------------------
static Return<uint32_t> createSuccess(ParsingContext const& p_context,
                                               YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Success>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a failure node
// ----------------------------------------------------------------------------
static Return<uint32_t> createFailure(ParsingContext const& p_context,
                                               YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Failure>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Apply port remapping from parent to child blackboard.
//! For inputs: copies value from parent BB to child BB under remapped key.
//! For outputs: stores remapping info for later propagation.
// ----------------------------------------------------------------------------
static void applySubTreePortRemapping(
    YamlNode const& p_parameters,
    Blackboard::Ptr const& p_parentBB,
    Blackboard::Ptr const& p_childBB,
    std::unordered_map<std::string, std::string>& p_outputRemapping)
{
    if (!p_parameters.isMap())
    {
        return;
    }

    p_parameters.forEachMap([&](std::string_view p_key, YamlNode p_value) {
        std::string childKey(p_key);
        std::string value = p_value.scalar();

        if (auto parentKey = extractBlackboardReference(value))
        {
            if (auto raw = p_parentBB->raw(*parentKey); raw)
            {
                p_childBB->setRaw(childKey, *raw);
            }
            else
            {
                p_outputRemapping[childKey] = *parentKey;
            }
        }
        else
        {
            p_childBB->set(childKey, value);
        }
    });
}

// ----------------------------------------------------------------------------
//! \brief Build a nested subtree tree from a YAML definition node.
// ----------------------------------------------------------------------------
static Return<Tree::Ptr>
buildSubTreeTree(ParsingContext const& p_context,
                 std::string const& p_reference,
                 YamlNode const& p_definition,
                 YamlNode const& p_parameters)
{
    auto subtree = Tree::create();

    ParsingContext nested = p_context;
    nested.tree = subtree.get();
    if (p_context.blackboard)
    {
        nested.blackboard = p_context.blackboard->createChild();
    }
    else
    {
        nested.blackboard = std::make_shared<Blackboard>();
    }

    std::unordered_map<std::string, std::string> outputRemapping;
    std::unordered_map<std::string, std::string> allRemapping;
    if (p_parameters.valid() && p_context.blackboard)
    {
        applySubTreePortRemapping(p_parameters,
                                  p_context.blackboard,
                                  nested.blackboard,
                                  outputRemapping);
        allRemapping = extractPortRemapping(p_parameters);
    }

    if (!allRemapping.empty())
    {
        nested.blackboard->setPortRemapping(allRemapping);
    }

    auto subtreeRoot = parseYAMLNodeInternal(nested, p_definition);
    if (!subtreeRoot)
    {
        return Return<Tree::Ptr>::error(
            "Failed to instantiate subtree '" + p_reference +
            "': " + subtreeRoot.getError());
    }

    subtree->setBlackboard(nested.blackboard);
    subtree->setRootIndex(subtreeRoot.getValue());

    if (!outputRemapping.empty())
    {
        subtree->setOutputRemapping(outputRemapping);
        subtree->setParentBlackboard(p_context.blackboard);
    }

    return Return<Tree::Ptr>::success(std::move(subtree));
}

// ----------------------------------------------------------------------------
//! \brief Create a subtree node referencing another behavior tree
// ----------------------------------------------------------------------------
static Return<uint32_t> createSubTree(ParsingContext const& p_context,
                                               YamlNode const& p_content)
{
    if (!p_context.subtrees)
    {
        return Return<uint32_t>::error(
            "SubTree node encountered but no 'SubTrees' section was provided");
    }

    if (!p_content.hasKey("reference"))
    {
        return Return<uint32_t>::error(
            "SubTree node missing 'reference' field");
    }

    std::string const reference = p_content.child("reference").scalar();
    auto it = p_context.subtrees->definitions.find(reference);
    if (it == p_context.subtrees->definitions.end())
    {
        return Return<uint32_t>::error("Unknown subtree reference: " +
                                                reference);
    }

    YamlNode const parameters = p_content.hasKey("parameters")
                                    ? p_content.child("parameters")
                                    : YamlNode{};

    std::string nodeName =
        p_content.hasKey("name") ? p_content.child("name").scalar() : reference;

    if (p_context.options.lazySubTrees)
    {
        NodeFactory const& factory = p_context.factory;
        Blackboard::Ptr parentBB = p_context.blackboard;
        SubTreeRegistry const* subtrees = p_context.subtrees;
        BuilderOptions options = p_context.options;
        YamlNode definition = it->second;

        auto builder = [factory,
                        parentBB,
                        subtrees,
                        options,
                        definition,
                        parameters,
                        reference]() -> Tree::Ptr {
            ParsingContext ctx{factory, parentBB, subtrees, nullptr, options};
            auto result =
                buildSubTreeTree(ctx, reference, definition, parameters);
            if (!result)
            {
                return nullptr;
            }
            return result.moveValue();
        };

        auto& node = p_context.tree->emplaceNode<SubTreeNode>(
            reference, std::move(builder));
        node.name = nodeName;
        assignNodeId(node.index(), p_context, p_content);
        return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
    }

    auto subtreeResult =
        buildSubTreeTree(p_context, reference, it->second, parameters);
    if (!subtreeResult)
    {
        return Return<uint32_t>::error(subtreeResult.getError());
    }

    auto& node = p_context.tree->emplaceNode<SubTreeNode>(
        reference, subtreeResult.moveValue());
    node.name = nodeName;
    assignNodeId(node.index(), p_context, p_content);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a timeout decorator node
// ----------------------------------------------------------------------------
static Return<uint32_t> createTimeout(ParsingContext const& p_context,
                                               YamlNode const& p_content)
{
    size_t ms = p_content.hasKey("milliseconds")
                    ? p_content.child("milliseconds").asSize().value_or(1000)
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Timeout>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);

    node.setBlackboard(p_context.blackboard);

    if (p_content.hasKey("parameters"))
    {
        node.setResolvedPorts(resolvePortRemapping(
            extractPortRemapping(p_content.child("parameters"))));
    }

    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Timeout must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a delay decorator node
// ----------------------------------------------------------------------------
static Return<uint32_t> createDelay(ParsingContext const& p_context,
                                             YamlNode const& p_content)
{
    size_t ms = p_content.hasKey("milliseconds")
                    ? p_content.child("milliseconds").asSize().value_or(1000)
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Delay>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Delay must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a cooldown decorator node
// ----------------------------------------------------------------------------
static Return<uint32_t> createCooldown(ParsingContext const& p_context,
                                                YamlNode const& p_content)
{
    size_t ms = p_content.hasKey("milliseconds")
                    ? p_content.child("milliseconds").asSize().value_or(1000)
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Cooldown>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "Cooldown must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a run once decorator node
// ----------------------------------------------------------------------------
static Return<uint32_t> createRunOnce(ParsingContext const& p_context,
                                               YamlNode const& p_content)
{
    auto& node = p_context.tree->emplaceNode<RunOnce>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return Return<uint32_t>::error(
            "RunOnce must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a wait leaf node
// ----------------------------------------------------------------------------
static Return<uint32_t> createWait(ParsingContext const& p_context,
                                            YamlNode const& p_content)
{
    size_t ms = p_content.hasKey("milliseconds")
                    ? p_content.child("milliseconds").asSize().value_or(1000)
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Wait>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Create a set blackboard leaf node
// ----------------------------------------------------------------------------
static Return<uint32_t>
createSetBlackboard(ParsingContext const& p_context, YamlNode const& p_content)
{
    if (!p_content.hasKey("key"))
    {
        return Return<uint32_t>::error(
            "SetBlackboard node missing 'key' field");
    }
    if (!p_content.hasKey("value"))
    {
        return Return<uint32_t>::error(
            "SetBlackboard node missing 'value' field");
    }

    std::string key = p_content.child("key").scalar();
    std::string value = p_content.child("value").scalar();
    auto& node = p_context.tree->emplaceNode<SetBlackboard>(
        key, value, p_context.blackboard);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return Return<uint32_t>::success(static_cast<uint32_t>(node.index()));
}

// ----------------------------------------------------------------------------
//! \brief Get the node creators registry
// ----------------------------------------------------------------------------
static NodeCreatorMap& getNodeCreators()
{
    static NodeCreatorMap creators = {
        {Sequence::toString(), createSequence},
        {Selector::toString(), createSelector},
        {Parallel::toString(), createParallel},
        {Inverter::toString(), createInverter},
        {Repeater::toString(), createRepeater},
        {"Repeater", createRepeater},
        {UntilSuccess::toString(), createRepeatUntilSuccess},
        {UntilFailure::toString(), createRepeatUntilFailure},
        {ForceSuccess::toString(), createForceSuccess},
        {ForceFailure::toString(), createForceFailure},
        {Timeout::toString(), createTimeout},
        {Delay::toString(), createDelay},
        {Cooldown::toString(), createCooldown},
        {RunOnce::toString(), createRunOnce},
        {CallbackLeaf::toString(), createAction},
        {Condition::toString(), createCondition},
        {Success::toString(), createSuccess},
        {Failure::toString(), createFailure},
        {Wait::toString(), createWait},
        {SetBlackboard::toString(), createSetBlackboard},
        {SubTreeNode::toString(), createSubTree},
    };
    return creators;
}

// ----------------------------------------------------------------------------
//! \brief Internal helper to parse YAML node with context
// ----------------------------------------------------------------------------
static Return<uint32_t>
parseYAMLNodeInternal(ParsingContext const& p_context, YamlNode const& p_node)
{
    if (!p_node.isMap())
    {
        return Return<uint32_t>::error(
            "Invalid node format: must be a map");
    }

    auto const [type, content] = p_node.typeEntry();
    if (type.empty())
    {
        return Return<uint32_t>::error(
            "Empty YAML node: a node must contain at least one key defining "
            "its type (e.g. Sequence, Selector, Action)");
    }

    auto const& creators = getNodeCreators();
    auto fn_it = creators.find(type);
    if (fn_it == creators.end())
    {
        return Return<uint32_t>::error("Unknown node type: " + type);
    }

    return fn_it->second(p_context, content);
}

//-----------------------------------------------------------------------------
Return<Tree::Ptr> Builder::fromFile(NodeFactory const& p_factory,
                                             std::string const& p_file_path,
                                             Blackboard::Ptr p_blackboard,
                                             BuilderOptions p_options)
{
    auto docResult = TreeDocument::parseFile(p_file_path);
    if (!docResult)
    {
        return Return<Tree::Ptr>::error(docResult.getError());
    }

    return docResult.getValue()->instantiate(
        p_factory, p_blackboard, p_options);
}

//-----------------------------------------------------------------------------
Return<Tree::Ptr> Builder::fromText(NodeFactory const& p_factory,
                                             std::string const& p_yaml_text,
                                             Blackboard::Ptr p_blackboard,
                                             BuilderOptions p_options)
{
    auto docResult = TreeDocument::parseText(p_yaml_text);
    if (!docResult)
    {
        return Return<Tree::Ptr>::error(docResult.getError());
    }

    return docResult.getValue()->instantiate(
        p_factory, p_blackboard, p_options);
}

//-----------------------------------------------------------------------------
Return<uint32_t>
Builder::parseYAMLNode(Tree& p_tree,
                       NodeFactory const& p_factory,
                       YamlNode const& p_node,
                       Blackboard::Ptr p_blackboard,
                       SubTreeRegistry const* p_subtrees,
                       BuilderOptions p_options)
{
    if (p_options.reserveNodes)
    {
        p_tree.reserveNodes(countYamlNodes(p_node));
    }

    ParsingContext context{
        p_factory, p_blackboard, p_subtrees, &p_tree, p_options};
    return parseYAMLNodeInternal(context, p_node);
}

namespace {

Return<SubTreeRegistry> buildSubTreeRegistry(YamlNode const& p_root)
{
    SubTreeRegistry registry;

    if (!p_root.hasKey("SubTrees"))
    {
        return Return<SubTreeRegistry>::success(std::move(registry));
    }

    YamlNode const subtrees = p_root.child("SubTrees");
    if (!subtrees.isMap())
    {
        return Return<SubTreeRegistry>::error(
            "'SubTrees' section must be a map of name -> node definitions");
    }

    subtrees.forEachMap([&](std::string_view p_name, YamlNode p_def) {
        registry.definitions.emplace(std::string(p_name), p_def);
    });

    return Return<SubTreeRegistry>::success(std::move(registry));
}

} // namespace

Return<std::shared_ptr<TreeDocument>>
TreeDocument::parseFile(std::string const& p_path)
{
    auto yamlResult = YamlDocument::parseFile(p_path);
    if (!yamlResult)
    {
        return Return<std::shared_ptr<TreeDocument>>::error(
            yamlResult.getError());
    }

    auto doc = std::make_shared<TreeDocument>();
    doc->m_yaml = yamlResult.moveValue();

    auto registryResult = buildSubTreeRegistry(doc->m_yaml.root());
    if (!registryResult)
    {
        return Return<std::shared_ptr<TreeDocument>>::error(
            registryResult.getError());
    }

    doc->m_subtrees = registryResult.moveValue();
    doc->m_hasBlackboard = doc->m_yaml.hasKey("Blackboard");

    return Return<std::shared_ptr<TreeDocument>>::success(
        std::move(doc));
}

Return<std::shared_ptr<TreeDocument>>
TreeDocument::parseText(std::string p_text)
{
    auto yamlResult = YamlDocument::parseText(std::move(p_text));
    if (!yamlResult)
    {
        return Return<std::shared_ptr<TreeDocument>>::error(
            yamlResult.getError());
    }

    auto doc = std::make_shared<TreeDocument>();
    doc->m_yaml = yamlResult.moveValue();

    auto registryResult = buildSubTreeRegistry(doc->m_yaml.root());
    if (!registryResult)
    {
        return Return<std::shared_ptr<TreeDocument>>::error(
            registryResult.getError());
    }

    doc->m_subtrees = registryResult.moveValue();
    doc->m_hasBlackboard = doc->m_yaml.hasKey("Blackboard");

    return Return<std::shared_ptr<TreeDocument>>::success(
        std::move(doc));
}

Return<Tree::Ptr>
TreeDocument::instantiate(NodeFactory const& p_factory,
                          Blackboard::Ptr p_blackboard,
                          BuilderOptions p_options) const
{
    if (!m_yaml.hasKey("BehaviorTree"))
    {
        return Return<Tree::Ptr>::error(
            "Missing 'BehaviorTree' node in YAML document");
    }

    Blackboard::Ptr blackboard =
        p_blackboard ? p_blackboard : std::make_shared<Blackboard>();

    if (p_options.mergeBlackboard && m_hasBlackboard)
    {
        BlackboardSerializer::load(
            *blackboard, m_yaml.root().child("Blackboard"), blackboard.get());
    }

    auto tree = Tree::create();
    tree->setBlackboard(blackboard);

    YamlNode const behaviorTree = m_yaml.root().child("BehaviorTree");

    SubTreeRegistry const* registryPtr =
        m_subtrees.definitions.empty() ? nullptr : &m_subtrees;

    auto nodeResult = Builder::parseYAMLNode(
        *tree, p_factory, behaviorTree, blackboard, registryPtr, p_options);
    if (!nodeResult)
    {
        return Return<Tree::Ptr>::error(nodeResult.getError());
    }

    tree->setRootIndex(nodeResult.getValue());
    return Return<Tree::Ptr>::success(std::move(tree));
}

} // namespace bt
