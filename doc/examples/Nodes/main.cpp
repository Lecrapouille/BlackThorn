#include "DecoratorExample.hpp"
#include "LeafExample.hpp"
#include "ParallelExample.hpp"
#include "SelectorExample.hpp"
#include "SequenceExample.hpp"

#include <iostream>

static int usage(char const* program)
{
    std::cerr << "Usage: " << program
              << " [decorator|parallel|selector|sequence|leaf]" << std::endl;
    return EXIT_FAILURE;
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        return usage(argv[0]);
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
    if (example == "leaf")
    {
        return leaf_example();
    }

    return usage(argv[0]);
}
