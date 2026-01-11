/**
 * @file TestLogical.cpp
 * @brief Unit tests for Logical decorator nodes: Inverter, ForceSuccess,
 * ForceFailure.
 *
 * Corresponds to src/BlackThorn/Nodes/Decorators/Logical.hpp
 *
 * Copyright (c) 2025 Quentin Quadrat <lecrapouille@gmail.com>
 * distributed under MIT License
 */

#include "main.hpp"

#include "BlackThorn/BlackThorn.hpp"

// ===========================================================================
// Helper Classes for Testing
// ===========================================================================

namespace {

class StatusAction final: public bt::Action
{
public:

    explicit StatusAction(bt::Status status) : m_status(status) {}

    bt::Status onRunning() override
    {
        return m_status;
    }

private:

    bt::Status m_status;
};

} // anonymous namespace

// ===========================================================================
// Inverter Tests
// ===========================================================================

TEST(TestInverter, InvertSuccess)
{
    auto inverter = bt::Node::create<bt::Inverter>();
    inverter->setChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(inverter->tick(), bt::Status::FAILURE);
}

TEST(TestInverter, InvertFailure)
{
    auto inverter = bt::Node::create<bt::Inverter>();
    inverter->setChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(inverter->tick(), bt::Status::SUCCESS);
}

TEST(TestInverter, KeepRunning)
{
    auto inverter = bt::Node::create<bt::Inverter>();
    inverter->setChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(inverter->tick(), bt::Status::RUNNING);
}

TEST(TestInverter, NoChildInvalid)
{
    auto inverter = bt::Node::create<bt::Inverter>();
    EXPECT_FALSE(inverter->isValid());
}

// ===========================================================================
// ForceSuccess Tests
// ===========================================================================

TEST(TestForceSuccess, SuccessStaysSuccess)
{
    auto force = bt::Node::create<bt::ForceSuccess>();
    force->setChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(force->tick(), bt::Status::SUCCESS);
}

TEST(TestForceSuccess, FailureBecomesSuccess)
{
    auto force = bt::Node::create<bt::ForceSuccess>();
    force->setChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(force->tick(), bt::Status::SUCCESS);
}

TEST(TestForceSuccess, RunningStaysRunning)
{
    auto force = bt::Node::create<bt::ForceSuccess>();
    force->setChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(force->tick(), bt::Status::RUNNING);
}

// ===========================================================================
// ForceFailure Tests
// ===========================================================================

TEST(TestForceFailure, SuccessBecomesFailure)
{
    auto force = bt::Node::create<bt::ForceFailure>();
    force->setChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(force->tick(), bt::Status::FAILURE);
}

TEST(TestForceFailure, FailureStaysFailure)
{
    auto force = bt::Node::create<bt::ForceFailure>();
    force->setChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(force->tick(), bt::Status::FAILURE);
}

TEST(TestForceFailure, RunningStaysRunning)
{
    auto force = bt::Node::create<bt::ForceFailure>();
    force->setChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));

    EXPECT_EQ(force->tick(), bt::Status::RUNNING);
}
