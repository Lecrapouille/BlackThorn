#include "ExampleUtilities.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

namespace bt::examples {

class LoadGameState final: public Action
{
public:

    explicit LoadGameState(Blackboard::Ptr blackboard)
    {
        m_blackboard = std::move(blackboard);
    }

    [[nodiscard]] Status onRunning() override
    {
        if (!m_blackboard)
        {
            std::cout << "[LoadGameState] Blackboard unavailable" << std::endl;
            return Status::FAILURE;
        }

        // Debug: check if snapshot exists
        if (!m_blackboard->has("snapshot"))
        {
            std::cout << "[LoadGameState] Missing 'snapshot' parameter"
                      << std::endl;
            // Debug: check if game_state exists in parent
            if (m_blackboard->has("game_state"))
            {
                std::cout
                    << "[LoadGameState] Debug: 'game_state' found in blackboard"
                    << std::endl;
            }
            else
            {
                std::cout << "[LoadGameState] Debug: 'game_state' NOT found in "
                             "blackboard"
                          << std::endl;
            }
            return Status::FAILURE;
        }

        auto snapshot = m_blackboard->get<AnyMap>("snapshot");
        if (!snapshot)
        {
            std::cout
                << "[LoadGameState] 'snapshot' exists but is not an AnyMap"
                << std::endl;
            return Status::FAILURE;
        }

        std::cout << "[LoadGameState] Snapshot content:" << std::endl;
        printAny(*snapshot, 2);
        return Status::SUCCESS;
    }
};

class ChoosePrimaryEnemy final: public Action
{
public:

    explicit ChoosePrimaryEnemy(Blackboard::Ptr blackboard)
    {
        m_blackboard = std::move(blackboard);
    }

    [[nodiscard]] Status onRunning() override
    {
        if (!m_blackboard)
        {
            std::cout << "[ChoosePrimaryEnemy] Blackboard unavailable"
                      << std::endl;
            return Status::FAILURE;
        }

        auto candidate = m_blackboard->get<AnyMap>("candidate");
        if (!candidate)
        {
            std::cout << "[ChoosePrimaryEnemy] Missing 'candidate' parameter"
                      << std::endl;
            return Status::FAILURE;
        }

        m_blackboard->set("engaged_enemy", *candidate);

        std::cout << "[ChoosePrimaryEnemy] Evaluating candidate:" << std::endl;
        printAny(*candidate, 2);
        return Status::SUCCESS;
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

    auto yamlPath =
        "/home/qq/MyGithub/BehaviorTree/doc/examples/GameState/GameState.yaml";
    auto result = Builder::fromFile(factory, yamlPath, blackboard);
    if (!result)
    {
        std::cerr << "Failed to build tree: " << result.getError() << std::endl;
        return 1;
    }

    auto tree = result.moveValue();
    tree->setBlackboard(blackboard);

    // Connect to visualizer (optional - will work without it)
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
    Status status = tree->tick();
    std::cout << "=== Finished with status: " << to_string(status)
              << " ===" << std::endl;

    return status == bt::Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
