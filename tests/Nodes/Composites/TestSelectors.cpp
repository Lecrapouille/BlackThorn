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

class LambdaTestAction: public bt::Action
{
public:

    using Tick = std::function<bt::Status()>;
    using Reset = std::function<void()>;

    explicit LambdaTestAction(Tick tick, Reset reset = {})
        : m_tick(std::move(tick)), m_reset(std::move(reset))
    {
    }

    bt::Status onRunning() override
    {
        return m_tick ? m_tick() : bt::Status::FAILURE;
    }

    void reset() override
    {
        bt::Action::reset();
        if (m_reset)
        {
            m_reset();
        }
    }

private:

    Tick m_tick;
    Reset m_reset;
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
    auto selector = bt::Node::create<bt::Selector>();
    selector->addChild(bt::Node::create<bt::Success>());
    selector->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(selector->tick(), bt::Status::SUCCESS);
}

TEST(TestSelector, AllChildrenFail)
{
    auto selector = bt::Node::create<bt::Selector>();
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(selector->tick(), bt::Status::FAILURE);
}

TEST(TestSelector, MiddleChildSucceeds)
{
    int counter = 0;
    auto selector = bt::Node::create<bt::Selector>();
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<CounterAction>(&counter));
    selector->addChild(bt::Node::create<bt::Failure>());

    EXPECT_EQ(selector->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);
}

TEST(TestSelector, ChildReturnsRunning)
{
    auto selector = bt::Node::create<bt::Selector>();
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));
    selector->addChild(bt::Node::create<bt::Success>());

    EXPECT_EQ(selector->tick(), bt::Status::RUNNING);
}

TEST(TestSelector, ExecutionStopsOnSuccess)
{
    int counter = 0;
    auto selector = bt::Node::create<bt::Selector>();
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<bt::Success>());
    selector->addChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(selector->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 0); // Third child never executed
}

// ===========================================================================
// ReactiveSelector Tests
// ===========================================================================

TEST(TestReactiveSelector, RestartsOnEachTick)
{
    int counter = 0;
    auto selector = bt::Node::create<bt::ReactiveSelector>();
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(selector->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 1);

    EXPECT_EQ(selector->tick(), bt::Status::SUCCESS);
    EXPECT_EQ(counter, 2); // Counter incremented again
}

// ===========================================================================
// SelectorWithMemory Tests
// ===========================================================================

TEST(TestSelectorWithMemory, RemembersProgress)
{
    int counter = 0;
    auto selector = bt::Node::create<bt::SelectorWithMemory>();
    selector->addChild(bt::Node::create<bt::Failure>());
    selector->addChild(bt::Node::create<StatusAction>(bt::Status::RUNNING));
    selector->addChild(bt::Node::create<CounterAction>(&counter));

    EXPECT_EQ(selector->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 0); // Third child not reached

    EXPECT_EQ(selector->tick(), bt::Status::RUNNING);
    EXPECT_EQ(counter, 0); // Still not reached
}
