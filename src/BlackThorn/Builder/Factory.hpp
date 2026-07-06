/**
 * @file NodeFactory.hpp
 * @brief Factory class for creating behavior tree nodes.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BlackThorn/Nodes/Node.hpp"
#include "BlackThorn/Nodes/Leaves/Callback.hpp"
#include "BlackThorn/Nodes/Leaves/Condition.hpp"

#include <functional>
#include <unordered_map>

namespace bt {

// ****************************************************************************
//! \brief Factory class for creating behavior tree nodes.
//!
//! NodeFactory maps YAML node names to C++ node types. Register your custom
//! classes, lambdas, or conditions, then pass the factory to Builder so it can
//! instantiate nodes referenced in YAML.
//!
//! Key features:
//! - Register a node class by type (\c registerNode<T>).
//! - Register actions and conditions from lambdas.
//! - Optional blackboard injection for stateful nodes.
//! - Lookup by name (\c createNode, \c hasNode).
//!
//! Usage example:
//! \code
//!   bt::NodeFactory factory;
//!   auto bb = std::make_shared<bt::Blackboard>();
//!
//!   // 1. Register a custom node class
//!   factory.registerNode<PatrolAction>("Patrol", bb);
//!
//!   // 2. Register a quick action from a lambda
//!   factory.registerAction("OpenDoor", []() {
//!       std::cout << "Door opened\n";
//!       return bt::Status::SUCCESS;
//!   });
//!
//!   // 3. Register a condition with blackboard access
//!   factory.registerCondition(
//!       "HasAmmo",
//!       [bb]() {
//!           return bb->getOrDefault<int>("ammo", 0) > 0
//!                      ? bt::Status::SUCCESS
//!                      : bt::Status::FAILURE;
//!       },
//!       bb);
//!
//!   // 4. Build a tree from YAML using registered names
//!   std::string yaml = R"(
//!   BehaviorTree:
//!     Sequence:
//!       children:
//!         - Condition:
//!             name: HasAmmo
//!         - Action:
//!             name: Patrol
//!         - Action:
//!             name: OpenDoor
//!   )";
//!   auto result = bt::Builder::fromText(factory, yaml, bb);
//! \endcode
// ****************************************************************************
class NodeFactory
{
public:

    // ------------------------------------------------------------------------
    //! \brief Function type for creating nodes.
    // ------------------------------------------------------------------------
    using NodeCreator = std::function<std::unique_ptr<Node>()>;

    // ------------------------------------------------------------------------
    //! \brief Default destructor.
    // ------------------------------------------------------------------------
    virtual ~NodeFactory() = default;

    // ------------------------------------------------------------------------
    //! \brief Helper template method to register a node without blackboard.
    //! \tparam T The node type to register.
    //! \param[in] p_name Name used to identify this node type.
    // ------------------------------------------------------------------------
    template <typename T>
    void registerNode(std::string const& p_name)
    {
        registerNode(p_name, []() { return std::make_unique<T>(); });
    }

    // ------------------------------------------------------------------------
    //! \brief Helper template method to register a node with blackboard.
    //! \tparam T The node type to register.
    //! \param[in] p_name Name used to identify this node type.
    //! \param[in] p_blackboard The blackboard to use.
    // ------------------------------------------------------------------------
    template <typename T>
    void registerNode(std::string const& p_name, Blackboard::Ptr p_blackboard)
    {
        registerNode(
            p_name,
            [p_blackboard]() { return std::make_unique<T>(p_blackboard); });
    }

    // ------------------------------------------------------------------------
    //! \brief Register a node type with a creation function.
    //! \param[in] p_name Name used to identify this node type.
    //! \param[in] p_creator Function that creates instances of this node type.
    // ------------------------------------------------------------------------
    void registerNode(std::string const& p_name, NodeCreator p_creator)
    {
        m_creators[p_name] = std::move(p_creator);
    }

    // ------------------------------------------------------------------------
    //! \brief Create a node instance by name.
    //! \param[in] p_name The registered name of the node type to create.
    //! \return Unique pointer to the new node instance, or nullptr if name not
    //! found.
    // ------------------------------------------------------------------------
    std::unique_ptr<Node> createNode(std::string const& p_name) const
    {
        if (auto it = m_creators.find(p_name); it != m_creators.end())
        {
            return it->second();
        }
        return nullptr;
    }

    // ------------------------------------------------------------------------
    //! \brief Check if a node type is registered.
    //! \param[in] p_name The name to check.
    //! \return True if the node type is registered.
    // ------------------------------------------------------------------------
    [[nodiscard]] bool hasNode(std::string const& p_name) const
    {
        return m_creators.find(p_name) != m_creators.end();
    }

    // ------------------------------------------------------------------------
    //! \brief Helper method to register an action with a lambda.
    //! \param[in] p_name Name used to identify this action.
    //! \param[in] p_func Lambda function implementing the action.
    // ------------------------------------------------------------------------
    void registerAction(std::string const& p_name,
                        CallbackLeaf::Function&& p_func)
    {
        registerNode(p_name, [func = std::move(p_func)]() {
            return std::make_unique<CallbackLeaf>(func);
        });
    }

    // ------------------------------------------------------------------------
    //! \brief Helper method to register an action with a lambda and blackboard.
    //! \param[in] name Name used to identify this action.
    //! \param[in] p_func Lambda function implementing the action.
    //! \param[in] p_blackboard The blackboard to use.
    // ------------------------------------------------------------------------
    void registerAction(std::string const& p_name,
                        CallbackLeaf::Function&& p_func,
                        Blackboard::Ptr p_blackboard)
    {
        registerNode(p_name, [func = std::move(p_func), p_blackboard]() {
            return std::make_unique<CallbackLeaf>(func, p_blackboard);
        });
    }

    // ------------------------------------------------------------------------
    //! \brief Helper method to register a condition with a lambda.
    //! \param[in] p_name Name used to identify this condition.
    //! \param[in] p_func Lambda function implementing the condition.
    // ------------------------------------------------------------------------
    void registerCondition(std::string const& p_name,
                           Condition::Function&& p_func)
    {
        registerNode(p_name, [func = std::move(p_func)]() {
            return std::make_unique<Condition>(func);
        });
    }

    // ------------------------------------------------------------------------
    //! \brief Helper method to register a condition with a lambda and
    //! blackboard.
    //! \param[in] p_name Name used to identify this condition.
    //! \param[in] p_func Lambda function implementing the condition.
    //! \param[in] p_blackboard The blackboard to use.
    // ------------------------------------------------------------------------
    void registerCondition(std::string const& p_name,
                           Condition::Function&& p_func,
                           Blackboard::Ptr p_blackboard)
    {
        registerNode(p_name, [func = std::move(p_func), p_blackboard]() {
            return std::make_unique<Condition>(func, p_blackboard);
        });
    }

private:

    //! \brief Map of node names to their creation functions
    std::unordered_map<std::string, NodeCreator> m_creators;
};

} // namespace bt
