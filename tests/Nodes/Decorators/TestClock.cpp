// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file TestClock.cpp
 * @brief Unit tests for the clock of a tree: timed nodes follow an injected
 * clock instead of the wall clock.
 *
 * Corresponds to Tree::clock in src/BlackThorn/Nodes/Tree.hpp
 *
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

#include <chrono>

namespace {

//! A clock moved by hand: what a simulator does with its simulated time.
struct ManualClock
{
    std::chrono::steady_clock::time_point time{};

    void advance(std::chrono::milliseconds p_step)
    {
        time += p_step;
    }
};

} // namespace

TEST(TestClock, WaitFollowsTheInjectedClock)
{
    ManualClock clock;
    auto tree = bt::Tree::create();
    tree->clock([&clock] { return clock.time; });
    tree->createRoot<bt::Wait>(100);

    EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
    clock.advance(std::chrono::milliseconds(99));
    EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
    clock.advance(std::chrono::milliseconds(1));
    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestClock, TimeoutDoesNotExpireWhileTheClockStands)
{
    ManualClock clock;
    auto tree = bt::Tree::create();
    tree->clock([&clock] { return clock.time; });
    auto& timeout = tree->createRoot<bt::Timeout>(50);
    timeout.createChild<bt::Wait>(1000);

    // Wall time passes, the injected clock does not: no timeout.
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
    }
    clock.advance(std::chrono::milliseconds(50));
    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestClock, EmptyFunctionRestoresTheWallClock)
{
    auto tree = bt::Tree::create();
    tree->clock({});
    auto const before = std::chrono::steady_clock::now();
    EXPECT_GE(tree->now(), before);
}
