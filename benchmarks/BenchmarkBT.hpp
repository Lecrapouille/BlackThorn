/**
 * @file BenchmarkBT.hpp
 * @brief BehaviorTree.CPP benchmark helpers.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#pragma once

#include "BenchmarkCommon.hpp"

#include "behaviortree_cpp/action_node.h"
#include "behaviortree_cpp/bt_factory.h"
#include "behaviortree_cpp/loggers/bt_observer.h"

namespace btcpp::benchmark {

inline std::filesystem::path xmlPath(std::string const& p_filename)
{
    return ::benchmark::benchmarksRoot() / "xml" / p_filename;
}

class ReadPorts final: public BT::SyncActionNode
{
public:

    ReadPorts(std::string const& p_name, BT::NodeConfig const& p_config)
        : SyncActionNode(p_name, p_config)
    {
    }

    BT::NodeStatus tick() override
    {
        (void)getInput<int>("a");
        (void)getInput<int>("b");
        (void)getInput<std::string>("label");
        return BT::NodeStatus::SUCCESS;
    }

    static BT::PortsList providedPorts()
    {
        return {BT::InputPort<int>("a"),
                BT::InputPort<int>("b"),
                BT::InputPort<std::string>("label")};
    }
};

inline BT::BehaviorTreeFactory makeFactory()
{
    BT::BehaviorTreeFactory factory;

    BT::NodeStatus const ok = BT::NodeStatus::SUCCESS;
    auto noop = [&ok](BT::TreeNode&) { return ok; };

    for (char const* name : {"LoadRoute",
                             "FollowWaypoints",
                             "AttemptNonLethal",
                             "NeutralizeThreat",
                             "ExtractTeam",
                             "LoadGameState",
                             "ChoosePrimaryEnemy"})
    {
        factory.registerSimpleAction(name, noop);
    }

    factory.registerNodeType<ReadPorts>("ReadPorts");

    return factory;
}

inline BT::Tree loadTreeFromXml(BT::BehaviorTreeFactory& p_factory,
                                std::filesystem::path const& p_path,
                                BT::Blackboard::Ptr p_blackboard = nullptr)
{
    if (!p_blackboard)
    {
        p_blackboard = BT::Blackboard::create();
    }
    return p_factory.createTreeFromFile(p_path, p_blackboard);
}

} // namespace btcpp::benchmark
