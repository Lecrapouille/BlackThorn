// SPDX-License-Identifier: GPL-3.0-or-later OR LicenseRef-BlackThorn-Commercial
// Copyright (c) 2025-2026 Quentin Quadrat
//
// This file is part of BlackThorn. It is available under the GNU GPL v3 or,
// for users who cannot use the GPL, under a commercial license.
// See LICENSING.md for details.

/**
 * @file TestLogical.cpp
 * @brief Unit tests for Logical decorator nodes: Inverter, ForceSuccess,
 * ForceFailure.
 *
 * Corresponds to src/BlackThorn/Nodes/Decorators/Logical.hpp
 *
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

// ===========================================================================
// Helper Classes for Testing
// ===========================================================================

namespace {

class StatusAction final: public bt::CallbackLeaf
{
public:

    explicit StatusAction(bt::Status status)
        : CallbackLeaf([status]() { return status; })
    {
    }
};

} // anonymous namespace

// ===========================================================================
// Inverter Tests
// ===========================================================================

TEST(TestInverter, InvertSuccess)
{
    auto tree = bt::Tree::create();
    auto& inverter = tree->createRoot<bt::Inverter>();
    inverter.createChild<bt::Success>();

    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestInverter, InvertFailure)
{
    auto tree = bt::Tree::create();
    auto& inverter = tree->createRoot<bt::Inverter>();
    inverter.createChild<bt::Failure>();

    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestInverter, KeepRunning)
{
    auto tree = bt::Tree::create();
    auto& inverter = tree->createRoot<bt::Inverter>();
    inverter.createChild<StatusAction>(bt::Status::RUNNING);

    EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
}

TEST(TestInverter, NoChildInvalid)
{
    auto tree = bt::Tree::create();
    auto& inverter = tree->createRoot<bt::Inverter>();
    EXPECT_FALSE(inverter.isValid());
}

// ===========================================================================
// ForceSuccess Tests
// ===========================================================================

TEST(TestForceSuccess, SuccessStaysSuccess)
{
    auto tree = bt::Tree::create();
    auto& force = tree->createRoot<bt::ForceSuccess>();
    force.createChild<bt::Success>();

    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestForceSuccess, FailureBecomesSuccess)
{
    auto tree = bt::Tree::create();
    auto& force = tree->createRoot<bt::ForceSuccess>();
    force.createChild<bt::Failure>();

    EXPECT_EQ(tree->tick(), bt::Status::SUCCESS);
}

TEST(TestForceSuccess, RunningStaysRunning)
{
    auto tree = bt::Tree::create();
    auto& force = tree->createRoot<bt::ForceSuccess>();
    force.createChild<StatusAction>(bt::Status::RUNNING);

    EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
}

// ===========================================================================
// ForceFailure Tests
// ===========================================================================

TEST(TestForceFailure, SuccessBecomesFailure)
{
    auto tree = bt::Tree::create();
    auto& force = tree->createRoot<bt::ForceFailure>();
    force.createChild<bt::Success>();

    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestForceFailure, FailureStaysFailure)
{
    auto tree = bt::Tree::create();
    auto& force = tree->createRoot<bt::ForceFailure>();
    force.createChild<bt::Failure>();

    EXPECT_EQ(tree->tick(), bt::Status::FAILURE);
}

TEST(TestForceFailure, RunningStaysRunning)
{
    auto tree = bt::Tree::create();
    auto& force = tree->createRoot<bt::ForceFailure>();
    force.createChild<StatusAction>(bt::Status::RUNNING);

    EXPECT_EQ(tree->tick(), bt::Status::RUNNING);
}
