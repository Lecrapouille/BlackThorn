// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file main.cpp
 * @brief Main entry point for unit test execution - Initializes the Google Test
 * framework and runs all tests.
 *
 * @see https://github.com/Lecrapouille/Robotik
 */

#include "main.hpp"

int main(int argc, char* argv[])
{
    // The following line must be executed to initialize Google Mock
    // (and Google Test) before running the tests.
    ::testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
