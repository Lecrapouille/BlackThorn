#include "SequenceExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class OpenDoor final: public bt::CallbackLeaf
{
public:

    OpenDoor()
        : CallbackLeaf([]() {
              std::cout << "[Sequence] Opening door\n";
              return bt::Status::SUCCESS;
          })
    {
    }
};

class WalkThrough final: public bt::CallbackLeaf
{
public:

    WalkThrough()
        : CallbackLeaf([]() {
              std::cout << "[Sequence] Walking through doorway\n";
              return bt::Status::SUCCESS;
          })
    {
    }
};

class CloseDoor final: public bt::CallbackLeaf
{
public:

    CloseDoor()
        : CallbackLeaf([]() {
              std::cout << "[Sequence] Closing door\n";
              return bt::Status::SUCCESS;
          })
    {
    }
};

int sequence_example()
{
    using namespace bt;

    auto tree = Tree::create();
    auto& sequence = tree->createRoot<Sequence>();
    sequence.addChild<OpenDoor>();
    sequence.addChild<WalkThrough>();
    sequence.addChild<CloseDoor>();

    Status status = tree->tick();
    std::cout << "[Sequence] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
