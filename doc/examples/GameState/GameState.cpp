#include "ExampleUtilities.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

namespace bt::examples {

class LoadGameState final: public CallbackLeaf
{
public:

    explicit LoadGameState(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [this]() {
                  // The YAML declares "snapshot: ${game_state}", which is a port
                  // remapping and not a blackboard entry named "snapshot": read
                  // it through the port, which resolves to the variable.
                  auto snapshot = getInput<ValueMap>("snapshot");
                  if (!snapshot)
                  {
                      std::cout << "[LoadGameState] Cannot resolve the "
                                   "'snapshot' port"
                                << std::endl;
                      return Status::FAILURE;
                  }

                  std::cout << "[LoadGameState] Snapshot content:" << std::endl;
                  printValue(*snapshot, 2);
                  return Status::SUCCESS;
              },
              blackboard)
    {
    }
};

class ChoosePrimaryEnemy final: public CallbackLeaf
{
public:

    explicit ChoosePrimaryEnemy(Blackboard::Ptr blackboard)
        : CallbackLeaf(
              [this]() {
                  auto const bb = this->blackboard();
                  auto candidate = getInput<ValueMap>("candidate");
                  if (!bb || !candidate)
                  {
                      std::cout << "[ChoosePrimaryEnemy] Cannot resolve the "
                                   "'candidate' port"
                                << std::endl;
                      return Status::FAILURE;
                  }

                  bb->set("engaged_enemy", *candidate);

                  std::cout << "[ChoosePrimaryEnemy] Evaluating candidate:"
                            << std::endl;
                  printValue(*candidate, 2);
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
    factory.registerNode<LoadGameState>("LoadGameState", blackboard);
    factory.registerNode<ChoosePrimaryEnemy>("ChoosePrimaryEnemy", blackboard);

    auto yamlPath = "doc/examples/GameState/GameState.yaml";
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
    Status status = tree->tick();
    std::cout << "=== Finished with status: " << to_string(status)
              << " ===" << std::endl;

    return status == bt::Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
