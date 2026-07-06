#include "ParallelExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class MonitorBattery final: public bt::CallbackLeaf
{
public:

    MonitorBattery()
        : CallbackLeaf([]() {
              std::cout << "[Parallel] Battery OK\n";
              return bt::Status::SUCCESS;
          })
    {
    }
};

class MonitorObstacles final: public bt::CallbackLeaf
{
public:

    MonitorObstacles()
        : CallbackLeaf([]() {
              std::cout << "[Parallel] Obstacles detected -> FAILURE\n";
              return bt::Status::FAILURE;
          })
    {
    }
};

class MonitorComms final: public bt::CallbackLeaf
{
public:

    MonitorComms()
        : CallbackLeaf([]() {
              std::cout << "[Parallel] Comms nominal\n";
              return bt::Status::SUCCESS;
          })
    {
    }
};

int parallel_example()
{
    using namespace bt;

    auto tree = Tree::create();
    auto& parallel = tree->createRoot<Parallel>(2, 2);
    parallel.addChild<MonitorBattery>();
    parallel.addChild<MonitorObstacles>();
    parallel.addChild<MonitorComms>();

    Status status = tree->tick();
    std::cout << "[Parallel] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
