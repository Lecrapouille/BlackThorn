/**
 * @file TestSelectors.cpp
 * @brief Unit tests for Selector composite nodes.
 *
 * Corresponds to src/BlackThorn/Nodes/Composites/Selectors.hpp
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
// Selector Tests
// ===========================================================================

TEST(TestSelector, FirstChildSucceeds)
{
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::Selector>();
    selector.addChild<bt::Success>();
    selector.addChild<bt::Failure>();

    EXPECT_EQ(selector.tick(), bt::Status::SUCCESS);
}

TEST(TestSelector, AllChildrenFail)
{
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::Selector>();
    selector.addChild<bt::Failure>();
    selector.addChild<bt::Failure>();
    selector.addChild<bt::Failure>();

    EXPECT_EQ(selector.tick(), bt::Status::FAILURE);
}

TEST(TestSelector, MiddleChildSucceeds)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::Selector>();
    selector.addChild<bt::Failure>();
    selector.addChild<CounterAction>(&counter);
    selector.addChild<bt::Failure>();

    EXPECT_EQ(selector.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
}

TEST(TestSelector, ChildReturnsRunning)
{
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::Selector>();
    selector.addChild<bt::Failure>();
    selector.addChild<StatusAction>(bt::Status::RUNNING);
    selector.addChild<bt::Success>();

    EXPECT_EQ(selector.tick(), bt::Status::RUNNING);
}

TEST(TestSelector, ExecutionStopsOnSuccess)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::Selector>();
    selector.addChild<bt::Failure>();
    selector.addChild<bt::Success>();
    selector.addChild<CounterAction>(&counter);

    EXPECT_EQ(selector.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 0); // Third child never executed
}

// ===========================================================================
// ReactiveSelector Tests
// ===========================================================================

TEST(TestReactiveSelector, RestartsOnEachTick)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::ReactiveSelector>();
    selector.addChild<bt::Failure>();
    selector.addChild<CounterAction>(&counter);

    EXPECT_EQ(selector.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);

    EXPECT_EQ(selector.tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 2); // Counter incremented again
}

// ===========================================================================
// SelectorWithMemory Tests
// ===========================================================================

TEST(TestSelectorWithMemory, RemembersProgress)
{
    int counter = 0;
    auto tree = bt::Tree::create();
    auto& selector = tree->createRoot<bt::SelectorWithMemory>();
    selector.addChild<bt::Failure>();
    selector.addChild<StatusAction>(bt::Status::RUNNING);
    selector.addChild<CounterAction>(&counter);

    EXPECT_EQ(selector.tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 0); // Third child not reached

    EXPECT_EQ(selector.tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 0); // Still not reached
}
