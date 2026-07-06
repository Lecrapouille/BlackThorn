/**
 * @file Builder.cpp
 * @brief Builder class for creating behavior trees from YAML.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/Robotik
 */

#include "BlackThorn/Builder/Builder.hpp"
#include "BlackThorn/BlackThorn.hpp"

#include <functional>
#include <unordered_map>
#include <yaml-cpp/yaml.h>

namespace bt {

// ****************************************************************************
//! \brief Context structure for parsing, containing factory and blackboard
// ****************************************************************************
struct SubTreeRegistry
{
    std::unordered_map<std::string, YAML::Node> definitions;
};

struct ParsingContext
{
    NodeFactory const& factory;
    Blackboard::Ptr blackboard;
    SubTreeRegistry const* subtrees = nullptr;
    Tree* tree = nullptr;
};

// ----------------------------------------------------------------------------
//! \brief Assign visualizer ID from YAML _id field or auto-generate
// ----------------------------------------------------------------------------
static void assignNodeId(uint32_t p_node_index,
                         ParsingContext const& p_context,
                         YAML::Node const& p_content)
{
    if (p_content["_id"])
    {
        p_context.tree->metadata().setVisualizerId(
            p_node_index, p_content["_id"].as<uint32_t>());
    }
    else
    {
        p_context.tree->metadata().assignVisualizerId(p_node_index);
    }
}

using NodeCreatorMap = std::unordered_map<
    std::string,
    std::function<robotik::Return<uint32_t>(ParsingContext const&,
                                             YAML::Node const&)>>;

static robotik::Return<uint32_t>
parseYAMLNodeInternal(ParsingContext const& p_context,
                      YAML::Node const& p_node);

// ----------------------------------------------------------------------------
//! \brief Get the name of a node from YAML content
// ----------------------------------------------------------------------------
static std::string getNodeName(YAML::Node const& p_content)
{
    return p_content["name"] ? p_content["name"].as<std::string>()
                             : p_content.begin()->first.as<std::string>();
}

// ----------------------------------------------------------------------------
//! \brief Extract port remapping from YAML parameters section.
//! Converts YAML parameters to a map of port name -> blackboard key.
// ----------------------------------------------------------------------------
static std::unordered_map<std::string, std::string>
extractPortRemapping(YAML::Node const& p_parameters)
{
    std::unordered_map<std::string, std::string> remapping;
    if (p_parameters && p_parameters.IsMap())
    {
        for (auto const& param : p_parameters)
        {
            remapping[param.first.as<std::string>()] =
                param.second.as<std::string>();
        }
    }
    return remapping;
}

// ----------------------------------------------------------------------------
//! \brief Load only literal parameters into blackboard.
//! Skips ${...} references which are only used for port remapping.
// ----------------------------------------------------------------------------
static void loadLiteralParameters(Blackboard& p_bb,
                                  YAML::Node const& p_parameters)
{
    if (!p_parameters || !p_parameters.IsMap())
    {
        return;
    }

    std::regex refPattern(R"(\$\{([^}]+)\})");

    for (auto const& param : p_parameters)
    {
        auto const& valueNode = param.second;

        // Skip ${...} references - they're for port remapping only
        if (valueNode.IsScalar())
        {
            std::string value = valueNode.as<std::string>();
            if (std::regex_match(value, refPattern))
            {
                continue; // Skip reference
            }
        }

        // Load literal value into blackboard
        std::string key = param.first.as<std::string>();
        YAML::Node single;
        single[key] = valueNode;
        BlackboardSerializer::load(p_bb, single, &p_bb);
    }
}

// ----------------------------------------------------------------------------
//! \brief Parse children nodes from YAML content
// ----------------------------------------------------------------------------
static robotik::Return<std::vector<uint32_t>>
parseChildren(ParsingContext const& p_context,
              YAML::Node const& p_content,
              std::string const& p_field_name)
{
    if (!p_content[p_field_name])
    {
        return robotik::Return<std::vector<uint32_t>>::error(
            "Node '" + getNodeName(p_content) + "' missing '" + p_field_name +
            "' field");
    }

    if (!p_content[p_field_name].IsSequence())
    {
        return robotik::Return<std::vector<uint32_t>>::error(
            "Node '" + getNodeName(p_content) + "': '" + p_field_name +
            "' field must be a sequence");
    }

    if (p_content[p_field_name].size() == 0)
    {
        return robotik::Return<std::vector<uint32_t>>::error(
            "Node '" + getNodeName(p_content) +
            "' must have at least one child");
    }

    std::vector<uint32_t> children;
    for (auto const& child : p_content[p_field_name])
    {
        auto result = parseYAMLNodeInternal(p_context, child);
        if (!result)
        {
            return robotik::Return<std::vector<uint32_t>>::error(
                result.getError());
        }
        children.push_back(result.getValue());
    }
    return robotik::Return<std::vector<uint32_t>>::success(std::move(children));
}

// ----------------------------------------------------------------------------
//! \brief Build the registry of reusable subtrees if provided in YAML input.
// ----------------------------------------------------------------------------
static robotik::Return<SubTreeRegistry>
buildSubTreeRegistry(YAML::Node const& p_root)
{
    SubTreeRegistry registry;

    if (!p_root["SubTrees"])
    {
        return robotik::Return<SubTreeRegistry>::success(std::move(registry));
    }

    if (!p_root["SubTrees"].IsMap())
    {
        return robotik::Return<SubTreeRegistry>::error(
            "'SubTrees' section must be a map of name -> node definitions");
    }

    for (auto const& entry : p_root["SubTrees"])
    {
        registry.definitions.emplace(entry.first.as<std::string>(),
                                     entry.second);
    }

    return robotik::Return<SubTreeRegistry>::success(std::move(registry));
}

// ----------------------------------------------------------------------------
//! \brief Static creator functions for each node type
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createSequence(ParsingContext const& p_context, YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Sequence>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "children");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    for (uint32_t child_idx : children.getValue())
    {
        node.addChildIndex(child_idx);
    }
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a selector node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createSelector(ParsingContext const& p_context, YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Selector>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "children");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    for (uint32_t child_idx : children.getValue())
    {
        node.addChildIndex(child_idx);
    }
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a parallel node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createParallel(ParsingContext const& p_context, YAML::Node const& p_content)
{
    bool has_policies = p_content["success_on_all"] || p_content["fail_on_all"];
    bool has_thresholds =
        p_content["success_threshold"] || p_content["failure_threshold"];

    if (has_policies && has_thresholds)
    {
        return robotik::Return<uint32_t>::error(
            "Cannot specify both policies and thresholds");
    }
    if (!has_policies && !has_thresholds)
    {
        return robotik::Return<uint32_t>::error(
            "Missing policies or thresholds");
    }

    Composite* composite = nullptr;
    if (has_policies)
    {
        bool success_on_all = p_content["success_on_all"]
                                  ? p_content["success_on_all"].as<bool>()
                                  : true;
        bool fail_on_all = p_content["fail_on_all"]
                               ? p_content["fail_on_all"].as<bool>()
                               : true;
        auto& par =
            p_context.tree->emplaceNode<ParallelAll>(success_on_all, fail_on_all);
        composite = &par;
    }
    else
    {
        size_t success_threshold =
            p_content["success_threshold"]
                ? p_content["success_threshold"].as<size_t>()
                : 1;
        size_t failure_threshold =
            p_content["failure_threshold"]
                ? p_content["failure_threshold"].as<size_t>()
                : 1;
        auto& par = p_context.tree->emplaceNode<Parallel>(success_threshold,
                                                          failure_threshold);
        composite = &par;
    }

    composite->name = getNodeName(p_content);
    assignNodeId(composite->index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "children");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());

    for (uint32_t child_idx : children.getValue())
    {
        composite->addChildIndex(child_idx);
    }

    return robotik::Return<uint32_t>::success(composite->index());
}

// ----------------------------------------------------------------------------
//! \brief Create an inverter node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createInverter(ParsingContext const& p_context, YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Inverter>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a repeater node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createRepeater(ParsingContext const& p_context, YAML::Node const& p_content)
{
    size_t times = p_content["times"] ? p_content["times"].as<size_t>() : 0;
    auto& node = p_context.tree->emplaceNode<Repeater>(times);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);

    // Set blackboard for port access
    node.setBlackboard(p_context.blackboard);

    // Configure port remapping if parameters are present
    if (p_content["parameters"])
    {
        node.setPortRemapping(extractPortRemapping(p_content["parameters"]));
    }

    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a repeat until success node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createRepeatUntilSuccess(ParsingContext const& p_context,
                         YAML::Node const& p_content)
{
    size_t attempts =
        p_content["attempts"] ? p_content["attempts"].as<size_t>() : 0;
    auto& node = p_context.tree->emplaceNode<UntilSuccess>(attempts);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a repeat until failure node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createRepeatUntilFailure(ParsingContext const& p_context,
                         YAML::Node const& p_content)
{
    size_t attempts =
        p_content["attempts"] ? p_content["attempts"].as<size_t>() : 0;
    auto& node = p_context.tree->emplaceNode<UntilFailure>(attempts);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a force success node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createForceSuccess(ParsingContext const& p_context, YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<ForceSuccess>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a force failure node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createForceFailure(ParsingContext const& p_context, YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<ForceFailure>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Decorator must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create an action node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createAction(ParsingContext const& p_context,
                                              YAML::Node const& p_content)
{
    if (!p_content["name"])
    {
        return robotik::Return<uint32_t>::error(
            p_content.begin()->first.as<std::string>() +
            " node missing 'name' field");
    }

    std::string name = p_content["name"].as<std::string>();

    auto node_ptr = p_context.factory.createNode(name);
    if (!node_ptr)
    {
        return robotik::Return<uint32_t>::error(
            "Failed to create " + p_content.begin()->first.as<std::string>() +
            " node: " + name);
    }

    uint32_t node_index = p_context.tree->adoptNode(std::move(node_ptr));
    Node& node = p_context.tree->node(node_index);

    // Set the blackboard for the node (now on Node base class)
    node.setBlackboard(p_context.blackboard);

    // Handle local parameters if present
    if (p_context.blackboard && p_content["parameters"])
    {
        // Load only literal parameters into blackboard (not ${...} references)
        // References are only used for port remapping, not stored in BB
        loadLiteralParameters(*p_context.blackboard, p_content["parameters"]);

        // Configure port remapping for all parameters
        node.setPortRemapping(extractPortRemapping(p_content["parameters"]));
    }

    node.name = name;
    assignNodeId(node_index, p_context, p_content);
    return robotik::Return<uint32_t>::success(node_index);
}

// ----------------------------------------------------------------------------
//! \brief Create a condition node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createCondition(ParsingContext const& p_context, YAML::Node const& p_content)
{
    return createAction(p_context, p_content);
}

// ----------------------------------------------------------------------------
//! \brief Create a success node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createSuccess(ParsingContext const& p_context,
                                               YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Success>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a failure node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createFailure(ParsingContext const& p_context,
                                               YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<Failure>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Apply port remapping from parent to child blackboard.
//! For inputs: copies value from parent BB to child BB under remapped key.
//! For outputs: stores remapping info for later propagation.
// ----------------------------------------------------------------------------
static void applySubTreePortRemapping(
    YAML::Node const& p_parameters,
    Blackboard::Ptr const& p_parentBB,
    Blackboard::Ptr const& p_childBB,
    std::unordered_map<std::string, std::string>& p_outputRemapping)
{
    if (!p_parameters || !p_parameters.IsMap())
    {
        return;
    }

    std::regex pattern(R"(\$\{([^}]+)\})");

    for (auto const& param : p_parameters)
    {
        std::string childKey = param.first.as<std::string>();
        std::string value = param.second.as<std::string>();

        std::smatch match;
        if (std::regex_match(value, match, pattern))
        {
            // It's a reference ${parent_key}
            std::string parentKey = match[1].str();

            // Try to get value from parent blackboard and copy to child
            if (auto raw = p_parentBB->raw(parentKey); raw)
            {
                // Input: copy value from parent to child under childKey
                p_childBB->setRaw(childKey, *raw);
            }
            else
            {
                // Output: parent key doesn't exist yet, store remapping
                // The child will write to childKey, we need to propagate to
                // parentKey
                p_outputRemapping[childKey] = parentKey;
            }
        }
        else
        {
            // Literal value, set directly in child blackboard
            p_childBB->set(childKey, value);
        }
    }
}

// ----------------------------------------------------------------------------
//! \brief Create a subtree node referencing another behavior tree
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createSubTree(ParsingContext const& p_context,
                                               YAML::Node const& p_content)
{
    if (!p_context.subtrees)
    {
        return robotik::Return<uint32_t>::error(
            "SubTree node encountered but no 'SubTrees' section was provided");
    }

    if (!p_content["reference"])
    {
        return robotik::Return<uint32_t>::error(
            "SubTree node missing 'reference' field");
    }

    auto reference = p_content["reference"].as<std::string>();
    auto it = p_context.subtrees->definitions.find(reference);
    if (it == p_context.subtrees->definitions.end())
    {
        return robotik::Return<uint32_t>::error("Unknown subtree reference: " +
                                                reference);
    }

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

    // Apply port remapping from parameters
    std::unordered_map<std::string, std::string> outputRemapping;
    std::unordered_map<std::string, std::string> allRemapping;
    if (p_content["parameters"] && p_context.blackboard)
    {
        applySubTreePortRemapping(p_content["parameters"],
                                  p_context.blackboard,
                                  nested.blackboard,
                                  outputRemapping);

        // Extract all remappings for display
        allRemapping = extractPortRemapping(p_content["parameters"]);
    }

    // Store port remapping info in the child blackboard for dump()
    if (!allRemapping.empty())
    {
        nested.blackboard->setPortRemapping(allRemapping);
    }

    auto subtreeRoot = parseYAMLNodeInternal(nested, it->second);
    if (!subtreeRoot)
    {
        return robotik::Return<uint32_t>::error(
            "Failed to instantiate subtree '" + reference +
            "': " + subtreeRoot.getError());
    }

    subtree->setBlackboard(nested.blackboard);
    subtree->setRootIndex(subtreeRoot.getValue());

    // Store output remapping and parent blackboard for later propagation
    if (!outputRemapping.empty())
    {
        subtree->setOutputRemapping(outputRemapping);
        subtree->setParentBlackboard(p_context.blackboard);
    }

    auto& node =
        p_context.tree->emplaceNode<SubTreeNode>(reference, std::move(subtree));
    node.name =
        p_content["name"] ? p_content["name"].as<std::string>() : reference;
    assignNodeId(node.index(), p_context, p_content);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a timeout decorator node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createTimeout(ParsingContext const& p_context,
                                               YAML::Node const& p_content)
{
    size_t ms = p_content["milliseconds"]
                    ? p_content["milliseconds"].as<size_t>()
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Timeout>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);

    // Set blackboard for port access
    node.setBlackboard(p_context.blackboard);

    // Configure port remapping if parameters are present
    if (p_content["parameters"])
    {
        node.setPortRemapping(extractPortRemapping(p_content["parameters"]));
    }

    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Timeout must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a delay decorator node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createDelay(ParsingContext const& p_context,
                                             YAML::Node const& p_content)
{
    size_t ms = p_content["milliseconds"]
                    ? p_content["milliseconds"].as<size_t>()
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Delay>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Delay must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a cooldown decorator node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createCooldown(ParsingContext const& p_context, YAML::Node const& p_content)
{
    size_t ms = p_content["milliseconds"]
                    ? p_content["milliseconds"].as<size_t>()
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Cooldown>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "Cooldown must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a run once decorator node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createRunOnce(ParsingContext const& p_context,
                                               YAML::Node const& p_content)
{
    auto& node = p_context.tree->emplaceNode<RunOnce>();
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    auto children = parseChildren(p_context, p_content, "child");
    if (!children)
        return robotik::Return<uint32_t>::error(children.getError());
    if (children.getValue().size() != 1)
    {
        return robotik::Return<uint32_t>::error(
            "RunOnce must have exactly one child");
    }
    node.setChildIndex(children.getValue()[0]);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a wait leaf node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t> createWait(ParsingContext const& p_context,
                                            YAML::Node const& p_content)
{
    size_t ms = p_content["milliseconds"]
                    ? p_content["milliseconds"].as<size_t>()
                    : 1000;
    auto& node = p_context.tree->emplaceNode<Wait>(ms);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return robotik::Return<uint32_t>::success(node.index());
}

// ----------------------------------------------------------------------------
//! \brief Create a set blackboard leaf node
// ----------------------------------------------------------------------------
static robotik::Return<uint32_t>
createSetBlackboard(ParsingContext const& p_context,
                    YAML::Node const& p_content)
{
    if (!p_content["key"])
    {
        return robotik::Return<uint32_t>::error(
            "SetBlackboard node missing 'key' field");
    }
    if (!p_content["value"])
    {
        return robotik::Return<uint32_t>::error(
            "SetBlackboard node missing 'value' field");
    }

    std::string key = p_content["key"].as<std::string>();
    std::string value = p_content["value"].as<std::string>();
    auto& node =
        p_context.tree->emplaceNode<SetBlackboard>(key, value, p_context.blackboard);
    node.name = getNodeName(p_content);
    assignNodeId(node.index(), p_context, p_content);
    return robotik::Return<uint32_t>::success(node.index());
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
static robotik::Return<uint32_t>
parseYAMLNodeInternal(ParsingContext const& p_context, YAML::Node const& p_node)
{
    if (!p_node.IsMap())
    {
        return robotik::Return<uint32_t>::error(
            "Invalid node format: must be a map");
    }

    auto it = p_node.begin();
    if (it == p_node.end())
    {
        return robotik::Return<uint32_t>::error(
            "Empty YAML node: a node must contain at least one key defining "
            "its type (e.g. Sequence, Selector, Action)");
    }

    std::string type = it->first.as<std::string>();
    auto const& creators = getNodeCreators();
    auto fn_it = creators.find(type);
    if (fn_it == creators.end())
    {
        return robotik::Return<uint32_t>::error("Unknown node type: " + type);
    }

    return fn_it->second(p_context, it->second);
}

//-----------------------------------------------------------------------------
robotik::Return<Tree::Ptr> Builder::fromFile(NodeFactory const& p_factory,
                                             std::string const& p_file_path,
                                             Blackboard::Ptr p_blackboard)
{
    try
    {
        YAML::Node root = YAML::LoadFile(p_file_path);
        if (!root["BehaviorTree"])
        {
            return robotik::Return<Tree::Ptr>::error(
                "Missing 'BehaviorTree' node in YAML file");
        }

        Blackboard::Ptr blackboard =
            p_blackboard ? p_blackboard : std::make_shared<Blackboard>();

        if (root["Blackboard"])
        {
            BlackboardSerializer::load(
                *blackboard, root["Blackboard"], blackboard.get());
        }

        auto registryResult = buildSubTreeRegistry(root);
        if (!registryResult)
        {
            return robotik::Return<Tree::Ptr>::error(registryResult.getError());
        }
        auto registry = registryResult.moveValue();
        SubTreeRegistry const* registryPtr =
            registry.definitions.empty() ? nullptr : &registry;

        auto tree = Tree::create();
        tree->setBlackboard(blackboard);

        auto nodeResult = parseYAMLNode(
            *tree, p_factory, root["BehaviorTree"], blackboard, registryPtr);
        if (!nodeResult)
        {
            return robotik::Return<Tree::Ptr>::error(nodeResult.getError());
        }

        tree->setRootIndex(nodeResult.getValue());
        return robotik::Return<Tree::Ptr>::success(std::move(tree));
    }
    catch (const YAML::Exception& e)
    {
        return robotik::Return<Tree::Ptr>::error("YAML parsing error: " +
                                                 std::string(e.what()));
    }
    catch (const std::exception& e)
    {
        return robotik::Return<Tree::Ptr>::error("Error: " +
                                                 std::string(e.what()));
    }
}

//-----------------------------------------------------------------------------
robotik::Return<Tree::Ptr> Builder::fromText(NodeFactory const& p_factory,
                                             std::string const& p_yaml_text,
                                             Blackboard::Ptr p_blackboard)
{
    try
    {
        YAML::Node root = YAML::Load(p_yaml_text);
        if (!root["BehaviorTree"])
        {
            return robotik::Return<Tree::Ptr>::error(
                "Missing 'BehaviorTree' node in YAML text");
        }

        Blackboard::Ptr blackboard =
            p_blackboard ? p_blackboard : std::make_shared<Blackboard>();

        if (root["Blackboard"])
        {
            BlackboardSerializer::load(
                *blackboard, root["Blackboard"], blackboard.get());
        }

        auto registryResult = buildSubTreeRegistry(root);
        if (!registryResult)
        {
            return robotik::Return<Tree::Ptr>::error(registryResult.getError());
        }
        auto registry = registryResult.moveValue();
        SubTreeRegistry const* registryPtr =
            registry.definitions.empty() ? nullptr : &registry;

        auto tree = Tree::create();
        tree->setBlackboard(blackboard);

        auto nodeResult = parseYAMLNode(
            *tree, p_factory, root["BehaviorTree"], blackboard, registryPtr);
        if (!nodeResult)
        {
            return robotik::Return<Tree::Ptr>::error(nodeResult.getError());
        }

        tree->setRootIndex(nodeResult.getValue());
        return robotik::Return<Tree::Ptr>::success(std::move(tree));
    }
    catch (const YAML::Exception& e)
    {
        return robotik::Return<Tree::Ptr>::error("YAML parsing error: " +
                                                 std::string(e.what()));
    }
    catch (const std::exception& e)
    {
        return robotik::Return<Tree::Ptr>::error("Error: " +
                                                 std::string(e.what()));
    }
}

//-----------------------------------------------------------------------------
robotik::Return<uint32_t>
Builder::parseYAMLNode(Tree& p_tree,
                       NodeFactory const& p_factory,
                       YAML::Node const& p_node,
                       Blackboard::Ptr p_blackboard,
                       SubTreeRegistry const* p_subtrees)
{
    ParsingContext context{p_factory, p_blackboard, p_subtrees, &p_tree};
    return parseYAMLNodeInternal(context, p_node);
}

} // namespace bt
