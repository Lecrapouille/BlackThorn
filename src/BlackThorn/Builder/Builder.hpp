/**
 * @file Builder.hpp
 * @brief Builder class for creating behavior trees from YAML.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/Robotik
 */

#pragma once

#include "BlackThorn/Builder/Factory.hpp"
#include "BlackThorn/Common/Return.hpp"
#include "BlackThorn/Nodes/Tree.hpp"

namespace YAML {
class Node;
}

namespace bt {

struct SubTreeRegistry;

// ****************************************************************************
//! \brief Builder class for creating behavior trees from YAML.
//!
//! The builder is a utility class that is used to create behavior trees from
//! a YAML file or a YAML text string:
//!   - Create a behavior tree from a YAML file or a YAML text.
//!   - Parse a YAML node into a behavior tree node.
//!   - Support for builtin nodes (Sequence, Selector, Success, SubTree, ...).
//!   - Support for custom nodes via NodeFactory.
//!   - Support for blackboard population and \c ${key} resolution.
//!   - Support for reusable subtrees (\c SubTrees section).
//!
//! Usage example 1: Builtin nodes only (from YAML text)
//! \code
//!   bt::NodeFactory factory;
//!   std::string yaml = R"(
//!   BehaviorTree:
//!     Sequence:
//!       children:
//!         - Success:
//!             name: OpenDoor
//!         - Success:
//!             name: Walk
//!   )";
//!
//!   auto result = bt::Builder::fromText(factory, yaml);
//!   if (result.isSuccess()) {
//!       auto tree = result.moveValue();
//!       tree->tick();
//!   }
//! \endcode
//!
//! Usage example 2: Custom nodes + blackboard (from YAML file)
//! \code
//!   auto bb = std::make_shared<bt::Blackboard>();
//!   factory.registerNode<PatrolAction>("Patrol", bb);
//!   factory.registerNode<AttackAction>("Attack", bb);
//!
//!   auto loaded = bt::Builder::fromFile(factory, "Patrol.yaml", bb);
//!   if (loaded.isSuccess()) {
//!       auto tree = loaded.moveValue();
//!       tree->setBlackboard(bb);
//!       tree->tick();
//!   }
//! \endcode
// ****************************************************************************
class Builder
{
public:

    // --------------------------------------------------------------------------
    //! \brief Create a behavior tree from a YAML file.
    //! \param[in] p_factory The factory to create custom nodes.
    //! \param[in] p_file_path The path to the YAML file.
    //! \param[in] p_blackboard Optional blackboard to populate from YAML.
    //! \return Return object containing the tree or an error message.
    // --------------------------------------------------------------------------
    static robotik::Return<Tree::Ptr>
    fromFile(NodeFactory const& p_factory,
             std::string const& p_file_path,
             Blackboard::Ptr p_blackboard = nullptr);

    // --------------------------------------------------------------------------
    //! \brief Create a behavior tree from YAML text.
    //! \param[in] p_factory The factory to create custom nodes.
    //! \param[in] p_yaml_text The YAML text describing the tree.
    //! \param[in] p_blackboard Optional blackboard to populate from YAML.
    //! \return Return object containing the tree or an error message.
    // --------------------------------------------------------------------------
    static robotik::Return<Tree::Ptr>
    fromText(NodeFactory const& p_factory,
             std::string const& p_yaml_text,
             Blackboard::Ptr p_blackboard = nullptr);

    // --------------------------------------------------------------------------
    //! \brief Parse a YAML node into a behavior tree, returning the root index.
    //! \param[in,out] p_tree The tree to populate with parsed nodes.
    //! \param[in] p_factory The factory to create custom nodes.
    //! \param[in] p_node The YAML node to parse.
    //! \param[in] p_blackboard Optional blackboard for parameter resolution.
    //! \param[in] p_subtrees Optional reusable subtree definitions.
    //! \return Return object containing the root node index or an error message.
    // --------------------------------------------------------------------------
    static robotik::Return<uint32_t>
    parseYAMLNode(Tree& p_tree,
                  NodeFactory const& p_factory,
                  YAML::Node const& p_node,
                  Blackboard::Ptr p_blackboard = nullptr,
                  SubTreeRegistry const* p_subtrees = nullptr);
};

} // namespace bt
