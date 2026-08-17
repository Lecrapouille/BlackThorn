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
              [this]() {
                  // "route: ${patrol_route}" in the YAML declares a port
                  // remapped to a blackboard variable, read through the port.
                  auto route = getInput<ValueMap>("route");
                  if (!route)
                  {
                      std::cout << "[LoadRoute] Cannot resolve the 'route' port"
                                << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[LoadRoute] Patrol route:" << std::endl;
                  printValue(*route, 2);
                  return Status::SUCCESS;
              },
              blackboard)
    {
    }
};

class FollowWaypoints final: public CallbackLeaf
{
public:

    explicit FollowWaypoints(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [this]() {
                  auto path = getInput<ValueMap>("path");
                  if (!path)
                  {
                      std::cout
                          << "[FollowWaypoints] Cannot resolve the 'path' port"
                          << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[FollowWaypoints] Executing patrol:"
                            << std::endl;
                  printValue(*path, 2);
                  return Status::SUCCESS;
              },
              blackboard)
    {
    }
};

class AttemptNonLethal final: public CallbackLeaf
{
public:

    explicit AttemptNonLethal(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [this]() {
                  auto contact = getInput<ValueMap>("contact");
                  if (!contact)
                  {
                      std::cout
                          << "[AttemptNonLethal] Cannot resolve the 'contact' "
                             "port"
                          << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout
                      << "[AttemptNonLethal] Attempting peaceful resolution:"
                      << std::endl;
                  printValue(*contact, 2);
                  std::cout << "[AttemptNonLethal] Contact resisted, "
                               "escalating..."
                            << std::endl;
                  return Status::FAILURE;
              },
              blackboard)
    {
    }
};

class NeutralizeThreat final: public CallbackLeaf
{
public:

    explicit NeutralizeThreat(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [this]() {
                  auto contact = getInput<ValueMap>("target");
                  if (!contact)
                  {
                      std::cout
                          << "[NeutralizeThreat] Cannot resolve the 'target' "
                             "port"
                          << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[NeutralizeThreat] Engaging hostile target:"
                            << std::endl;
                  printValue(*contact, 2);
                  std::cout << "[NeutralizeThreat] Threat neutralized"
                            << std::endl;
                  return Status::SUCCESS;
              },
              blackboard)
    {
    }
};

class ExtractTeam final: public CallbackLeaf
{
public:

    explicit ExtractTeam(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [this]() {
                  auto destination = getInput<ValueMap>("destination");
                  if (!destination)
                  {
                      std::cout << "[ExtractTeam] Cannot resolve the "
                                   "'destination' port"
                                << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[ExtractTeam] Heading to extraction point:"
                            << std::endl;
                  printValue(*destination, 2);
                  std::cout << "[ExtractTeam] Team evacuated" << std::endl;
                  return Status::SUCCESS;
              },
              blackboard)
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

#if defined(BLACKTHORN_HAS_NETWORK)
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
#else
    std::cout << "=== Built without network support: no visualization ==="
              << std::endl;
#endif

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
