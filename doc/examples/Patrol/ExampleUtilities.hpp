/**
 * @file ExampleUtilities.hpp
 * @brief Printing helpers shared by the examples loading composite YAML data.
 *
 * The blackboard stores an entry as a \ref bt::BlackboardValue, a variant able
 * to hold scalars, sequences and nested mappings. These helpers walk that
 * variant so that an example can dump what its YAML file loaded.
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 * @see https://github.com/Lecrapouille/BlackThorn
 */

#pragma once

#include "BlackThorn/Blackboard/BlackboardValue.hpp"

#include <any>
#include <cstddef>
#include <iostream>
#include <string>
#include <variant>
#include <vector>

namespace bt::examples {

//! \brief A blackboard entry loaded from a YAML mapping.
using ValueMap = BlackboardMap;
//! \brief A blackboard entry loaded from a YAML sequence of anything.
using ValueList = std::vector<BlackboardValue>;
//! \brief A blackboard entry loaded from a YAML sequence of numbers.
using NumericList = std::vector<double>;

inline void printIndent(int indent)
{
    for (int i = 0; i < indent; ++i)
    {
        std::cout << ' ';
    }
}

// ----------------------------------------------------------------------------
//! \brief Print a blackboard entry, recursing into sequences and mappings.
//! \param[in] p_value Entry to print.
//! \param[in] p_indent Number of leading spaces.
// ----------------------------------------------------------------------------
void printValue(BlackboardValue const& p_value, int p_indent = 0);

// ----------------------------------------------------------------------------
//! \brief Print \p p_value when it holds a \c T streamable as is.
//! \return \c true when the alternative matched and was printed.
// ----------------------------------------------------------------------------
template <typename T>
inline bool printScalarAs(BlackboardValue const& p_value, int p_indent)
{
    auto const* scalar = std::get_if<T>(&p_value.asBase());
    if (scalar == nullptr)
    {
        return false;
    }
    printIndent(p_indent);
    std::cout << *scalar << std::endl;
    return true;
}

inline void printValue(BlackboardValue const& p_value, int p_indent)
{
    auto const& base = p_value.asBase();

    if (std::holds_alternative<std::monostate>(base))
    {
        printIndent(p_indent);
        std::cout << "null" << std::endl;
        return;
    }

    // Streamed apart from the other scalars: bool would print as 0 or 1 and a
    // string reads better quoted.
    if (auto const* flag = std::get_if<bool>(&base))
    {
        printIndent(p_indent);
        std::cout << (*flag ? "true" : "false") << std::endl;
        return;
    }
    if (auto const* text = std::get_if<std::string>(&base))
    {
        printIndent(p_indent);
        std::cout << '"' << *text << '"' << std::endl;
        return;
    }
    if (printScalarAs<int>(p_value, p_indent) ||
        printScalarAs<double>(p_value, p_indent) ||
        printScalarAs<float>(p_value, p_indent) ||
        printScalarAs<std::size_t>(p_value, p_indent))
    {
        return;
    }

    if (auto const* numbers = std::get_if<NumericList>(&base))
    {
        printIndent(p_indent);
        std::cout << "[ ";
        for (double number : *numbers)
        {
            std::cout << number << ' ';
        }
        std::cout << ']' << std::endl;
        return;
    }
    if (auto const* list = std::get_if<ValueList>(&base))
    {
        printIndent(p_indent);
        std::cout << '[' << std::endl;
        for (auto const& entry : *list)
        {
            printValue(entry, p_indent + 2);
        }
        printIndent(p_indent);
        std::cout << ']' << std::endl;
        return;
    }
    if (auto const* map = std::get_if<ValueMap>(&base))
    {
        printIndent(p_indent);
        std::cout << '{' << std::endl;
        for (auto const& [key, entry] : *map)
        {
            printIndent(p_indent + 2);
            std::cout << key << ':' << std::endl;
            printValue(entry, p_indent + 4);
        }
        printIndent(p_indent);
        std::cout << '}' << std::endl;
        return;
    }
    if (auto const* boxed = std::get_if<std::any>(&base))
    {
        printIndent(p_indent);
        std::cout << "<custom type: " << boxed->type().name() << '>'
                  << std::endl;
        return;
    }

    printIndent(p_indent);
    std::cout << "<unhandled alternative>" << std::endl;
}

} // namespace bt::examples
