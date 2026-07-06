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

class LambdaTestAction: public bt::CallbackLeaf
{
public:

    using Tick = std::function<bt::Status()>;
    using Reset = std::function<void()>;

    explicit LambdaTestAction(Tick tick, Reset reset = {})
        : CallbackLeaf(std::move(tick), std::move(reset))
    {
    }

    explicit LambdaTestAction(std::pair<Tick, Reset> handlers)
        : CallbackLeaf(std::move(handlers.first), std::move(handlers.second))
    {
    }
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
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::Parallel>(2, 2); // Need 2 success or 2 failures
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Failure>();

    EXPECT_EQ(parallel.tick(), bt::Status::SUCCESS);
}

TEST(TestParallel, ThresholdFailure)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::Parallel>(3, 2);
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Failure>();
    parallel.addChild<bt::Failure>();

    EXPECT_EQ(parallel.tick(), bt::Status::FAILURE);
}

TEST(TestParallel, StillRunning)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::Parallel>(2, 2);
    parallel.addChild<bt::Success>();
    parallel.addChild<StatusAction>(bt::Status::RUNNING);
    parallel.addChild<bt::Failure>();

    EXPECT_EQ(parallel.tick(), bt::Status::RUNNING);
}

TEST(TestParallel, AllChildrenExecuted)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::Parallel>(1, 3);
    parallel.addChild<CounterAction>(&counter);
    parallel.addChild<CounterAction>(&counter);
    parallel.addChild<CounterAction>(&counter);

    EXPECT_EQ(counter, 0);
    // 3 successes >= 1 required
    EXPECT_EQ(parallel.tick(), bt::Status::SUCCESS);
    // All children executed
    EXPECT_EQ(counter, 3);
}

TEST(TestParallel, GetThresholds)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::Parallel>(3, 2);
    EXPECT_EQ(parallel.getMinSuccess(), 3);
    EXPECT_EQ(parallel.getMinFail(), 2);
}

// ===========================================================================
// ParallelAll Tests
// ===========================================================================

TEST(TestParallelAll, SuccessOnAll)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::ParallelAll>(true, true);
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Success>();

    EXPECT_EQ(parallel.tick(), bt::Status::SUCCESS);
}

TEST(TestParallelAll, SuccessOnAllWithOneFailure)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::ParallelAll>(true, true);
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Failure>();
    parallel.addChild<bt::Success>();

    EXPECT_EQ(parallel.tick(), bt::Status::RUNNING);
}

TEST(TestParallelAll, SuccessOnAny)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::ParallelAll>(false, true);
    parallel.addChild<bt::Success>();
    parallel.addChild<bt::Failure>();
    parallel.addChild<bt::Failure>();

    EXPECT_EQ(parallel.tick(), bt::Status::SUCCESS);
}

TEST(TestParallelAll, FailOnAll)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::ParallelAll>(true, true);
    parallel.addChild<bt::Failure>();
    parallel.addChild<bt::Failure>();

    EXPECT_EQ(parallel.tick(), bt::Status::FAILURE);
}

TEST(TestParallelAll, GetPolicies)
{
    auto tree = bt::Tree::create();
    auto& parallel = tree->createRoot<bt::ParallelAll>(true, false);
    EXPECT_TRUE(parallel.getSuccessOnAll());
    EXPECT_FALSE(parallel.getFailOnAll());
}
