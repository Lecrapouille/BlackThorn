/**
 * @file StatusDemo.cpp
 * @brief Demo showing different node statuses for visualizer testing
 *
 * Run Oakular in Visualizer mode, then run this example to see
 * nodes change color: green (SUCCESS), yellow (RUNNING), red (FAILURE), gray
 * (INVALID)
 */

#include "BlackThorn/BlackThorn.hpp"
#include <chrono>
#include <iostream>
#include <random>
#include <thread>

// Random generator for simulating failures
static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_int_distribution<> dis(0, 2);

// Counter for attempt tracking
static int g_attempt = 0;

int main()
{
    // Create blackboard
    auto blackboard = std::make_shared<bt::Blackboard>();

    // Create factory and register actions
    bt::NodeFactory factory;

    // CheckConditionA: Always fails (to demonstrate red status)
    factory.registerAction("CheckConditionA", []() {
        std::cout << "[CheckConditionA] Checking... FAILED" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return bt::Status::FAILURE;
    });

    // ExecuteA: Never reached because CheckConditionA fails
    factory.registerAction("ExecuteA", []() {
        std::cout << "[ExecuteA] This should not be reached" << std::endl;
        return bt::Status::SUCCESS;
    });

    // ExecuteB: Always succeeds (after Wait completes)
    factory.registerAction("ExecuteB", []() {
        std::cout << "[ExecuteB] Executing... SUCCESS" << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        return bt::Status::SUCCESS;
    });

    // TryAction: Random outcome to show different statuses
    factory.registerAction("TryAction", []() {
        g_attempt++;
        int outcome = dis(gen);
        std::cout << "[TryAction] Attempt " << g_attempt << ": ";

        std::this_thread::sleep_for(std::chrono::milliseconds(150));

        if (outcome == 0)
        {
            std::cout << "SUCCESS" << std::endl;
            return bt::Status::SUCCESS;
        }
        else
        {
            std::cout << "FAILURE" << std::endl;
            return bt::Status::FAILURE;
        }
    });

    // Get the YAML file path
    std::string yaml_path = __FILE__;
    yaml_path = yaml_path.substr(0, yaml_path.find_last_of("/\\") + 1);
    yaml_path += "StatusDemo.yaml";

    // Build tree from YAML
    auto result = bt::Builder::fromFile(factory, yaml_path, blackboard);
    if (!result)
    {
        std::cerr << "Failed to build tree: " << result.getError() << std::endl;
        return 1;
    }

    auto& tree = result.getValue();

    // Connect to visualizer
    auto visualizer = std::make_shared<bt::VisualizerClient>();
    if (visualizer->connect())
    {
        std::cout << "=== Connected to visualizer on localhost:8888 ==="
                  << std::endl;
        std::cout << "=== Open Oakular in Visualizer mode to see the tree ==="
                  << std::endl;
        tree->setVisualizerClient(visualizer);
    }
    else
    {
        std::cout << "=== Visualizer not available (Oakular not running?) ==="
                  << std::endl;
        std::cout << "=== Running without visualization ===" << std::endl;
    }

    std::cout << "\n=== Running " << yaml_path << " ===" << std::endl;
    std::cout << "=== Watch nodes change color in Oakular! ===" << std::endl;
    std::cout << "=== Green=SUCCESS, Yellow=RUNNING, Red=FAILURE ==="
              << std::endl;
    std::cout << std::endl;

    // Run the tree multiple times to show status changes
    for (int i = 0; i < 3; ++i)
    {
        std::cout << "\n--- Run " << (i + 1) << " ---" << std::endl;
        g_attempt = 0;
        tree->reset();

        bt::Status status = bt::Status::RUNNING;
        while (status == bt::Status::RUNNING)
        {
            status = tree->tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }

        std::cout << "Tree completed with status: " << bt::to_string(status)
                  << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    std::cout << "\n=== Demo completed ===" << std::endl;
    return 0;
}
