#include "ParallelExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class MonitorBattery final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Parallel] Battery OK\n";
        return bt::Status::SUCCESS;
    }
};

class MonitorObstacles final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Parallel] Obstacles detected -> FAILURE\n";
        return bt::Status::FAILURE;
    }
};

class MonitorComms final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Parallel] Comms nominal\n";
        return bt::Status::SUCCESS;
    }
};

int parallel_example()
{
    using namespace bt;

    auto parallel = Node::create<Parallel>(2, 2);
    parallel->addChild(Node::create<MonitorBattery>());
    parallel->addChild(Node::create<MonitorObstacles>());
    parallel->addChild(Node::create<MonitorComms>());

    auto tree = Tree::create();
    tree->setRoot(std::move(parallel));

    Status status = tree->tick();
    std::cout << "[Parallel] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
