#pragma once

#include <any>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace bt::examples {

using AnyMap = std::unordered_map<std::string, std::any>;
using AnyList = std::vector<std::any>;
using NumericList = std::vector<double>;

inline void printIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        std::cout << ' ';
    }
}

inline void printAny(std::any const& value, int indent = 0)
{
    if (!value.has_value())
    {
        printIndent(indent);
        std::cout << "null" << std::endl;
        return;
    }

    if (value.type() == typeid(int))
    {
        printIndent(indent);
        std::cout << std::any_cast<int>(value) << std::endl;
        return;
    }
    if (value.type() == typeid(double))
    {
        printIndent(indent);
        std::cout << std::any_cast<double>(value) << std::endl;
        return;
    }
    if (value.type() == typeid(bool))
    {
        printIndent(indent);
        std::cout << (std::any_cast<bool>(value) ? "true" : "false")
                  << std::endl;
        return;
    }
    if (value.type() == typeid(std::string))
    {
        printIndent(indent);
        std::cout << '"' << std::any_cast<std::string>(value) << '"'
                  << std::endl;
        return;
    }
    if (value.type() == typeid(NumericList))
    {
        printIndent(indent);
        std::cout << "[ ";
        for (double d : std::any_cast<NumericList>(value))
        {
            std::cout << d << ' ';
        }
        std::cout << ']' << std::endl;
        return;
    }
    if (value.type() == typeid(AnyList))
    {
        printIndent(indent);
        std::cout << "[" << std::endl;
        for (auto const& entry : std::any_cast<AnyList>(value))
        {
            printAny(entry, indent + 2);
        }
        printIndent(indent);
        std::cout << "]" << std::endl;
        return;
    }
    if (value.type() == typeid(AnyMap))
    {
        printIndent(indent);
        std::cout << "{" << std::endl;
        for (auto const& [key, entry] : std::any_cast<AnyMap>(value))
        {
            printIndent(indent + 2);
            std::cout << key << ":" << std::endl;
            printAny(entry, indent + 4);
        }
        printIndent(indent);
        std::cout << "}" << std::endl;
        return;
    }

    printIndent(indent);
    std::cout << "<unhandled type: " << value.type().name() << ">" << std::endl;
}

} // namespace bt::examples
