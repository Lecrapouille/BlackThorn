// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file main.cpp
 * @brief Entry point for Oakular - BlackThorn behavior tree editor
 *
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
