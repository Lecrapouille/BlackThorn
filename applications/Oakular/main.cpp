/**
 * @file main.cpp
 * @brief Entry point for Oakular - BlackThorn behavior tree editor
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

// Must be defined before including imgui.h (through OakularApp.hpp)
#define IMGUI_DEFINE_MATH_OPERATORS

#include "OakularApp.hpp"

#include <iostream>

int main()
{
    try
    {
        // Create and run Oakular editor
        OakularApp app(1600, 900);

        if (!app.run())
        {
            std::cerr << "Failed to run Oakular: " << app.error() << std::endl;
            return EXIT_FAILURE;
        }

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
