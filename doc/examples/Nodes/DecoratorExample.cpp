#include "DecoratorExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class FlakyConnect final: public bt::Action
{
public:

    bt::Status onRunning() override
    {
        ++m_attempts;
        if (m_attempts < 3)
        {
            std::cout << "[Decorator] Connect attempt " << m_attempts
                      << " failed\n";
            return bt::Status::FAILURE;
        }
        std::cout << "[Decorator] Connect attempt " << m_attempts
                  << " succeeded\n";
        return bt::Status::SUCCESS;
    }

private:

    int m_attempts = 0;
};

int decorator_example()
{
    using namespace bt;

    auto retry = Node::create<UntilSuccess>(3);
    retry->setChild(Node::create<FlakyConnect>());

    auto tree = Tree::create();
    tree->setRoot(std::move(retry));

    Status status = Status::RUNNING;
    while (status == Status::RUNNING)
    {
        status = tree->tick();
    }

    std::cout << "[Decorator] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
