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

#include <cstdlib>
#include <iostream>
#include <string>

// ----------------------------------------------------------------------------
static int usage(char const* p_program)
{
    std::cout << "Usage: " << p_program << " [--port N]\n"
              << "  --port N   TCP port listened to in Visualizer mode "
                 "(default: 8888)\n";
    return EXIT_SUCCESS;
}

// ----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
    uint16_t port = 8888;
    for (int i = 1; i < argc; ++i)
    {
        std::string const argument(argv[i]);
        if ((argument == "--help") || (argument == "-h"))
        {
            return usage(argv[0]);
        }
        if ((argument == "--port") && ((i + 1) < argc))
        {
            long const value = std::strtol(argv[++i], nullptr, 10);
            if ((value <= 0) || (value > 65535))
            {
                std::cerr << "Invalid port: " << argv[i] << std::endl;
                return EXIT_FAILURE;
            }
            port = static_cast<uint16_t>(value);
            continue;
        }
        std::cerr << "Unknown option: " << argument << std::endl;
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    try
    {
        // Create and run Oakular editor
        oakular::OakularApp app(1600, 900, port);

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
