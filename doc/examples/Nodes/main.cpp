#include "DecoratorExample.hpp"
#include "ParallelExample.hpp"
#include "SelectorExample.hpp"
#include "SequenceExample.hpp"

#include <iostream>

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0]
                  << " [decorator|parallel|selector|sequence]" << std::endl;
        return EXIT_FAILURE;
    }

    std::string example = argv[1];

    if (example == "decorator")
    {
        return decorator_example();
    }
    if (example == "parallel")
    {
        return parallel_example();
    }
    if (example == "selector")
    {
        return selector_example();
    }
    if (example == "sequence")
    {
        return sequence_example();
    }

    std::cerr << "Usage: " << argv[0]
              << " [decorator|parallel|selector|sequence|leaf]" << std::endl;
    return EXIT_FAILURE;
}