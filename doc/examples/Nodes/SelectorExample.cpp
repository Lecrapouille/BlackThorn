#include "SelectorExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class ScanPrimary final: public bt::CallbackLeaf
{
public:

    ScanPrimary()
        : CallbackLeaf([]() {
              std::cout << "[Selector] Scanning primary sector ... not found\n";
              return bt::Status::FAILURE;
          })
    {
    }
};

class ScanFallback final: public bt::CallbackLeaf
{
public:

    ScanFallback()
        : CallbackLeaf([]() {
              std::cout << "[Selector] Fallback scan succeeded\n";
              return bt::Status::SUCCESS;
          })
    {
    }
};

int selector_example()
{
    using namespace bt;

    auto tree = Tree::create();
    auto& selector = tree->createRoot<Selector>();
    selector.addChild<ScanPrimary>();
    selector.addChild<ScanFallback>();

    Status status = tree->tick();
    std::cout << "[Selector] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
