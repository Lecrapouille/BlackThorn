#include "DecoratorExample.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <iostream>

class FlakyConnect final: public bt::CallbackLeaf
{
public:

    FlakyConnect()
        : CallbackLeaf([this]() {
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
          })
    {
    }

private:

    int m_attempts = 0;
};

int decorator_example()
{
    using namespace bt;

    auto tree = Tree::create();
    auto& retry = tree->createRoot<UntilSuccess>(3);
    retry.createChild<FlakyConnect>();

    Status status = Status::RUNNING;
    while (status == Status::RUNNING)
    {
        status = tree->tick();
    }

    std::cout << "[Decorator] Result: " << to_string(status) << '\n';
    return status == Status::SUCCESS ? EXIT_SUCCESS : EXIT_FAILURE;
}
