#include "SelectorExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class ScanPrimary final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Selector] Scanning primary sector ... not found\n";
        return bt::Status::FAILURE;
    }
};

class ScanFallback final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Selector] Fallback scan succeeded\n";
        return bt::Status::SUCCESS;
    }
};

int selector_example()
{
    using namespace bt;

    auto selector = Node::create<Selector>();
    selector->addChild(Node::create<ScanPrimary>());
    selector->addChild(Node::create<ScanFallback>());

    auto tree = Tree::create();
    tree->setRoot(std::move(selector));

    Status status = tree->tick();
    std::cout << "[Selector] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
