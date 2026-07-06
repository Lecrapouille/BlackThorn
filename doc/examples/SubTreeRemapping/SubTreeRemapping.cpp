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

class MoveBase: public bt::CallbackLeaf
{
public:

    MoveBase()
        : CallbackLeaf([this]() {
              if (auto goal = getInput<std::string>("goal"); goal)
              {
                  std::cout << "MoveBase: Moving to goal '" << *goal << "'\n";
                  return bt::Status::SUCCESS;
              }
              std::cout << "MoveBase: No goal provided\n";
              return bt::Status::FAILURE;
          })
    {
    }

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<std::string>("goal");
        return ports;
    }
};

class SaySomething: public bt::CallbackLeaf
{
public:

    SaySomething()
        : CallbackLeaf([this]() {
              if (auto msg = getInput<std::string>("message"); msg)
              {
                  std::cout << "SaySomething: '" << *msg << "'\n";
                  return bt::Status::SUCCESS;
              }
              std::cout << "SaySomething: No message provided\n";
              return bt::Status::FAILURE;
          })
    {
    }

    bt::PortList providedPorts() const override
    {
        bt::PortList ports;
        ports.addInput<std::string>("message");
        return ports;
    }
};

int main()
{
    bt::NodeFactory factory;
    factory.registerNode<MoveBase>("MoveBase");
    factory.registerNode<SaySomething>("SaySomething");

    std::string yaml_path = __FILE__;
    yaml_path = yaml_path.substr(0, yaml_path.find_last_of("/\\") + 1);
    yaml_path += "SubTreeRemapping.yaml";

    auto bb = std::make_shared<bt::Blackboard>();
    auto result = bt::Builder::fromFile(factory, yaml_path, bb);

    if (!result.isSuccess())
    {
        std::cerr << "Failed to load tree: " << result.getError() << std::endl;
        return 1;
    }

    auto tree = result.moveValue();

    std::cout << bb->dump() << std::endl << std::endl;

    std::cout << "Executing tree...\n";
    bt::Status status;
    do
    {
        status = tree->tick();
    } while (status == bt::Status::RUNNING);

    std::cout << "\nFinal status: " << bt::to_string(status) << "\n";

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
