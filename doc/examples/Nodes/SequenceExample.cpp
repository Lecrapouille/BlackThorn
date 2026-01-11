#include "SequenceExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class OpenDoor final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Sequence] Opening door\n";
        return bt::Status::SUCCESS;
    }
};

class WalkThrough final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Sequence] Walking through doorway\n";
        return bt::Status::SUCCESS;
    }
};

class CloseDoor final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        std::cout << "[Sequence] Closing door\n";
        return bt::Status::SUCCESS;
    }
};

int sequence_example()
{
    using namespace bt;

    auto sequence = Node::create<Sequence>();
    sequence->addChild(Node::create<OpenDoor>());
    sequence->addChild(Node::create<WalkThrough>());
    sequence->addChild(Node::create<CloseDoor>());

    auto tree = Tree::create();
    tree->setRoot(std::move(sequence));

    Status status = tree->tick();
    std::cout << "[Sequence] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
