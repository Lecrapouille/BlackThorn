/**
 * @file SubTreeRemapping.cpp
 * @brief Example demonstrating SubTree port remapping.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "BlackThorn/BlackThorn.hpp"
#include "BlackThorn/Builder/Builder.hpp"

#include <iostream>

// Test action that reads a goal from port
class MoveBase: public bt::Action
{
public:

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<std::string>("goal");
        return ports;
    }

    bt::Status onRunning() override
    {
        if (auto goal = getInput<std::string>("goal"); goal)
        {
            std::cout << "MoveBase: Moving to goal '" << *goal << "'\n";
            return bt::Status::SUCCESS;
        }
        std::cout << "MoveBase: No goal provided\n";
        return bt::Status::FAILURE;
    }
};

// Test action that reads a message from port
class SaySomething: public bt::Action
{
public:

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<std::string>("message");
        return ports;
    }

    bt::Status onRunning() override
    {
        if (auto msg = getInput<std::string>("message"); msg)
        {
            std::cout << "SaySomething: '" << *msg << "'\n";
            return bt::Status::SUCCESS;
        }
        std::cout << "SaySomething: No message provided\n";
        return bt::Status::FAILURE;
    }
};

int main()
{
    std::cout << "=== SubTree Port Remapping Example ===\n\n";

    // Register custom actions
    bt::NodeFactory factory;
    factory.registerNode<MoveBase>("MoveBase");
    factory.registerNode<SaySomething>("SaySomething");

    // Get the YAML file path
    std::string yaml_path = __FILE__;
    yaml_path = yaml_path.substr(0, yaml_path.find_last_of("/\\") + 1);
    yaml_path += "SubTreeRemapping.yaml";

    // Load the tree from YAML
    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromFile(factory, yaml_path, bb);

    if (!result.isSuccess())
    {
        std::cerr << "Failed to load tree: " << result.getError() << std::endl;
        return 1;
    }

    auto tree = result.moveValue();

    std::cout << bb->dump() << std::endl << std::endl;

    // Execute the tree
    std::cout << "Executing tree...\n";
    bt::Status status;
    do
    {
        status = tree->tick();
    } while (status == bt::Status::RUNNING);

    std::cout << "\nFinal status: " << bt::to_string(status) << "\n";

    // Check the result after SubTree execution
    auto move_result = bb->get<std::string>("move_result");
    if (move_result)
    {
        std::cout << "Result from SubTree (via port remapping): "
                  << *move_result << "\n";
    }
    else
    {
        std::cout << "No result from SubTree\n";
    }

    // Display both blackboards (like BehaviorTree.CPP example)
    std::cout << "\n";
    std::cout << bb->dump("Main Tree Blackboard") << std::endl;

    if (auto* subtree = tree->findSubTree("MoveRobot"))
    {
        std::cout << "\n";
        std::cout << subtree->blackboard()->dump("SubTree MoveRobot Blackboard")
                  << std::endl;
    }

    return 0;
}
