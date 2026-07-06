/**
 * @file Patrol.cpp
 * @brief Example of a patrol behavior tree with real-time visualization.
 */

#include "ExampleUtilities.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <chrono>
#include <iostream>
#include <thread>

namespace bt::examples {

class LoadRoute final: public CallbackLeaf
{
public:

    explicit LoadRoute(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [bb = blackboard]() {
                  if (!bb)
                  {
                      std::cout << "[LoadRoute] Blackboard unavailable"
                                << std::endl;
                      return Status::FAILURE;
                  }
                  auto route = bb->get<AnyMap>("route");
                  if (!route)
                  {
                      std::cout << "[LoadRoute] Missing 'route' parameter"
                                << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[LoadRoute] Patrol route:" << std::endl;
                  printAny(*route, 2);
                  return Status::SUCCESS;
              },
              std::move(blackboard))
    {
    }
};

class FollowWaypoints final: public CallbackLeaf
{
public:

    explicit FollowWaypoints(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [bb = blackboard]() {
                  if (!bb)
                  {
                      std::cout << "[FollowWaypoints] Blackboard unavailable"
                                << std::endl;
                      return Status::FAILURE;
                  }
                  auto path = bb->get<AnyMap>("path");
                  if (!path)
                  {
                      std::cout << "[FollowWaypoints] Missing 'path' parameter"
                                << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[FollowWaypoints] Executing patrol:"
                            << std::endl;
                  printAny(*path, 2);
                  return Status::SUCCESS;
              },
              std::move(blackboard))
    {
    }
};

class AttemptNonLethal final: public CallbackLeaf
{
public:

    explicit AttemptNonLethal(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [bb = blackboard]() {
                  if (!bb)
                  {
                      std::cout << "[AttemptNonLethal] Blackboard unavailable"
                                << std::endl;
                      return Status::FAILURE;
                  }
                  auto contact = bb->get<AnyMap>("contact");
                  if (!contact)
                  {
                      std::cout
                          << "[AttemptNonLethal] Missing 'contact' parameter"
                          << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout
                      << "[AttemptNonLethal] Attempting peaceful resolution:"
                      << std::endl;
                  printAny(*contact, 2);
                  std::cout << "[AttemptNonLethal] Contact resisted, "
                               "escalating..."
                            << std::endl;
                  return Status::FAILURE;
              },
              std::move(blackboard))
    {
    }
};

class NeutralizeThreat final: public CallbackLeaf
{
public:

    explicit NeutralizeThreat(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [bb = blackboard]() {
                  if (!bb)
                  {
                      std::cout << "[NeutralizeThreat] Blackboard unavailable"
                                << std::endl;
                      return Status::FAILURE;
                  }
                  auto contact = bb->get<AnyMap>("contact");
                  if (!contact)
                  {
                      std::cout
                          << "[NeutralizeThreat] Missing 'contact' parameter"
                          << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[NeutralizeThreat] Engaging hostile target:"
                            << std::endl;
                  printAny(*contact, 2);
                  std::cout << "[NeutralizeThreat] Threat neutralized"
                            << std::endl;
                  return Status::SUCCESS;
              },
              std::move(blackboard))
    {
    }
};

class ExtractTeam final: public CallbackLeaf
{
public:

    explicit ExtractTeam(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [bb = blackboard]() {
                  if (!bb)
                  {
                      std::cout << "[ExtractTeam] Blackboard unavailable"
                                << std::endl;
                      return Status::FAILURE;
                  }
                  auto destination = bb->get<AnyMap>("destination");
                  if (!destination)
                  {
                      std::cout
                          << "[ExtractTeam] Missing 'destination' parameter"
                          << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[ExtractTeam] Heading to extraction point:"
                            << std::endl;
                  printAny(*destination, 2);
                  std::cout << "[ExtractTeam] Team evacuated" << std::endl;
                  return Status::SUCCESS;
              },
              std::move(blackboard))
    {
    }
};

} // namespace bt::examples

int main()
{
    using namespace bt;
    using namespace bt::examples;

    auto blackboard = std::make_shared<Blackboard>();
    NodeFactory factory;
    factory.registerNode<LoadRoute>("LoadRoute", blackboard);
    factory.registerNode<FollowWaypoints>("FollowWaypoints", blackboard);
    factory.registerNode<AttemptNonLethal>("AttemptNonLethal", blackboard);
    factory.registerNode<NeutralizeThreat>("NeutralizeThreat", blackboard);
    factory.registerNode<ExtractTeam>("ExtractTeam", blackboard);

    auto yamlPath = "doc/examples/Patrol/Patrol.yaml";
    auto result = Builder::fromFile(factory, yamlPath, blackboard);
    if (!result)
    {
        std::cerr << "Failed to build tree: " << result.getError() << std::endl;
        return 1;
    }

    auto tree = result.moveValue();
    tree->setBlackboard(blackboard);

    auto visualizer = std::make_shared<VisualizerClient>();
    if (visualizer->connect("localhost", 8888))
    {
        tree->setVisualizerClient(visualizer);
        std::cout << "=== Connected to visualizer on localhost:8888 ==="
                  << std::endl;
        std::cout << "=== Open Oakular in Visualizer mode to see the tree ==="
                  << std::endl;
    }
    else
    {
        std::cout << "=== Visualizer not available (Oakular not running?) ==="
                  << std::endl;
        std::cout << "=== Running without visualization ===" << std::endl;
    }

    std::cout << "=== Running " << yamlPath << " ===" << std::endl;

    int tick_count = 0;
    const int max_ticks = 10;

    while (tick_count < max_ticks)
    {
        Status status = tree->tick();
        std::cout << "=== Tick " << (tick_count + 1)
                  << " - Status: " << to_string(status) << " ===" << std::endl;

        tree->reset();
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        tick_count++;
    }

    std::cout << "=== Finished after " << tick_count
              << " ticks ===" << std::endl;

    return EXIT_SUCCESS;
}
