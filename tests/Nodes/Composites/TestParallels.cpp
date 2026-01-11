/**
 * @file TestParallels.cpp
 * @brief Unit tests for Parallel composite nodes.
 *
 * Corresponds to src/BlackThorn/Nodes/Composites/Parallels.hpp
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

class LambdaTestAction: public bt::Action
{
public:

    using Tick = std::function<bt::Status()>;

    explicit LambdaTestAction(Tick tick) : m_tick(std::move(tick)) {}

    bt::Status onRunning() override
    {
        return m_tick ? m_tick() : bt::Status::FAILURE;
    }

private:

    Tick m_tick;
};

class CounterAction final: public LambdaTestAction
{
public:

    explicit CounterAction(int* counter)
        : LambdaTestAction([counter]() {
              (*counter)++;
              return bt::Status::SUCCESS;
          })
    {
    }
};

class StatusAction final: public LambdaTestAction
{
public:

    explicit StatusAction(bt::Status status)
        : LambdaTestAction([status]() { return status; })
    {
    }
};

} // anonymous namespace

// ===========================================================================
// Parallel Tests
// ===========================================================================

TEST(TestParallel, ThresholdSuccess)
{
    auto parallel =
        bt::Node::create<bt::Parallel>(2, 2); // Need 2 success or 2 failures
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(parallel->tick(), bt::Status::SUCCESS);
}

TEST(TestParallel, ThresholdFailure)
{
    auto parallel = bt::Node::create<bt::Parallel>(3, 2);
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Failure>());
    parallel->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(parallel->tick(), bt::Status::FAILURE);
}

TEST(TestParallel, StillRunning)
{
    auto parallel = bt::Node::create<bt::Parallel>(2, 2);
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));
    parallel->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(parallel->tick(), bt::Status::RUNNING);
}

TEST(TestParallel, AllChildrenExecuted)
{
    int counter = 0;
    auto parallel = bt::Node::create<bt::Parallel>(1, 3);
    parallel->addChild(bt::Node::create<CounterAction>(&counter));
    parallel->addChild(bt::Node::create<CounterAction>(&counter));
    parallel->addChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(parallel->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 3); // All children executed
}

TEST(TestParallel, GetThresholds)
{
    auto parallel = bt::Node::create<bt::Parallel>(3, 2);
    EXPECT_EQ(parallel->getMinSuccess(), 3);
    EXPECT_EQ(parallel->getMinFail(), 2);
}

// ===========================================================================
// ParallelAll Tests
// ===========================================================================

TEST(TestParallelAll, SuccessOnAll)
{
    auto parallel = bt::Node::create<bt::ParallelAll>(true, true);
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(parallel->tick(), bt::Status::SUCCESS);
}

TEST(TestParallelAll, SuccessOnAllWithOneFailure)
{
    auto parallel = bt::Node::create<bt::ParallelAll>(true, true);
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Failure>());
    parallel->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(parallel->tick(), bt::Status::RUNNING);
}

TEST(TestParallelAll, SuccessOnAny)
{
    auto parallel = bt::Node::create<bt::ParallelAll>(false, true);
    parallel->addChild(bt::Node::create<bt::Success>());
    parallel->addChild(bt::Node::create<bt::Failure>());
    parallel->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(parallel->tick(), bt::Status::SUCCESS);
}

TEST(TestParallelAll, FailOnAll)
{
    auto parallel = bt::Node::create<bt::ParallelAll>(true, true);
    parallel->addChild(bt::Node::create<bt::Failure>());
    parallel->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(parallel->tick(), bt::Status::FAILURE);
}

TEST(TestParallelAll, GetPolicies)
{
    auto parallel = bt::Node::create<bt::ParallelAll>(true, false);
    EXPECT_TRUE(parallel->getSuccessOnAll());
    EXPECT_FALSE(parallel->getFailOnAll());
}
